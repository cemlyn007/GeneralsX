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
// With RTS_ENGINE_CONTEXT a host's tests can make an engine act from inside GeneralsX's own code, so that they
// exercise its real paths rather than a path of their own. A hook is armed in the current engine and strikes once,
// the next time that engine's code reaches the hook's point:
// - frame: the start of a frame, inside GameEngine::guardedUpdate's try block.
// An armed quit makes the next frame ask to quit through GameEngine::setQuitting(TRUE), as the quit menu does.
//
// The hooks are per engine (a per-engine static, Common/EngineStatic.h), and go with the engine. Nothing in
// GeneralsX arms one: the banned symbol check's rule "test hooks" refuses a call to an arming function from any
// GeneralsX source.
#pragma once

#if RTS_ENGINE_CONTEXT

namespace rts
{

enum class TestHookPoint
{
	frame,
};

// Arms a request to quit at the current engine's next frame. For a host's tests only.
void armInjectedQuit();

// GeneralsX's hook points: strikes what is armed at `point` in the current engine, once.
void strikeTestHooks(TestHookPoint point);

} // namespace rts

#endif
