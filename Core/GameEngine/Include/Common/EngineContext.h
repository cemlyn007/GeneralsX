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

namespace rts
{

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

	// Set while this engine is being torn down (GameEngine's destructor onwards), cleared when a
	// GameEngine is constructed in it. Fatal errors raised in that window do not throw (see
	// FatalEngineError.h).
	bool engineTearingDown = false;

	// Set while this engine boots from names another engine primed, until its upgrades are loaded: it
	// must intern no new NameKey then (NameKeyGenerator::PrimingLatch, PLAN-023 Decision 2).
	bool nameKeysFrozen = false;

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

	// PathfindCellInfo::s_infoArray/s_firstFree: the pathfinder's cell-info pool and its free list
	// (AIPathfind.cpp), made and freed by this engine's Pathfinder.
	::PathfindCellInfo* pathfindCellInfoArray = nullptr;
	::PathfindCellInfo* pathfindCellInfoFirstFree = nullptr;

	// PolygonTrigger::ThePolygonTriggerListPtr/s_currentID: the map's trigger areas, walked on every
	// object's cell change, and the next trigger ID.
	::PolygonTrigger* polygonTriggerList = nullptr;
	std::int32_t polygonTriggerCurrentID = 1;

	// MapObject::TheMapObjectListPtr: the map objects of the last map this engine read.
	::MapObject* mapObjectList = nullptr;

	// PartitionManager.cpp's TheContactList (the contact list of the partition update in progress) and
	// getClosestObjects()'s iteration stamp (nonzero).
	::PartitionContactList* partitionContactList = nullptr;
	std::int32_t partitionIterFlag = 1;

	// ScriptList::m_curId: the last script ID handed out.
	std::int32_t scriptListCurId = 0;

	// REPLAY_CRC_INTERVAL (Recorder.cpp): the logic CRC interval of a solo game or replay.
	std::int32_t replayCrcInterval = 100;

	// W3DDisplay's scenes and asset manager (W3DDisplay::m_3DScene, ...; PLAN-023 Phase 3). Headless
	// builds them too, and each engine's GameClient::reset resets its own.
	::RTS3DScene* w3dDisplay3DScene = nullptr;
	::RTS2DScene* w3dDisplay2DScene = nullptr;
	::RTS3DInterfaceScene* w3dDisplay3DInterfaceScene = nullptr;
	::W3DAssetManager* w3dDisplayAssetManager = nullptr;

	// WW3DAssetManager::TheInstance: the asset manager the W3D loaders use (bones and meshes, headless
	// too); the same object as w3dDisplayAssetManager once W3DDisplay has made it.
	::WW3DAssetManager* ww3dAssetManager = nullptr;

	// WW3D's timing statics (WW3D::SyncTime, ...), with their upstream initial values: the animation clock
	// (bones are posed headless too), advanced by this engine's frames only. 1000.0f / WWSyncPerSecond.
	float ww3dLogicFrameTimeMs = 1000.0f / 30;
	float ww3dFractionalSyncMs = 0.0f;
	unsigned int ww3dSyncTime = 0;
	unsigned int ww3dPreviousSyncTime = 0;
	int ww3dFrameCount = 0;

	// Whether this engine holds one of WWMath's Init counts (wwmath.cpp): set by its first WWMath::Init and
	// cleared by its first WWMath::Shutdown after that, so an engine that calls Shutdown twice (W3DDisplay's
	// failed init, then its destructor) releases only its own count, never another live engine's.
	bool wwMathInitialized = false;

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

// True when g_noEngine still has every singleton null and no slot object (nothing assigned one, or used a
// PerEngineStatic, outside a Scope).
RTS_ENGINE_CONTEXT_API bool noEngineIsPristine();

// GeneralsX @feature cemlyn007 28/09/2026 The per-thread state an engine relies on (PLAN-023 Phase 5b): the calling
// thread's floating-point environment (with the x87 control word) and its locale, saved by a Scope that enters
// an engine. Opaque here, so that this force-included header pulls in neither <cfenv> nor <locale.h>;
// EngineContext.cpp checks that fenv_t and locale_t fit.
struct ThreadInvariants
{
	alignas(8) unsigned char floatingPointEnvironment[32];
	void* locale;
	unsigned short x87ControlWord;
};

// Saves the calling thread's invariants into `saved`, then sets the engine's: setFPMode()'s rounding and
// precision, and the engine's locale (made once per process: LC_NUMERIC "C", the rest as the process has it)
// for this thread.
RTS_ENGINE_CONTEXT_API void enterEngineThreadInvariants(ThreadInvariants& saved) noexcept;
// Restores what enterEngineThreadInvariants saved.
RTS_ENGINE_CONTEXT_API void leaveEngineThreadInvariants(const ThreadInvariants& saved) noexcept;

// Makes `context` the current context for its lifetime, then restores the previous one. Scopes nest.
// A null context means g_noEngine.
//
// GeneralsX @feature cemlyn007 28/09/2026 A Scope that switches the thread to a different engine also sets the
// engine's per-thread invariants (PLAN-023 Phase 5b): the floating-point mode setFPMode() sets, which applies
// to the calling thread only, and LC_NUMERIC "C" (the other locale categories stay the host's). The engine calls
// setFPMode() at boot, at INI and map loads and around every GameLogic::update, so the Scope sets it at entry
// for the rest of a call (client update, host reads). An engine may be stepped on another thread than it
// booted on, or on one whose mode or number format the host changed, and must still run as it would alone. The thread's own mode and locale come back when the Scope ends. A nested
// Scope on the engine already current, and a Scope for g_noEngine, do nothing more than before, so the cost is
// paid once per outermost call into an engine.
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
// index, so it is process-wide and written once, at static initialisation. A TU-local (or header) `#define`
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
		EngineContext* context = ctx();
		void* object = context->getSlot(m_index);
		if (object == nullptr)
		{
			Holder* holder = new Holder();
			if (m_initialize != nullptr)
				m_initialize(holder->value);
			context->setSlot(m_index, holder, &destroy);
			object = holder;
		}
		return static_cast<Holder*>(object)->value;
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

// GeneralsX @feature cemlyn007 28/09/2026 A stand-in for a class's static data member whose value is a
// direct EngineContext field (PLAN-023 Phase 2), for statics that are also used qualified
// (`MapObject::TheMapObjectListPtr`), where a macro cannot stand in: declare it as
// `static constexpr rts::ContextField<T, &rts::EngineContext::field> name{};` and the upstream reads,
// assignments, `->` and comparisons compile unchanged. Taking its address gives the stand-in's, not the
// field's, so a static whose address is taken needs a macro instead.
template <typename T, T EngineContext::*Field>
struct ContextField
{
	operator T&() const noexcept
	{
		return ctx()->*Field;
	}
	T operator->() const noexcept
	{
		return ctx()->*Field;
	}
	const ContextField& operator=(T value) const noexcept
	{
		ctx()->*Field = value;
		return *this;
	}
	T& operator++() const noexcept
	{
		return ++(ctx()->*Field);
	}
	T operator++(int) const noexcept
	{
		return (ctx()->*Field)++;
	}
	// Compound assignment needs members: a built-in `+=` takes no user-defined conversion of its left operand.
	template <typename U>
	T& operator+=(const U& value) const noexcept
	{
		return ctx()->*Field += value;
	}
	template <typename U>
	T& operator-=(const U& value) const noexcept
	{
		return ctx()->*Field -= value;
	}
};

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
