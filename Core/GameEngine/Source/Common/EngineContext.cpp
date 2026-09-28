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
#include <langinfo.h>
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
#endif

namespace
{
#if defined(__x86_64__) && !defined(_WIN32)
#define RTS_SCOPE_CONTROL_WORDS 1
// GeneralsX @bugfix cemlyn007 28/09/2026 On x86-64 the engine's floating-point mode is two control words, the x87 one
// and MXCSR, as setFPMode() leaves them. The same for every thread, so the first Scope learns them (after its
// setFPMode()) and later ones load them directly: fnstcw/stmxcsr to save, fldcw/ldmxcsr to set and restore, in
// place of fegetenv, setFPMode()'s fesetenv(FE_DFL_ENV) and friends, and fesetenv. A thread already in the mode
// skips even that. Neither exception flags (masked, so they change no result) nor the empty x87 stack (empty at
// every call) are part of the mode.
constexpr unsigned int kMxcsrExceptionFlags = 0x3F;
// (x87 control word << 32) | MXCSR less its exception flags, with bit 63 set once learnt.
constexpr unsigned long long kFloatingPointModeKnown = 1ull << 63;
std::atomic<unsigned long long> theEngineFloatingPointMode(0);

unsigned long long packFloatingPointMode(unsigned short controlWord, unsigned int mxcsr) noexcept
{
	return kFloatingPointModeKnown | (static_cast<unsigned long long>(controlWord) << 32) | (mxcsr & ~kMxcsrExceptionFlags);
}
#else
#define RTS_SCOPE_CONTROL_WORDS 0
#endif

#ifndef _WIN32
// Whether the thread's LC_NUMERIC reads and writes numbers as "C" does: a '.' radix and no grouping, all that
// category holds.
bool numericIsC() noexcept
{
	const char* const radix = nl_langinfo(RADIXCHAR);
	const char* const separator = nl_langinfo(THOUSEP);
	return radix != nullptr && radix[0] == '.' && radix[1] == '\0' && (separator == nullptr || separator[0] == '\0');
}
#endif
}

void enterEngineThreadInvariants(ThreadInvariants& saved) noexcept
{
	saved.floatingPointSaved = true;
#if RTS_SCOPE_CONTROL_WORDS
	__asm__ __volatile__("fnstcw %0" : "=m" (saved.x87ControlWord));
	__asm__ __volatile__("stmxcsr %0" : "=m" (saved.mxcsr));
	const unsigned long long mode = theEngineFloatingPointMode.load(std::memory_order_relaxed);
	if (mode == packFloatingPointMode(saved.x87ControlWord, saved.mxcsr))
	{
		saved.floatingPointSaved = false;
	}
	else if (mode != 0)
	{
		unsigned short controlWord = static_cast<unsigned short>(mode >> 32);
		unsigned int mxcsr = static_cast<unsigned int>(mode);
		__asm__ __volatile__("fldcw %0" : : "m" (controlWord));
		__asm__ __volatile__("ldmxcsr %0" : : "m" (mxcsr));
	}
	else
	{
		setFPMode();
		unsigned short controlWord = 0;
		unsigned int mxcsr = 0;
		__asm__ __volatile__("fnstcw %0" : "=m" (controlWord));
		__asm__ __volatile__("stmxcsr %0" : "=m" (mxcsr));
		theEngineFloatingPointMode.store(packFloatingPointMode(controlWord, mxcsr), std::memory_order_relaxed);
	}
#else
	std::fegetenv(reinterpret_cast<std::fenv_t*>(saved.floatingPointEnvironment));
	saved.x87ControlWord = 0;
	saved.mxcsr = 0;
	setFPMode();
#endif
	saved.locale = nullptr;
	saved.engineLocale = nullptr;
#ifndef _WIN32
	// The thread's own locale with LC_NUMERIC "C", made only when its LC_NUMERIC differs (never in a host that
	// leaves LC_NUMERIC alone, as Python does), and freed on exit.
	if (!numericIsC())
	{
		const locale_t base = duplocale(uselocale((locale_t)0));
		if (base != (locale_t)0)
		{
			const locale_t engine = newlocale(LC_NUMERIC_MASK, "C", base);
			if (engine != (locale_t)0)
			{
				saved.engineLocale = static_cast<void*>(engine);
				saved.locale = static_cast<void*>(uselocale(engine));
			}
			else
			{
				freelocale(base);
			}
		}
	}
#else
	// Windows has no per-thread uselocale; the engine keeps the thread's locale there.
#endif
}

void leaveEngineThreadInvariants(const ThreadInvariants& saved) noexcept
{
#ifndef _WIN32
	if (saved.engineLocale != nullptr)
	{
		uselocale(static_cast<locale_t>(saved.locale));
		freelocale(static_cast<locale_t>(saved.engineLocale));
	}
#endif
	if (!saved.floatingPointSaved)
		return;
#if RTS_SCOPE_CONTROL_WORDS
	__asm__ __volatile__("fldcw %0" : : "m" (saved.x87ControlWord));
	__asm__ __volatile__("ldmxcsr %0" : : "m" (saved.mxcsr));
#else
	std::fesetenv(reinterpret_cast<const std::fenv_t*>(saved.floatingPointEnvironment));
#endif
}

bool noEngineIsPristine()
{
	return g_noEngine.countLiveSingletons() == 0 && !g_noEngine.engineTearingDown && !g_noEngine.nameKeysFrozen
		&& g_noEngine.originalGlobalData == nullptr && !g_noEngine.wwMathInitialized && !g_noEngine.hasSlotObjects();
}

} // namespace rts

#endif // RTS_ENGINE_CONTEXT
