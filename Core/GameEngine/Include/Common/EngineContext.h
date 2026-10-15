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
// GeneralsX @feature cemlyn007 14/10/2026 The embedding contract (docs/EMBEDDING.md): a context made for an embedded
// engine keeps the host's EmbeddingConfig (Common/EmbeddingContract.h), its roots, modes and services. Statics that
// hold engine state are per-engine statics (Common/EngineStatic.h): each context holds its own copy of each, made
// on first use and destroyed with the context.
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
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

// GeneralsX @feature cemlyn007 14/10/2026 The identity of one per-engine static (Common/EngineStatic.h,
// PerEngineStatic): only its address is used, as the key of the static in each EngineContext's table. A key is
// `constexpr`, so it is read-only and holds no engine state. It points to itself so that no two keys can have the same
// contents, which a linker folding identical read-only data could otherwise merge.
struct EngineStaticKey
{
	const EngineStaticKey* self;
	const char* name;
};

// The storage of one engine's copy of a per-engine static, so that an array type and a braced initializer both work.
template <typename T>
struct EngineStaticHolder
{
	T value;
};

// How to make and destroy one engine's copy of a per-engine static: `construct` constructs it in `size` bytes of
// zero-filled storage aligned to `alignment`, using `closure`, and `destroy` destroys it in place. A static destroyed
// last (`destroyLast`, an allocator other statics' objects come from) is destroyed after every other one.
struct EngineStaticType
{
	std::size_t size;
	std::size_t alignment;
	void (*construct)(void* storage, void* closure);
	void (*destroy)(void* storage) noexcept;
	bool destroyLast;
};

struct EngineStatics;
struct EmbeddingConfig;

struct RTS_ENGINE_CONTEXT_API EngineContext
{
	EngineContext() = default;
	// GeneralsX @feature cemlyn007 14/10/2026 A context for an engine embedded by a host, which keeps `embedding`.
	explicit EngineContext(EmbeddingConfig embedding);
	// Destroys the per-engine statics in the reverse order of their creation, inside a Scope for this context.
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

	// GeneralsX @feature cemlyn007 14/10/2026 How the host embeds this engine, or null for a context made without one.
	const EmbeddingConfig* embedding() const noexcept { return m_embedding; }

	// This engine's copy of the per-engine static `key`, or null if it has not made one.
	void* findStatic(const EngineStaticKey& key) const noexcept;
	// Makes this engine's copy of the per-engine static `key` with `type` and `closure`, or returns the one another
	// thread of this engine made first. A copy whose construction throws is not kept, and the next use tries again.
	void* makeStatic(const EngineStaticKey& key, const EngineStaticType& type, void* closure);
	// Destroys every per-engine static now, inside a Scope for this context: newest first, those destroyed last after
	// every other (the engine's teardown calls it; the destructor does it for any made since). A later use makes a
	// fresh one.
	void destroyStatics();
	// The number of per-engine statics this context holds (for the lifecycle checks: g_noEngine must hold none).
	std::size_t countStatics() const;
	// Calls `visit` with the name of every per-engine static this context holds, oldest first.
	void forEachStatic(void (*visit)(const char* name, void* user), void* user) const;

	// The number of singleton fields and direct pointer fields (pathfindCellInfoArray, ...) that are not
	// null (for the lifecycle checks).
	std::size_t countLiveSingletons() const;
	// Calls `visit` with the name (`TheXxx`, or the field's name for a direct pointer field) of every such
	// field that is not null, singletons first in list order, and returns how many there were.
	std::size_t forEachLiveSingleton(void (*visit)(const char* name, void* user), void* user) const;

private:
	// Owned, and deleted by the destructor: a pointer, so that this header need not define EmbeddingConfig.
	EmbeddingConfig* m_embedding = nullptr;
	std::atomic<EngineStatics*> m_statics{nullptr};

	EngineStatics& statics();
};

// The context of "no engine": all singletons null. The current context outside every Scope.
// GeneralsX @refactor cemlyn007 15/10/2026 A constant in read-only storage, never destroyed, so nothing can write it
// (EngineContext.cpp, docs/EMBEDDING.md).
extern RTS_ENGINE_CONTEXT_API EngineContext& g_noEngine;

// The current context. Never null: &g_noEngine outside every Scope. constinit, so no access pays a
// thread_local initialisation check.
extern RTS_ENGINE_CONTEXT_API constinit thread_local EngineContext* t_engine RTS_TLS_IE;

inline EngineContext* ctx() noexcept
{
	return t_engine;
}

// GeneralsX @feature cemlyn007 14/10/2026 Whether an engine's context is current: code that may also run outside every
// engine (a static's destructor at exit) leaves per-engine statics alone when none is.
inline bool hasCurrentEngine() noexcept
{
	return t_engine != &g_noEngine;
}

// GeneralsX @feature cemlyn007 15/10/2026 The current engine's EmbeddingConfig, through which GeneralsX reaches the host's
// services (Common/EmbeddingContract.h). Throws std::logic_error naming `service` when no engine is current or the
// current one was made without an EmbeddingConfig.
RTS_ENGINE_CONTEXT_API const EmbeddingConfig& currentEmbedding(const char* service);

// True when g_noEngine still has every singleton and direct field at its initial value and no slot object
// (nothing assigned one, or used a PerEngineStatic, outside a Scope).
RTS_ENGINE_CONTEXT_API bool noEngineIsPristine();

// GeneralsX @refactor cemlyn007 14/10/2026 RTS_ENGINE_SCOPE_ENVIRONMENT: the Scope also installs the engine's
// floating-point mode and "C" LC_NUMERIC, for the CMake build's hosts (the game executable and rlgenerals' old
// launcher), which install nothing themselves. An embedding host installs that environment in its own entry gate
// (docs/EMBEDDING.md), so without it a Scope only makes its context current, and keeps no cache of its own.
#if RTS_ENGINE_SCOPE_ENVIRONMENT
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
#endif

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
#if RTS_ENGINE_SCOPE_ENVIRONMENT
		if (m_entered)
			enterEngineThreadInvariants(m_saved);
#endif
	}
	~Scope()
	{
#if RTS_ENGINE_SCOPE_ENVIRONMENT
		if (m_entered)
			leaveEngineThreadInvariants(m_saved);
#endif
		t_engine = m_previous;
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

private:
	EngineContext* m_previous;
	bool m_entered;
#if RTS_ENGINE_SCOPE_ENVIRONMENT
	ThreadInvariants m_saved; // set only when m_entered
#endif
};

// GeneralsX @feature cemlyn007 14/10/2026 The current engine's copy of the per-engine static `key`, made on its first
// use in this engine: zero-filled, then constructed by `construct(storage)`, which placement-constructs an
// EngineStaticHolder<T>. It is destroyed with the engine's context. See Common/EngineStatic.h.
template <typename T, bool DestroyLast = false, typename Construct>
T& engineStatic(const EngineStaticKey& key, Construct&& construct)
{
	EngineContext& context = *ctx();
	void* storage = context.findStatic(key);
	if (storage == nullptr)
	{
		static constexpr EngineStaticType type = {
			sizeof(EngineStaticHolder<T>),
			alignof(EngineStaticHolder<T>),
			[](void* where, void* closure) { (*static_cast<std::remove_reference_t<Construct>*>(closure))(where); },
			[](void* where) noexcept { static_cast<EngineStaticHolder<T>*>(where)->~EngineStaticHolder<T>(); },
			DestroyLast,
		};
		storage = context.makeStatic(key, type, static_cast<void*>(&construct));
	}
	return static_cast<EngineStaticHolder<T>*>(storage)->value;
}

// GeneralsX @feature cemlyn007 28/09/2026 PER_ENGINE_STATIC (PLAN-023 Phases 2-4)
// One object per engine in place of a file, class or function-local static, declared `constexpr`:
// `static constexpr rts::PerEngineStatic<T> name_perEngine{};`. The object lives in the current EngineContext,
// made on its first use in that context (value-initialised, then passed to `initialize` if one is given) and
// destroyed with it, newest first. A TU-local (or header) `#define` of the old name to `(name_perEngine.get())`
// keeps the uses unchanged. It must not be used outside every Scope (g_noEngine holds no statics).
// GeneralsX @refactor cemlyn007 14/10/2026 The PerEngineStatic is itself the key of its object in each engine's table
// (EngineStaticKey), so it is `constexpr` and read-only: it no longer holds a process-wide slot index, which made it
// writable state that every engine shared. Common/EngineStatic.h's macros declare the same kind of static in place.
template <typename T>
class PerEngineStatic
{
public:
	constexpr PerEngineStatic() noexcept : m_key{&m_key, "PerEngineStatic"}, m_initialize(nullptr) {}
	constexpr explicit PerEngineStatic(void (*initialize)(T& object)) noexcept
		: m_key{&m_key, "PerEngineStatic"}, m_initialize(initialize)
	{
	}

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
	// `static constexpr rts::PerEngineStatic<const X*> x_perEngine{};` and
	// `const X* x = x_perEngine.get([&](const X*& value) { value = find(info.name); });`, so each engine
	// runs the initialiser once, on its first call, as a solo run does.
	template <typename Initialize>
	T& get(Initialize&& initialize) const
	{
		return engineStatic<T>(m_key, [&](void* storage) {
			EngineStaticHolder<T>* const holder = ::new (storage) EngineStaticHolder<T>{};
			try
			{
				initialize(holder->value);
			}
			catch (...)
			{
				holder->~EngineStaticHolder<T>();
				throw;
			}
		});
	}

private:
	EngineStaticKey m_key;
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
// GeneralsX @refactor cemlyn007 15/10/2026 `Defaults` is a constexpr constant in read-only storage, which only reads
// reach: a write through the reference this returns for it would fault (docs/EMBEDDING.md).
template <typename S, S* EngineContext::*Pointer, const S& Defaults>
inline S& indirectContext() noexcept
{
	S* const state = ctx()->*Pointer;
	return state != nullptr ? *state : const_cast<S&>(Defaults);
}

// A stand-in for a class's static data member that moved into such a struct (member `Field`), declared as
// `static constexpr rts::IndirectContextField<S, &rts::EngineContext::p, S::Defaults, T, &S::name> name{};`.
// Like ContextField: the upstream reads, assignments, `->` and comparisons compile unchanged, but `sizeof` and
// `std::addressof` give the stand-in's size and address, not the field's (scripts/cpp/engine_context_standins.py
// fails on those), so a name used that way, or with `.`, needs a reference-returning macro instead. An array field also
// takes `[]` and decays to a pointer to its first element.
template <typename S, S* EngineContext::*Pointer, const S& Defaults, typename T, T S::*Field>
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

template <typename S, S* EngineContext::*Pointer, const S& Defaults, typename E, std::size_t N, E (S::*Field)[N]>
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

// RTS_ENGINE_STATIC and RTS_ENGINE_GLOBAL, the per-engine statics' declarations.
#include "EngineStatic.h"

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
