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
// FILE: EngineTestHooks.h
// GeneralsX @feature cemlyn007 16/10/2026 Test hooks a host's tests arm in one engine (docs/EMBEDDING.md)
//
// With RTS_ENGINE_CONTEXT a host's tests can make an engine fault, or ask to quit, from inside GeneralsX's own code,
// so that they exercise its real crash paths and catch-alls rather than a path of their own. A hook is armed in the
// current engine and strikes once, the next time that engine's code reaches the hook's point:
// - startUp:  the start of GameEngine::init's try block (so a hook armed before GameEngine::init);
// - frame:    the start of a frame, inside GameEngine::guardedUpdate's try block;
// - load:     the start of a game's load: GameLogic::startNewGame, inside the frame that starts a game, and the
//             start of GameState::loadGame's try block;
// - teardown: the start of GameEngine's destructor, inside its try block, in the teardown window
//             (Common/EngineFault.h).
// An injected fault is either GeneralsX's release crash with the given reason (as RELEASE_CRASH), or a C++
// exception of no GeneralsX type (a std::runtime_error with the reason as its message), as a library GeneralsX
// calls might throw. Either can be raised inside a catch-all at the point, `catch (...) {}`, which tries to swallow
// it, as engine code that catches everything would. A swallowed release crash still faults the engine: the sink has
// latched it (Common/EngineFault.h). A swallowed foreign exception is the engine's own business, and no fault.
// An armed quit makes the next frame ask to quit through GameEngine::setQuitting(TRUE), as the quit menu does.
//
// The hooks are per engine (a per-engine static, Common/EngineStatic.h), and go with the engine. Nothing in
// GeneralsX arms one: the banned symbol check's rule "test hooks" refuses a call to an arming function from any
// GeneralsX source.
#pragma once

#if RTS_ENGINE_CONTEXT

#include <string>

namespace rts
{

enum class TestHookPoint
{
	startUp,
	frame,
	load,
	teardown,
};

enum class InjectedFaultKind
{
	releaseCrash,
	foreignException,
};

struct InjectedFault
{
	InjectedFaultKind kind;
	// The release crash's reason, or the foreign exception's message: raw bytes, which cannot hold a NUL.
	std::string reason;
	// Whether the fault is raised inside a catch-all that tries to swallow it.
	bool caughtByCatchAll;
};

// Arms `fault` at `point` in the current engine, replacing a fault already armed there. For a host's tests only.
void armInjectedFault(TestHookPoint point, InjectedFault fault);

// Arms a request to quit at the current engine's next frame. For a host's tests only.
void armInjectedQuit();

// GeneralsX's hook points: strikes what is armed at `point` in the current engine, once.
void strikeTestHooks(TestHookPoint point);

} // namespace rts

#endif
