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
// FILE: EngineContext.cpp
// GeneralsX @feature cemlyn007 28/09/2026 The engine context (PLAN-023 Phase 1; see Common/EngineContext.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineContext.h"

#include <algorithm>
#include <atomic>
#include <cfenv>
#include <cstdio>
#include <mutex>
#include <vector>

#ifndef _WIN32
#include <locale.h>
#if defined(__APPLE__)
#include <xlocale.h>
#endif
#endif

// GameLogic.cpp (GameLogic/FPUControl.h): the engine's floating-point mode.
void setFPMode();

namespace rts
{

EngineContext g_noEngine;

constinit thread_local EngineContext* t_engine RTS_TLS_IE = &g_noEngine;

namespace
{
std::atomic<std::size_t> theNextSlotIndex(0);
}

std::size_t allocateEngineSlotIndex()
{
	return theNextSlotIndex.fetch_add(1);
}

struct EngineSlotTable
{
	struct Owned
	{
		std::size_t index;
		void* object;
		EngineSlotDestroyFn destroy;
	};

	std::vector<void*> byIndex;
	std::vector<Owned> inCreationOrder;
};

#ifdef DEBUG_CRASHING
namespace
{
// Appends each live field's name to a bounded, comma-separated list (for the ~EngineContext assert).
struct LiveFieldNames
{
	char text[512] = "";
	std::size_t length = 0;
};

void appendLiveFieldName(const char* name, void* user)
{
	LiveFieldNames& names = *static_cast<LiveFieldNames*>(user);
	const std::size_t room = sizeof(names.text) - names.length;
	if (room <= 1)
		return;
	const int written = std::snprintf(names.text + names.length, room, "%s%s", names.length == 0 ? "" : ", ", name);
	if (written > 0)
		names.length += std::min<std::size_t>((std::size_t)written, room - 1);
}
}
#endif

EngineContext::~EngineContext()
{
#ifdef DEBUG_CRASHING
	if (this != &g_noEngine && (countLiveSingletons() != 0 || originalGlobalData != nullptr || wwMathInitialized))
	{
		LiveFieldNames names;
		const std::size_t live = forEachLiveSingleton(&appendLiveFieldName, &names);
		DEBUG_CRASH(("EngineContext destroyed before its engine was shut down, or its shutdown left state: "
			"%u live singleton/pointer fields (%s), originalGlobalData %s, wwMathInitialized %s",
			(unsigned)live, live != 0 ? names.text : "none", originalGlobalData != nullptr ? "set" : "null",
			wwMathInitialized ? "true" : "false"));
	}
#endif
	DEBUG_ASSERTCRASH(this == &g_noEngine || noEngineIsPristine(), ("g_noEngine was written: engine state leaked outside every Scope"));

	destroySlots();
}

void EngineContext::destroySlots()
{
	if (m_slots == nullptr)
		return;

	// A slot object's destructor may read engine state, which must be this engine's.
	Scope scope(this);

	// A slot object's destructor may read another slot (or create one), so destroy them newest first and
	// clear each before its destructor runs.
	while (!m_slots->inCreationOrder.empty())
	{
		EngineSlotTable::Owned owned = m_slots->inCreationOrder.back();
		m_slots->inCreationOrder.pop_back();
		m_slots->byIndex[owned.index] = nullptr;
		if (owned.destroy != nullptr)
			owned.destroy(owned.object);
	}
	delete m_slots;
	m_slots = nullptr;
}

void* EngineContext::getSlot(std::size_t index) const
{
	if (m_slots == nullptr || index >= m_slots->byIndex.size())
		return nullptr;
	return m_slots->byIndex[index];
}

void EngineContext::setSlot(std::size_t index, void* object, EngineSlotDestroyFn destroy)
{
	DEBUG_ASSERTCRASH(this != &g_noEngine, ("EngineContext::setSlot on g_noEngine: no engine is current"));
	DEBUG_ASSERTCRASH(getSlot(index) == nullptr, ("EngineContext::setSlot: slot %u is already set", (unsigned)index));
	if (m_slots == nullptr)
		m_slots = new EngineSlotTable;
	if (index >= m_slots->byIndex.size())
		m_slots->byIndex.resize(index + 1, nullptr);
	m_slots->byIndex[index] = object;
	m_slots->inCreationOrder.push_back(EngineSlotTable::Owned{index, object, destroy});
}

bool EngineContext::hasSlotObjects() const
{
	return m_slots != nullptr && !m_slots->inCreationOrder.empty();
}

std::size_t EngineContext::countLiveSingletons() const
{
	return forEachLiveSingleton([](const char*, void*) {}, nullptr);
}

std::size_t EngineContext::forEachLiveSingleton(void (*visit)(const char* name, void* user), void* user) const
{
	std::size_t live = 0;
	// Three separate definitions: forwarding one macro to another would macro-expand the name.
#define RTS_ENGINE_SINGLETON(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#define RTS_ENGINE_SINGLETON_STRUCT(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#define RTS_ENGINE_SINGLETON_ZH(T, n) if (n##_ != nullptr) { ++live; visit(#n, user); }
#include "Common/EngineSingletons.inl"
#undef RTS_ENGINE_SINGLETON
#undef RTS_ENGINE_SINGLETON_STRUCT
#undef RTS_ENGINE_SINGLETON_ZH
	// The direct pointer fields, which the engine's teardown frees and nulls just as it does the singletons.
#define RTS_ENGINE_CONTEXT_POINTER(n) if (n != nullptr) { ++live; visit(#n, user); }
	RTS_ENGINE_CONTEXT_POINTER(pathfindCellInfoArray)
	RTS_ENGINE_CONTEXT_POINTER(pathfindCellInfoFirstFree)
	RTS_ENGINE_CONTEXT_POINTER(polygonTriggerList)
	RTS_ENGINE_CONTEXT_POINTER(mapObjectList)
	RTS_ENGINE_CONTEXT_POINTER(partitionContactList)
	RTS_ENGINE_CONTEXT_POINTER(w3dDisplay3DScene)
	RTS_ENGINE_CONTEXT_POINTER(w3dDisplay2DScene)
	RTS_ENGINE_CONTEXT_POINTER(w3dDisplay3DInterfaceScene)
	RTS_ENGINE_CONTEXT_POINTER(w3dDisplayAssetManager)
	RTS_ENGINE_CONTEXT_POINTER(ww3dAssetManager)
#undef RTS_ENGINE_CONTEXT_POINTER
	return live;
}

// GeneralsX @feature cemlyn007 28/09/2026 The per-thread invariants a Scope sets when it enters an engine (PLAN-023
// Phase 5b; see Scope).
static_assert(sizeof(std::fenv_t) <= sizeof(ThreadInvariants::floatingPointEnvironment), "fenv_t does not fit ThreadInvariants");
static_assert(alignof(std::fenv_t) <= 8, "fenv_t is over-aligned for ThreadInvariants");
#ifndef _WIN32
static_assert(sizeof(locale_t) <= sizeof(void*), "locale_t does not fit ThreadInvariants");

namespace
{
std::once_flag theEngineLocaleOnce;
locale_t theEngineLocale = (locale_t)0;

// The locale every engine runs in: the process's locale with LC_NUMERIC "C" (INI parsing uses strtod and atof),
// so the other categories (LC_CTYPE's multibyte conversions) stay as the host set them. Made once per process
// and never freed (a thread may still use it at exit).
locale_t engineLocale() noexcept
{
	std::call_once(theEngineLocaleOnce, []() {
		const locale_t base = duplocale(LC_GLOBAL_LOCALE);
		if (base != (locale_t)0)
		{
			theEngineLocale = newlocale(LC_NUMERIC_MASK, "C", base);
			if (theEngineLocale == (locale_t)0)
				freelocale(base);
		}
	});
	return theEngineLocale;
}
}
#endif

void enterEngineThreadInvariants(ThreadInvariants& saved) noexcept
{
	std::fegetenv(reinterpret_cast<std::fenv_t*>(saved.floatingPointEnvironment));
#if (defined(__i386__) || defined(__x86_64__)) && !defined(_WIN32)
	// fesetenv need not restore the x87 precision bits that setFPMode clears, so the control word is saved too.
	__asm__ __volatile__("fnstcw %0" : "=m" (saved.x87ControlWord));
#else
	saved.x87ControlWord = 0;
#endif
	setFPMode();
#ifndef _WIN32
	const locale_t locale = engineLocale();
	saved.locale = locale != (locale_t)0 ? static_cast<void*>(uselocale(locale)) : nullptr;
#else
	// Windows has no per-thread uselocale; the engine keeps the thread's locale there.
	saved.locale = nullptr;
#endif
}

void leaveEngineThreadInvariants(const ThreadInvariants& saved) noexcept
{
#ifndef _WIN32
	if (saved.locale != nullptr)
		uselocale(static_cast<locale_t>(saved.locale));
#endif
	std::fesetenv(reinterpret_cast<const std::fenv_t*>(saved.floatingPointEnvironment));
#if (defined(__i386__) || defined(__x86_64__)) && !defined(_WIN32)
	__asm__ __volatile__("fldcw %0" : : "m" (saved.x87ControlWord));
#endif
}

bool noEngineIsPristine()
{
	return g_noEngine.countLiveSingletons() == 0 && !g_noEngine.engineTearingDown && !g_noEngine.nameKeysFrozen
		&& g_noEngine.originalGlobalData == nullptr && !g_noEngine.wwMathInitialized && !g_noEngine.hasSlotObjects();
}

} // namespace rts

#endif // RTS_ENGINE_CONTEXT
