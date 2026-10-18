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
// FILE: EngineTestHooks.cpp
// GeneralsX @feature cemlyn007 16/10/2026 See Common/EngineTestHooks.h.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/EngineTestHooks.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineContext.h"
#include "Common/GameEngine.h"

namespace
{

struct TestHooks
{
	bool quitAtNextFrame;
};

// This engine's armed hooks.
RTS_ENGINE_GLOBAL(TestHooks, theTestHooks);
#define theTestHooks RTS_ENGINE_GLOBAL_REF(theTestHooks)

} // namespace

void rts::armInjectedQuit()
{
	theTestHooks.quitAtNextFrame = true;
}

void rts::strikeTestHooks(TestHookPoint point)
{
	TestHooks& hooks = theTestHooks;
	if (point == TestHookPoint::frame && hooks.quitAtNextFrame)
	{
		hooks.quitAtNextFrame = false;
		TheGameEngine->setQuitting(TRUE);
	}
}

#endif
