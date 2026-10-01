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
// FILE: W3DDrawClock.h
// GeneralsX @feature cemlyn007 01/10/2026 The millisecond clock of the draw path's timed effects (PLAN-023 Phase 8,
// stage RR3)
//
// The letterbox fade, the water's sky scroll and the water tracks' update timed themselves by the wall clock
// (timeGetTime), so an image drawn for an embedding host's observations (render-headless: GlobalData's m_headless and
// m_headlessRender) depended on how fast the machine drew, not on the game, and two engines drawing at once read one
// clock between them. For such an engine the clock is WW3D's sync time, its animation clock, which a render-headless
// engine advances only with its own logic frames (W3DDisplay::draw, RR0c): logic-frame time in milliseconds. Every
// other engine keeps the wall clock, as upstream. A macro, so that it needs timeGetTime declared only where it is used.

#pragma once

#include "Common/GlobalData.h"
#include "WW3D2/ww3d.h"

// Whether the current engine's draw path times its effects by logic frames (see above).
inline Bool W3DDrawClockFollowsLogic()
{
	return TheGlobalData != nullptr && TheGlobalData->m_headless && TheGlobalData->m_headlessRender;
}

// The draw path's clock in milliseconds: WW3D::Get_Sync_Time() for a render-headless engine, timeGetTime() otherwise.
#define W3D_DRAW_CLOCK_MS() (W3DDrawClockFollowsLogic() ? (UnsignedInt)WW3D::Get_Sync_Time() : (UnsignedInt)timeGetTime())
