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
// FILE: ExecutableResourceLedger.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's ResourceLedger (see ExecutableEmbedding.h): it keeps every
// resource its engine records until the layer that acquired it releases or forgets it. The process is the engine's, so
// whatever is still recorded when the process ends is released by the operating system, as upstream's resources are. It is
// never destroyed, so that a layer that releases a resource while the process exits still finds it.

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace rts
{

namespace
{

class ExecutableResourceLedger final : public ResourceLedger
{
public:
	Id record(std::string kind, std::uintptr_t handle, Release release) override
	{
		const std::scoped_lock lock(m_mutex);
		const Id id = m_nextId++;
		m_entries.push_back(Entry{id, std::move(kind), handle, std::move(release)});
		return id;
	}

	bool releaseOne(Id id) override
	{
		Entry entry;
		if (!take(id, entry))
			return false;
		try
		{
			entry.release(entry.handle);
		}
		catch (...)
		{
			return false;
		}
		return true;
	}

	bool forget(Id id) override
	{
		Entry entry;
		return take(id, entry);
	}

private:
	struct Entry
	{
		Id id = 0;
		std::string kind;
		std::uintptr_t handle = 0;
		Release release;
	};

	bool take(Id id, Entry& taken)
	{
		const std::scoped_lock lock(m_mutex);
		const auto found = std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& entry) { return entry.id == id; });
		if (found == m_entries.end())
			return false;
		taken = std::move(*found);
		m_entries.erase(found);
		return true;
	}

	std::mutex m_mutex;
	std::vector<Entry> m_entries;
	Id m_nextId = 1;
};

} // namespace

ResourceLedger* executableResourceLedger()
{
	static ExecutableResourceLedger* const ledger = new ExecutableResourceLedger;
	return ledger;
}

} // namespace rts

#endif
