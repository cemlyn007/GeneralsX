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
// FILE: dx8ledger.cpp
// GeneralsX @feature cemlyn007 16/10/2026 The device-wrapper layer's record of the D3D8 COM objects it holds (see
// dx8ledger.h).

#include "dx8ledger.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EmbeddingContract.h"
#include "Common/EngineContext.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace DX8Ledger
{

namespace
{

void Release_Recorded_Reference(std::uintptr_t handle)
{
	reinterpret_cast<IUnknown*>(handle)->Release();
}

// The ledger ids of the references this engine's device-wrapper layer holds, by object, newest last. Destroyed after
// the engine's other per-engine statics, so that a reference one of their destructors releases is still found.
struct RecordedReferences
{
	std::mutex mutex;
	rts::ResourceLedger* ledger = nullptr;
	std::unordered_map<IUnknown*, std::vector<rts::ResourceLedger::Id>> ids;
	unsigned count = 0;
};

RecordedReferences& Recorded_References()
{
	RTS_ENGINE_STATIC_LAST(RecordedReferences, references);
	return references;
}

rts::ResourceLedger& Engine_Ledger()
{
	const rts::EmbeddingConfig& embedding = rts::currentEmbedding("the render device's resource ledger");
	if (embedding.render == rts::RenderMode::none)
		throw std::logic_error("an engine with no rendering makes no render device, so it records no D3D8 object");
	return *embedding.resourceLedger;
}

} // namespace

void Record(IUnknown* object, const char* kind)
{
	try
	{
		rts::ResourceLedger& ledger = Engine_Ledger();
		const rts::ResourceLedger::Id id =
			ledger.record(kind, reinterpret_cast<std::uintptr_t>(object), &Release_Recorded_Reference);
		RecordedReferences& references = Recorded_References();
		const std::scoped_lock lock(references.mutex);
		references.ledger = &ledger;
		references.ids[object].push_back(id);
		++references.count;
	}
	catch (...)
	{
		object->Release();
		throw;
	}
}

ULONG Add_Ref(IUnknown* object, const char* kind)
{
	const ULONG count = object->AddRef();
	Record(object, kind);
	return count;
}

ULONG Release(IUnknown* object)
{
	RecordedReferences& references = Recorded_References();
	rts::ResourceLedger* ledger = nullptr;
	rts::ResourceLedger::Id id = 0;
	{
		const std::scoped_lock lock(references.mutex);
		const auto found = references.ids.find(object);
		if (found != references.ids.end())
		{
			ledger = references.ledger;
			id = found->second.back();
			found->second.pop_back();
			if (found->second.empty())
				references.ids.erase(found);
			--references.count;
		}
	}
	if (ledger != nullptr)
		ledger->forget(id);
	const ULONG count = object->Release();
	if (count == 0 && ledger != nullptr)
	{
		std::vector<rts::ResourceLedger::Id> stale;
		{
			const std::scoped_lock lock(references.mutex);
			const auto found = references.ids.find(object);
			if (found != references.ids.end())
			{
				stale = std::move(found->second);
				references.ids.erase(found);
				references.count -= static_cast<unsigned>(stale.size());
			}
		}
		if (!stale.empty())
		{
			// A reference was released past its record, so the object is gone while records of it remain:
			// drop them, so that the ledger does not release a freed object.
			std::fprintf(stderr, "ERROR: DX8Ledger::Release - %u recorded reference(s) to a released D3D8 object\n",
				static_cast<unsigned>(stale.size()));
			for (const rts::ResourceLedger::Id staleId : stale)
				ledger->forget(staleId);
		}
	}
	return count;
}

unsigned Recorded_Count(IUnknown* object)
{
	RecordedReferences& references = Recorded_References();
	const std::scoped_lock lock(references.mutex);
	const auto found = references.ids.find(object);
	return found == references.ids.end() ? 0u : static_cast<unsigned>(found->second.size());
}

unsigned Recorded_Count()
{
	RecordedReferences& references = Recorded_References();
	const std::scoped_lock lock(references.mutex);
	return references.count;
}

} // namespace DX8Ledger

#endif
