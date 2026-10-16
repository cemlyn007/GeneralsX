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
// FILE: RealTimeStopwatch.cpp
// GeneralsX @feature cemlyn007 16/10/2026 A stopwatch over the monotonic clock (see Common/RealTimeStopwatch.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/RealTimeStopwatch.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <chrono>
#include <climits>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace
{

std::int64_t nowNanoseconds()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

} // namespace

void RealTimeStopwatch::start()
{
	m_startNanoseconds = nowNanoseconds();
	m_started = true;
}

UnsignedInt RealTimeStopwatch::elapsedMilliseconds() const
{
	if (!m_started)
		return UINT_MAX;
	const std::int64_t elapsed = (nowNanoseconds() - m_startNanoseconds) / 1000000;
	if (elapsed >= static_cast<std::int64_t>(UINT_MAX))
		return UINT_MAX;
	return static_cast<UnsignedInt>(elapsed);
}

#endif
