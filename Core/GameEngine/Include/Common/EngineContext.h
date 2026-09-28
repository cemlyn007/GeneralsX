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
#include <functional>
#include <type_traits>
#include <utility>

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
	// Destroys the slot objects in the reverse order of their creation. It does not touch the singletons:
	// the engine's own shutdown deletes those, inside a Scope for this context.
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

	// Later phases add hot per-engine state here as direct fields (the RNG seeds, the pathfinder pool,
	// the polygon triggers, ...: PLAN-023 Phases 2-3), since a field costs one load where a slot costs a
	// lookup.

	// Per-engine slot objects, by allocateEngineSlotIndex() index; null until set.
	void* getSlot(std::size_t index) const;
	// Stores a slot object that this context owns and destroys with `destroy`. The index must not be set.
	void setSlot(std::size_t index, void* object, EngineSlotDestroyFn destroy);

	// The number of singleton fields that are not null (for the lifecycle checks).
	std::size_t countLiveSingletons() const;

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

// True when g_noEngine still has every singleton null (nothing assigned one outside a Scope).
RTS_ENGINE_CONTEXT_API bool noEngineIsPristine();

// Makes `context` the current context for its lifetime, then restores the previous one. Scopes nest.
// A null context means g_noEngine.
class [[nodiscard]] Scope
{
public:
	explicit Scope(EngineContext* context) noexcept : m_previous(t_engine)
	{
		t_engine = context != nullptr ? context : &g_noEngine;
	}
	~Scope()
	{
		t_engine = m_previous;
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

private:
	EngineContext* m_previous;
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
