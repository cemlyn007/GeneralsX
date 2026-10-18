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
// FILE: InvalidInputData.h
// GeneralsX @feature cemlyn007 15/10/2026 The error GeneralsX's readers of host-supplied files throw for bad data
// (docs/EMBEDDING.md)
//
// The INI parser (for the game files in the asset root), the map loader, the save reader and the replay reader
// throw InvalidInputData when the file a host gave holds data they cannot use: `path` names the file and `reason`
// says what is wrong with it. It is an ordinary error, not a fault: no crash path runs, the FaultSink is never
// called, and every catch-all at the top of a frame, start-up, load or teardown lets it through
// (Common/EngineFault.h), so the host can catch it at the operation's boundary. Files GeneralsX writes itself
// (the map cache, SagePatch.ini) are not host input: bad data there is a fault.
#pragma once

#include <stdexcept>
#include <string>

namespace rts
{

// Default visibility, so a host built with -fvisibility=hidden can catch it across the shared-library boundary.
#if defined(__GNUC__) || defined(__clang__)
#define RTS_INVALID_INPUT_DATA_API __attribute__((visibility("default")))
#else
#define RTS_INVALID_INPUT_DATA_API
#endif

class RTS_INVALID_INPUT_DATA_API InvalidInputData : public std::runtime_error
{
public:
	InvalidInputData(std::string path, std::string reason);
	~InvalidInputData() override;

	// The file the host gave, as the reader opened it.
	std::string path;
	// What is wrong with its data.
	std::string reason;
};

} // namespace rts
