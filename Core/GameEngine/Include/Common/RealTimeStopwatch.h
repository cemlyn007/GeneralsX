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
// FILE: RealTimeStopwatch.h
// GeneralsX @feature cemlyn007 16/10/2026 A stopwatch over the monotonic clock, for the real-time waits GameLogic
// runs beside the simulation without being part of it: pacing the load screen while the map's objects are made, and
// giving up on network players who stop reporting their load progress.
//
// Logic code may not read a clock (design decision 9's banned symbol check, and the simulation-determinism spec), and
// nothing this measures reaches the simulation's state or its checksums. The clock is read here, outside the logic
// layer, so that the check can keep logic code clock-free; code that feeds the simulation must not use this.
#pragma once

#if RTS_ENGINE_CONTEXT

#include "Lib/BaseType.h"

#include <cstdint>

class RealTimeStopwatch
{
public:
	// A stopwatch that has never been started: it counts as long expired, as upstream's zeroed timestamps did.
	RealTimeStopwatch() = default;

	// Starts (or restarts) the stopwatch now.
	void start();
	// Returns it to never started.
	void clear() { m_started = false; }
	// The milliseconds since start(), or UINT_MAX if it has never been started.
	UnsignedInt elapsedMilliseconds() const;

private:
	bool m_started = false;
	std::int64_t m_startNanoseconds = 0;
};

#endif
