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

#include <atomic>
#include <vector>

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

EngineContext::~EngineContext()
{
	if (m_slots == nullptr)
		return;

	// A slot object's destructor may read another slot (or create one), so destroy them inside this
	// context's own Scope, newest first, clearing each before its destructor runs.
	Scope scope(this);
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

bool EngineContext::hasSlots() const
{
	return m_slots != nullptr && !m_slots->inCreationOrder.empty();
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

std::size_t EngineContext::countLiveSingletons() const
{
	std::size_t live = 0;
#define RTS_ENGINE_SINGLETON(T, n) live += (n##_ != nullptr) ? 1 : 0;
#define RTS_ENGINE_SINGLETON_STRUCT(T, n) live += (n##_ != nullptr) ? 1 : 0;
#define RTS_ENGINE_SINGLETON_ZH(T, n) live += (n##_ != nullptr) ? 1 : 0;
#include "Common/EngineSingletons.inl"
#undef RTS_ENGINE_SINGLETON
#undef RTS_ENGINE_SINGLETON_STRUCT
#undef RTS_ENGINE_SINGLETON_ZH
	return live;
}

bool noEngineIsPristine()
{
	return g_noEngine.countLiveSingletons() == 0 && !g_noEngine.hasSlots() && !g_noEngine.engineTearingDown;
}

} // namespace rts

#endif // RTS_ENGINE_CONTEXT
