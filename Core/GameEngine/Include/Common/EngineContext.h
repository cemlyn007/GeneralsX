/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// FILE: EngineContext.h
// GeneralsX @feature cemlyn007 28/09/2026 One engine's singletons, reached through a current-context pointer
// (PLAN-023 Decision 1 and Phase 1)
//
// With the CMake option RTS_ENGINE_CONTEXT on, every per-engine singleton (Common/EngineSingletons.inl)
// lives in an rts::EngineContext instead of a global, and `TheXxx` is a macro that reads the current
// context's field: `#define TheGameLogic (::rts::ctx()->TheGameLogic_)`. No use site changes. The current
// context is one thread_local pointer (initial-exec TLS on ELF), so an access costs about what a global
// access through the GOT costs today.
//
// The build force-includes this header into every engine translation unit (and a host must include it,
// or force-include it, into every TU that touches an engine header). With RTS_ENGINE_CONTEXT off (the
// default, upstream-shaped build) it only declares rts::withCurrentEngine, which then returns its argument.
//
// Rules:
// - A host owns one EngineContext per engine and enters it with an rts::Scope around every call into that
//   engine (boot, update, shutdown and every read of engine state). Scopes nest and restore the previous
//   context, and they are per call, not per thread, so an engine is not tied to the thread that made it.
// - Outside any Scope the current context is rts::g_noEngine, whose singletons are all null. Code that
//   runs with no engine (static initialisation, threads started outside every Scope, static destructors
//   after the host has left its Scope) therefore sees "no engine", never another engine's state. Nothing
//   may assign a singleton there: that would leak into every thread and every later engine.
// - The current context is per thread, so a thread the engine starts carries its creator's context
//   explicitly: WWLib's ThreadClass captures it in Execute(), and every other thread function that reads
//   engine state is wrapped in rts::withCurrentEngine(...) where the thread is created (std::thread).
//   Such a thread must end before its engine's EngineContext is destroyed, just as it must not outlive
//   the singletons it reads. A new thread start that reads engine state needs the same wrapper; without
//   it the thread sees g_noEngine and its first singleton access dereferences null.
// - Process-global state (the critical sections, the memory manager, the NameKey generator, TheVersion,
//   debug globals, constant tables) stays in ordinary globals; see EngineSingletons.inl.
#pragma once

#if defined(__cplusplus) && defined(RTS_ENGINE_CONTEXT) && RTS_ENGINE_CONTEXT

#if __cplusplus < 202002L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#error "RTS_ENGINE_CONTEXT requires C++20 (constinit)"
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include "RandomValueSeeds.h"

// initial-exec TLS: one 8-byte pointer in the static TLS block, read with a single %fs-relative load and no
// __tls_get_addr call. ELF only; Mach-O has only the TLV model, and Windows its own implicit TLS.
#if defined(__ELF__)
#define RTS_TLS_IE __attribute__((tls_model("initial-exec")))
#else
#define RTS_TLS_IE
#endif

// Default visibility, so a host built with -fvisibility=hidden links the context pointer and g_noEngine
// across the shared-library boundary.
#if defined(__GNUC__) || defined(__clang__)
#define RTS_ENGINE_CONTEXT_API __attribute__((visibility("default")))
#else
#define RTS_ENGINE_CONTEXT_API
#endif

// The singletons' types, forward-declared at global scope.
#define RTS_ENGINE_SINGLETON(T, n) class T;
#define RTS_ENGINE_SINGLETON_STRUCT(T, n) struct T;
#define RTS_ENGINE_SINGLETON_ZH(T, n) class T;
#include "EngineSingletons.inl"
#undef RTS_ENGINE_SINGLETON
#undef RTS_ENGINE_SINGLETON_STRUCT
#undef RTS_ENGINE_SINGLETON_ZH
// The types of the direct per-engine fields below.
class PathfindCellInfo;
class PolygonTrigger;
class MapObject;
class PartitionContactList;
class RTS3DScene;
class RTS2DScene;
class RTS3DInterfaceScene;
class W3DAssetManager;
class WW3DAssetManager;
struct W3DRenderState;
struct WW3DState;
struct SDL_Window;

namespace rts
{

// GeneralsX @feature cemlyn007 30/09/2026 The alignment, and so the size granule, of the render states an engine's
// w3dRender and ww3dState point to (W3DRenderState, WW3DState): the largest memory page size the target can run with
// (16 KiB on Apple silicon; 4 KiB on x86 and x86-64; 64 KiB on every other target, whose kernels may be built with
// 4, 16 or 64 KiB pages), so that their defaults fill whole pages of their own, which a host can make
// read-only (W3D_Protect_Render_Defaults; PLAN-023 Phase 8, stage RR2a-2).
// GeneralsX @bugfix cemlyn007 30/09/2026 64 KiB wherever a larger page is possible, where 4 KiB made every protect
// fail on a 16 or 64 KiB page kernel (PLAN-023 Phase 8, stage RR2b).
#if defined(__APPLE__)
inline constexpr std::size_t renderStateAlignment = 16384;
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
inline constexpr std::size_t renderStateAlignment = 4096;
#else
inline constexpr std::size_t renderStateAlignment = 65536;
#endif

// Destroys one per-engine slot object (see EngineContext::setSlot).
typedef void (*EngineSlotDestroyFn)(void* object);

// Hands out a process-wide index for one per-engine slot (a function-local or file static that must be
// per engine, PLAN-023 Phase 4's PER_ENGINE_STATIC). Thread-safe; an index is never reused.
RTS_ENGINE_CONTEXT_API std::size_t allocateEngineSlotIndex();

struct EngineSlotTable;

struct RTS_ENGINE_CONTEXT_API EngineContext
{
	EngineContext() = default;
	// Destroys the slot objects in the reverse order of their creation, inside a Scope for this context.
	// It does not touch the singletons: the engine's own shutdown deletes and nulls those, inside a
	// Scope for this context, before the context is destroyed. Debug builds assert that it did (every
	// singleton null) and that nothing wrote g_noEngine. A context whose engine faulted is never
	// destroyed: its state is corrupt.
	~EngineContext();

	EngineContext(const EngineContext&) = delete;
	EngineContext& operator=(const EngineContext&) = delete;

	// Every per-engine singleton: `TheXxx_`, reached through the `TheXxx` macro.
#define RTS_ENGINE_SINGLETON(T, n) ::T* n##_ = nullptr;
#define RTS_ENGINE_SINGLETON_STRUCT(T, n) ::T* n##_ = nullptr;
#define RTS_ENGINE_SINGLETON_ZH(T, n) ::T* n##_ = nullptr;
#include "EngineSingletons.inl"
#undef RTS_ENGINE_SINGLETON
#undef RTS_ENGINE_SINGLETON_STRUCT
#undef RTS_ENGINE_SINGLETON_ZH

	// The direct fields that are not seeds (the engine's teardown flags, the pathfinder pool, the map and
	// partition lists, W3D's scenes, asset managers and clocks, ...), from EngineContextFields.inl.
#define RTS_ENGINE_CONTEXT_POINTER(T, n) ::T* n = nullptr;
#define RTS_ENGINE_CONTEXT_VALUE(T, n, init) T n = init;
#include "EngineContextFields.inl"
#undef RTS_ENGINE_CONTEXT_POINTER
#undef RTS_ENGINE_CONTEXT_VALUE

	// GlobalData::m_theOriginal, this engine's GlobalData with no overrides (Zero Hour; see GlobalData.h).
	// GlobalData nulls it when that instance is deleted.
	::GlobalData* originalGlobalData = nullptr;

	// Hot per-engine state lives here as direct fields, since a field costs one load where a slot costs a
	// lookup (PLAN-023 Phases 2-3).

	// RandomValue.cpp's seeds (theGameAudioSeed, ...), with their upstream initial values.
	std::uint32_t gameAudioSeed[6] = RTS_RANDOM_SEED_INITIAL_VALUES;
	std::uint32_t gameClientSeed[6] = RTS_RANDOM_SEED_INITIAL_VALUES;
	std::uint32_t gameLogicSeed[6] = RTS_RANDOM_SEED_INITIAL_VALUES;
	std::uint32_t gameLogicBaseSeed = 0;

	// Per-engine slot objects, by allocateEngineSlotIndex() index; null until set.
	void* getSlot(std::size_t index) const;
	// Stores a slot object that this context owns and destroys with `destroy`. The index must not be set.
	void setSlot(std::size_t index, void* object, EngineSlotDestroyFn destroy);
	// Destroys every slot object now, newest first, inside a Scope for this context (the engine's teardown
	// calls it; the destructor does it for any made since). A later get() makes a fresh object.
	void destroySlots();
	// Whether this context holds any slot object (for the lifecycle checks: g_noEngine must hold none).
	bool hasSlotObjects() const;

	// The number of singleton fields and direct pointer fields (pathfindCellInfoArray, ...) that are not
	// null (for the lifecycle checks).
	std::size_t countLiveSingletons() const;
	// Calls `visit` with the name (`TheXxx`, or the field's name for a direct pointer field) of every such
	// field that is not null, singletons first in list order, and returns how many there were.
	std::size_t forEachLiveSingleton(void (*visit)(const char* name, void* user), void* user) const;

private:
	EngineSlotTable* m_slots = nullptr;
};

// The context of "no engine": all singletons null. The current context outside every Scope.
extern RTS_ENGINE_CONTEXT_API EngineContext g_noEngine;

// The current context. Never null: &g_noEngine outside every Scope. constinit, so no access pays a
// thread_local initialisation check.
extern RTS_ENGINE_CONTEXT_API constinit thread_local EngineContext* t_engine RTS_TLS_IE;

inline EngineContext* ctx() noexcept
{
	return t_engine;
}

// True when g_noEngine still has every singleton and direct field at its initial value and no slot object
// (nothing assigned one, or used a PerEngineStatic, outside a Scope).
RTS_ENGINE_CONTEXT_API bool noEngineIsPristine();

// GeneralsX @feature cemlyn007 28/09/2026 The per-thread state an engine relies on (PLAN-023 Phase 5b): the calling
// thread's floating-point environment (with the x87 control word) and its locale, saved by a Scope that enters
// an engine. Opaque here, so that this force-included header pulls in neither <cfenv> nor <locale.h>;
// EngineContext.cpp checks that fenv_t and locale_t fit.
// GeneralsX @bugfix cemlyn007 28/09/2026 Only what the Scope changes is saved and restored. On x86-64 (not Windows) that
// is the x87 control word and MXCSR, reloaded on exit for a thread already in the engine's mode only if the Scope
// changed them; elsewhere the whole fenv_t (with the x87 control word on i386). Only LC_NUMERIC is switched (to "C"), so that the engine sees
// the thread's other categories (LC_CTYPE for towlower, iswspace, mbstowcs) as it does without the engine context.
// GeneralsX @bugfix cemlyn007 29/09/2026 The locale is switched on every entry, to one cached per thread (the
// thread's own locale with LC_NUMERIC "C": made at the first entry for a thread on the global locale, at every
// outermost entry for one on a locale of its own), never left as the process's global locale:
// see enterEngineThreadInvariants. The host must still not call setlocale() while any engine steps (glibc's
// locale functions are not safe against it), and must set the process locale before the first entry: a thread on
// the global locale keeps the LC_CTYPE it had at its first entry, so every thread that steps engines needs the
// same LC_CTYPE.
struct ThreadInvariants
{
	alignas(8) unsigned char floatingPointEnvironment[32];
	// The thread's locale to restore (null when the Scope left the thread's locale alone), and the one made for
	// this Scope only (freed on exit; null when it used the thread's cached one).
	void* locale;
	void* engineLocale;
	unsigned int mxcsr;
	unsigned short x87ControlWord;
	// Whether the thread entered outside the engine's mode, so the fields above hold its own state to reload on
	// exit. When false they hold the engine's mode as at entry, reloaded on exit only if something inside the
	// Scope changed it.
	bool floatingPointSaved;
};

// Saves the calling thread's invariants into `saved` where they differ from the engine's, then sets the engine's:
// setFPMode()'s rounding and precision, and LC_NUMERIC "C" over the thread's own locale.
RTS_ENGINE_CONTEXT_API void enterEngineThreadInvariants(ThreadInvariants& saved) noexcept;
// Restores what enterEngineThreadInvariants saved.
RTS_ENGINE_CONTEXT_API void leaveEngineThreadInvariants(const ThreadInvariants& saved) noexcept;

// Makes `context` the current context for its lifetime, then restores the previous one. Scopes nest.
// A null context means g_noEngine.
//
// GeneralsX @feature cemlyn007 28/09/2026 A Scope that switches the thread to a different engine also sets the
// engine's per-thread invariants (PLAN-023 Phase 5b): the floating-point mode setFPMode() sets, which the engine's
// boot, INI and load-screen paths set only on the thread that ran them, and a "C" LC_NUMERIC for its number
// parsing and formatting. An engine may be stepped on another thread than it booted on, or on one whose mode
// the host changed, and must still run as it would alone, given the host's locale contract above. The thread's
// own mode and locale come back when the Scope ends. A nested Scope on the engine already current, and a Scope
// for g_noEngine, do nothing more than before, so the cost is paid once per outermost call into an engine.
class [[nodiscard]] Scope
{
public:
	explicit Scope(EngineContext* context) noexcept : m_previous(t_engine)
	{
		EngineContext* const next = context != nullptr ? context : &g_noEngine;
		m_entered = next != m_previous && next != &g_noEngine;
		t_engine = next;
		if (m_entered)
			enterEngineThreadInvariants(m_saved);
	}
	~Scope()
	{
		if (m_entered)
			leaveEngineThreadInvariants(m_saved);
		t_engine = m_previous;
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

private:
	EngineContext* m_previous;
	bool m_entered;
	ThreadInvariants m_saved; // set only when m_entered
};

// GeneralsX @feature cemlyn007 28/09/2026 PER_ENGINE_STATIC (PLAN-023 Phases 2-4)
// One object per engine in place of a file, class or function-local static: the object lives in a slot of
// the current EngineContext, made on first use in that context (value-initialised, then passed to
// `initialize` if one is given) and destroyed with it, newest first. The static itself only holds the slot
// index, process-wide: taken at static initialisation for a file or class static, or at the first call (under
// the static-init guard) for a function-local one. A TU-local (or header) `#define`
// of the old name to `(name_perEngine.get())` keeps the uses unchanged. An access costs a slot lookup (a
// call), so hot state belongs in direct EngineContext fields instead. It must not be used outside every
// Scope (g_noEngine owns no slots).
template <typename T>
class PerEngineStatic
{
public:
	PerEngineStatic() : m_index(allocateEngineSlotIndex()), m_initialize(nullptr) {}
	explicit PerEngineStatic(void (*initialize)(T& object)) : m_index(allocateEngineSlotIndex()), m_initialize(initialize) {}

	PerEngineStatic(const PerEngineStatic&) = delete;
	PerEngineStatic& operator=(const PerEngineStatic&) = delete;

	T& get() const
	{
		return get([this](T& value) {
			if (m_initialize != nullptr)
				m_initialize(value);
		});
	}

	// GeneralsX @feature cemlyn007 28/09/2026 As get(), but the object is made with `initialize(object)`, which
	// may capture (PLAN-023 Phase 4): for a function-local static whose initialiser reads the function's
	// locals, `static const X* x = find(info.name);` becomes
	// `static rts::PerEngineStatic<const X*> x_perEngine;` and
	// `const X* x = x_perEngine.get([&](const X*& value) { value = find(info.name); });`, so each engine
	// runs the initialiser once, on its first call, as a solo run does.
	template <typename Initialize>
	T& get(Initialize&& initialize) const
	{
		EngineContext* context = ctx();
		void* object = context->getSlot(m_index);
		if (object == nullptr)
		{
			Holder* holder = new Holder();
			try
			{
				initialize(holder->value);
			}
			catch (...)
			{
				delete holder;
				throw;
			}
			context->setSlot(m_index, holder, &destroy);
			object = holder;
		}
		return static_cast<Holder*>(object)->value;
	}

private:
	// A struct, so that T may be an array.
	struct Holder
	{
		T value{};
	};

	static void destroy(void* object)
	{
		delete static_cast<Holder*>(object);
	}

	std::size_t m_index;
	void (*m_initialize)(T& object);
};

// GeneralsX @refactor cemlyn007 02/10/2026 ContextField and IndirectContextField are the same family (one
// operator set over a `T&` the stand-in locates differently): factor that set into this CRTP base, which calls
// `Derived::get()`, so a later operator (PLAN-023 Phase 8, stage RR2a-1) is added once for both instead
// of being copied and risking the two kinds diverging.
//
// GeneralsX @fix cemlyn007 02/10/2026 The deleted copy constructor and unary operator& turn two of the gate's
// (scripts/cpp/engine_context_standins.py) regex checks into compiler diagnostics instead: copying a stand-in
// (`auto x = C::name;`, or passing it by value to a variadic logger, which must copy it to place it in `...`)
// now fails with "use of deleted function", and so does `&name` (operator&). `sizeof(name)` and
// `std::addressof(name)` still compile, so the gate keeps checking those.
template <typename Derived, typename T>
struct ContextFieldOps
{
	constexpr ContextFieldOps() noexcept = default;
	ContextFieldOps(const ContextFieldOps&) = delete;
	ContextFieldOps& operator=(const ContextFieldOps&) = delete;
	Derived* operator&() const = delete;

	operator T&() const noexcept
	{
		return Derived::get();
	}
	// An explicit cast to an enum type (`(SomeEnum)name`, for an integer field) casts the field's value.
	template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && std::is_integral_v<T>>>
	explicit operator E() const noexcept
	{
		return static_cast<E>(Derived::get());
	}
	T operator->() const noexcept
	{
		return Derived::get();
	}
	const Derived& operator=(T value) const noexcept
	{
		Derived::get() = value;
		return static_cast<const Derived&>(*this);
	}
	T& operator++() const noexcept
	{
		return ++Derived::get();
	}
	T operator++(int) const noexcept
	{
		return Derived::get()++;
	}
	// Compound assignment needs members: a built-in `+=` takes no user-defined conversion of its left operand.
	template <typename U>
	T& operator+=(const U& value) const noexcept
	{
		return Derived::get() += value;
	}
	template <typename U>
	T& operator-=(const U& value) const noexcept
	{
		return Derived::get() -= value;
	}
};

// GeneralsX @feature cemlyn007 28/09/2026 A stand-in for a class's static data member whose value is a
// direct EngineContext field (PLAN-023 Phase 2), for statics that are also used qualified
// (`MapObject::TheMapObjectListPtr`), where a macro cannot stand in: declare it as
// `static constexpr rts::ContextField<T, &rts::EngineContext::field> name{};` and the upstream reads,
// assignments, `->` and comparisons compile unchanged. Its copy constructor and unary `operator&` are deleted
// (see ContextFieldOps), but `sizeof` and `std::addressof` give the stand-in's, not the field's, so a static
// used with either needs a macro instead.
template <typename T, T EngineContext::*Field>
struct ContextField : ContextFieldOps<ContextField<T, Field>, T>
{
	// A derived class's own operator=, even the implicitly-declared copy assignment, hides every base
	// class operator= by name; without this, `field = value` stops finding the base's and falls back to
	// (and fails to match) the implicit one, which takes only a `const ContextField&`.
	using ContextFieldOps<ContextField<T, Field>, T>::operator=;

	static T& get() noexcept
	{
		return ctx()->*Field;
	}
};

// GeneralsX @feature cemlyn007 30/09/2026 ContextField's indirect variant (PLAN-023 Phase 8, stage RR2a-1): state
// that a direct EngineContext field points to (`S* EngineContext::*Pointer`, allocated only for some engines),
// read through that pointer, or through `Defaults` (an S with every field at its default) while it is null.
template <typename S, S* EngineContext::*Pointer, S& Defaults>
inline S& indirectContext() noexcept
{
	S* const state = ctx()->*Pointer;
	return state != nullptr ? *state : Defaults;
}

// A stand-in for a class's static data member that moved into such a struct (member `Field`), declared as
// `static constexpr rts::IndirectContextField<S, &rts::EngineContext::p, S::Defaults, T, &S::name> name{};`.
// Like ContextField: the upstream reads, assignments, `->` and comparisons compile unchanged, but `sizeof` and
// `std::addressof` give the stand-in's size and address, not the field's (scripts/cpp/engine_context_standins.py
// fails on those), so a name used that way, or with `.`, needs a reference-returning macro instead. An array field also
// takes `[]` and decays to a pointer to its first element.
template <typename S, S* EngineContext::*Pointer, S& Defaults, typename T, T S::*Field>
struct IndirectContextField
	: ContextFieldOps<IndirectContextField<S, Pointer, Defaults, T, Field>, T>
{
	// See ContextField's same `using`: without it the implicit copy assignment hides the base's operator=.
	using ContextFieldOps<IndirectContextField<S, Pointer, Defaults, T, Field>, T>::operator=;

	static T& get() noexcept
	{
		return indirectContext<S, Pointer, Defaults>().*Field;
	}
};

template <typename S, S* EngineContext::*Pointer, S& Defaults, typename E, std::size_t N, E (S::*Field)[N]>
struct IndirectContextField<S, Pointer, Defaults, E[N], Field>
{
	// Same deleted copy/operator& backstop as ContextFieldOps (this specialisation does not derive from
	// it: an array field decays to E* rather than converting to E&, so it needs its own operator[]/
	// operator E*() instead of ContextFieldOps's operator T&()/operator->()).
	constexpr IndirectContextField() noexcept = default;
	IndirectContextField(const IndirectContextField&) = delete;
	IndirectContextField& operator=(const IndirectContextField&) = delete;
	IndirectContextField* operator&() const = delete;

	static E (&get() noexcept)[N]
	{
		return indirectContext<S, Pointer, Defaults>().*Field;
	}
	operator E*() const noexcept
	{
		return get();
	}
	E& operator[](std::size_t index) const noexcept
	{
		return get()[index];
	}
};

// True when a stand-in type cannot be copied or have its address taken with unary `&`: the compile-time half
// of what scripts/cpp/engine_context_standins.py checks. A static_assert on it where a stand-in is declared
// keeps those two deleted operations from being dropped.
template <typename Field, typename = void>
struct HasUnaryAddressOf : std::false_type
{
};
template <typename Field>
struct HasUnaryAddressOf<Field, std::void_t<decltype(&std::declval<const Field&>())>> : std::true_type
{
};
template <typename Field>
inline constexpr bool isStandInGuarded =
	!std::is_copy_constructible_v<std::remove_cv_t<Field>> && !HasUnaryAddressOf<std::remove_cv_t<Field>>::value;

// A callable that runs `function` inside a Scope for the context that was current when it was made. For
// thread functions: see withCurrentEngine.
template <typename Function>
class BoundToEngine
{
public:
	BoundToEngine(EngineContext* context, Function function) : m_context(context), m_function(std::move(function)) {}

	template <typename... Args>
	decltype(auto) operator()(Args&&... args)
	{
		Scope scope(m_context);
		return std::invoke(m_function, std::forward<Args>(args)...);
	}

private:
	EngineContext* m_context;
	Function m_function;
};

// Wraps a thread function so that it runs in the engine context current where the thread is created:
// `std::thread(rts::withCurrentEngine([this]() { ... }))`. A function rather than a macro, because some
// of the wrapped lambdas hold preprocessor directives.
template <typename Function>
BoundToEngine<std::decay_t<Function>> withCurrentEngine(Function&& function)
{
	return BoundToEngine<std::decay_t<Function>>(ctx(), std::forward<Function>(function));
}

} // namespace rts

// `#define TheXxx (::rts::ctx()->TheXxx_)` for every entry, generated from EngineSingletons.inl.
#include "EngineSingletonMacros.h"

#else // !(__cplusplus && RTS_ENGINE_CONTEXT)

#if defined(__cplusplus)
#include <utility>

namespace rts
{
// Off: the thread function itself, forwarded unchanged (see the RTS_ENGINE_CONTEXT version above).
template <typename Function>
constexpr Function&& withCurrentEngine(Function&& function) noexcept
{
	return std::forward<Function>(function);
}
} // namespace rts
#endif

#endif // __cplusplus && RTS_ENGINE_CONTEXT
