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
// FILE: EngineFault.h
// GeneralsX @feature cemlyn007 15/10/2026 How an embedded engine's crash paths end (docs/EMBEDDING.md)
//
// With RTS_ENGINE_CONTEXT every crash path ends in the current engine's FaultSink (Common/EmbeddingContract.h):
// the release crash (ReleaseCrash, ReleaseCrashLocalized, ReleaseCrashNoReturn and so RELEASE_CRASH), the
// assertion crash (AssertionCrash, which DebugCrash's and WWDebug_Assert_Fail's Abort reach), and the
// catch-alls at the top of a frame (GameEngine::guardedUpdate), start-up (GameEngine::init), a save load
// (GameState::loadGame) and teardown (GameEngine's destructor), which turn an uncaught exception or a
// thrown error code into a fault. Each:
// 1. writes the crash report, upstream's ReleaseCrashInfo.txt (the one before kept as ReleaseCrashInfoPrev.txt),
//    into the user data root of the context's EmbeddingConfig;
// 2. marks the engine's fault raised (faultRaised below);
// 3. calls FaultSink::fault with the reason, raw bytes kept exactly. The sink latches the fault and throws the
//    host's unwind exception, which leaves the engine's code.
// With no embedded engine current (rts::currentEmbedding) a crash path throws std::logic_error instead.
//
// The teardown window. From the start of GameEngine's destructor, or while another exception is propagating,
// the unwind exception would leave a destructor, which ends the process through std::terminate. There a crash
// path stops the unwind where it called the sink and returns to its caller instead (the host's latch is set
// all the same), and the teardown stops at the end of the step that faulted. ReleaseCrashNoReturn does not
// return even there, so it must not be called from a destructor.
//
// A second fault in an engine whose fault is raised writes no report: the host keeps the first reason.
#pragma once

#if RTS_ENGINE_CONTEXT

namespace rts
{

// True once a crash path has raised the current engine's fault. A catch (...) that would swallow the sink's unwind
// rethrows when this is true (rethrowIfFaultRaised), so the fault leaves the engine with its own reason.
bool faultRaised();

// Inside a handler only: rethrows the exception being handled if the engine's fault is raised.
inline void rethrowIfFaultRaised()
{
	if (faultRaised())
		throw;
}

// Opens the current engine's teardown window (above): GameEngine's destructor calls it first.
void beginEngineTeardown();

// Whether a fault raised now could not unwind: the teardown window, or another exception propagating.
bool faultCannotUnwind();

// Inside a handler only, as a catch-all at the top of a frame, start-up, load or teardown. Rethrows the sink's
// unwind (the engine's fault is raised), and otherwise raises a fault for the exception being handled, with
// `uncaughtReason` as its reason: the message of an INIException that has one in its place, and an error code or
// a std::exception's message appended to it. Returns only in the teardown window.
void faultFromCaughtException(const char* uncaughtReason);

} // namespace rts

#endif
