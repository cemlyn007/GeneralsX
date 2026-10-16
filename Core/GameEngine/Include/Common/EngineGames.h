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
// FILE: EngineGames.h
// GeneralsX @feature cemlyn007 16/10/2026 How many games an engine has started (docs/EMBEDDING.md)
//
// With RTS_ENGINE_CONTEXT, GameLogic::startNewGame counts each game the current engine starts: a skirmish, a saved
// game's load and a replay's playback alike. A host that keeps state about a game, such as the changes it made to
// the game's shroud that it can still undo, compares the count with the one it noted when it made them, and so
// knows that a new game has replaced the one they belong to. The count is per engine (a per-engine static,
// Common/EngineStatic.h), starts at zero and never decreases, and is not part of the simulation: no checksum,
// save or replay holds it.
#pragma once

#if RTS_ENGINE_CONTEXT

#include "Lib/BaseType.h"

namespace rts
{

UnsignedInt startedGames();

}

#endif
