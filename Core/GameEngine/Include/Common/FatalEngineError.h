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

// FILE: FatalEngineError.h
// GeneralsX @feature cemlyn007 28/09/2026 Embedded mode: fatal errors throw instead of exiting (PLAN-023 Phase 0)
//
// By default a fatal error (ReleaseCrash / ReleaseCrashLocalized, and so RELEASE_CRASH) writes
// ReleaseCrashInfo.txt and ends the process with _exit(1). A host that embeds the engine as a
// library (rlgenerals) turns on embedded mode before it initialises the engine. From then on a
// fatal error still writes ReleaseCrashInfo.txt, but then throws FatalEngineError instead of
// exiting, so the host can abandon that engine and keep running.
//
// The engine that threw is corrupt: the host must not call into it again, and should not destroy
// it either (its destructors may run on inconsistent state). A fatal error raised while a
// destructor is running still ends the process, through std::terminate.
//
// GeneralsX @bugfix cemlyn007 28/09/2026 The teardown window (PLAN-023 Phase 1). With no TheGlobalData
// a fatal error has no crash file to write. Before the engine has made TheGlobalData (early boot) it
// still throws, but once the engine is being torn down (TheGlobalData is freed by then) it returns, as
// it did before embedded mode: a throw there would almost always escape a destructor and end the
// process through std::terminate. It also returns while another exception is already propagating.

#pragma once

#include <stdexcept>
#include <string>

// Default visibility, so a host built with -fvisibility=hidden can catch the engine's exception
// type and call the functions below across the shared-library boundary.
#if defined(__GNUC__) || defined(__clang__)
#define FATAL_ENGINE_ERROR_API __attribute__((visibility("default")))
#else
#define FATAL_ENGINE_ERROR_API
#endif

class FATAL_ENGINE_ERROR_API FatalEngineError : public std::runtime_error
{
public:
	explicit FatalEngineError(const std::string& reason);
	virtual ~FatalEngineError() override;
};

// Process-wide. Set it before the first engine is initialised; off by default.
FATAL_ENGINE_ERROR_API void SetEngineEmbeddedMode(bool embedded);
FATAL_ENGINE_ERROR_API bool IsEngineEmbeddedMode();

// GeneralsX @bugfix cemlyn007 03/10/2026 Whether a FatalEngineError was constructed in the current
// engine (with RTS_ENGINE_CONTEXT, in the current engine context, never outside every one; without it,
// in the process) since the last call, which clears it. A host that enters an engine around its calls
// reads it as the call ends, to tell an exception leaving the engine that is a fatal error from one that
// is not, before it can have caught the exception.
FATAL_ENGINE_ERROR_API bool TakeEngineFatalErrorRaised();

// Per engine (per engine context with RTS_ENGINE_CONTEXT): GameEngine's destructor sets it and its
// constructor clears it.
//
// GeneralsX @bugfix cemlyn007 28/09/2026 It must not carry over into the next engine:
// - With RTS_ENGINE_CONTEXT it is a field of the engine context, so a new engine (a new context) starts
//   clear. Outside every engine context (rts::g_noEngine) it is never set. A fatal error there with no
//   TheGlobalData (always the case there) is not thrown, though, but reported on stderr: the caller may
//   be a static destructor or an exit handler (noexcept, so a throw would end the process), which
//   cannot be told apart from a host call that entered no engine.
// - Without it there is one flag for the process, which stays set after ~GameEngine (the statics
//   destroyed at exit still see no TheGlobalData). A host that brings up another engine after a
//   teardown clears it first, SetEngineTearingDown(false), before anything that can raise a fatal error.
FATAL_ENGINE_ERROR_API void SetEngineTearingDown(bool tearingDown);
FATAL_ENGINE_ERROR_API bool IsEngineTearingDown();

// GeneralsX @bugfix cemlyn007 28/09/2026 A fatal error that never returns (PLAN-023 Phase 1). ReleaseCrash
// returns when there is no TheGlobalData and it does not throw (not embedded, the teardown window, another
// exception propagating, or outside every engine context: see above), and upstream callers carry on after
// it then. A caller that must not carry on in any case calls this instead: ReleaseCrash, and if that
// returns, FatalEngineError in embedded mode (which ends the process through std::terminate if a
// destructor is running) or abort() otherwise, with the reason on stderr.
//
// Never call it from a destructor (or anything a destructor calls, an exit handler included): in
// embedded mode it throws even where ReleaseCrash would not, and a throw out of a destructor ends the
// process through std::terminate. Such callers keep ReleaseCrash, which returns there.
[[noreturn]] FATAL_ENGINE_ERROR_API void ReleaseCrashNoReturn(const char *reason);
