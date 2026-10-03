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
// GeneralsX @bugfix cemlyn007 03/10/2026 The teardown window (PLAN-023 Phase 1). Once the engine is
// being torn down, or while another exception is already propagating, a fatal error writes the crash
// file (if TheGlobalData exists) and then returns to its caller instead of throwing: a throw there
// would almost always escape a destructor and end the process through std::terminate. Upstream
// returns only once TheGlobalData is gone and otherwise exits the process, so with TheGlobalData
// alive this is new behaviour: the caller carries on past the fatal error. The sticky fault latch
// below is still set. Outside that window, with no TheGlobalData it throws without a crash file.
// The teardown flag is cleared only by the next GameEngine(): a host that boots again in the same
// process (or context) must call SetEngineTearingDown(false) before its startup parse, or fatal
// errors in that parse return as well.

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
	explicit FatalEngineError(const std::string& reason) : std::runtime_error(reason) {}
	virtual ~FatalEngineError() override;
};

// Process-wide. Set it before the first engine is initialised; off by default.
FATAL_ENGINE_ERROR_API void SetEngineEmbeddedMode(bool embedded);
FATAL_ENGINE_ERROR_API bool IsEngineEmbeddedMode();

// Per engine (per engine context with RTS_ENGINE_CONTEXT): GameEngine's destructor sets it and its
// constructor clears it. Outside every engine context it reads as set (nothing to hand an error to).
FATAL_ENGINE_ERROR_API void SetEngineTearingDown(bool tearingDown);
FATAL_ENGINE_ERROR_API bool IsEngineTearingDown();

// GeneralsX @feature cemlyn007 02/10/2026 Sticky fault latch: fault delivery to the host otherwise
// depends on every catch (...) between RELEASE_CRASH and the host having a
// catch (const FatalEngineError&) { throw; } in front of it. Something that swallows the
// exception anyway (a catch (...) added later, upstream or in a merge) still sets this latch
// first, even when the error is returned instead of thrown (the teardown window above), so a host that polls HasEngineFaulted() after each call into the engine can detect the
// fault even when the exception itself never reaches it.
//
// The flag is process-wide, like IsEngineEmbeddedMode() above, and nothing in this engine ever
// clears it: the engine that set it is corrupt and the host must not call back into it to ask.
// Clearing it is therefore entirely the host's job, and the host must do it as soon as it has
// recorded a fault, not only when it found one by polling. A host that catches FatalEngineError
// directly (for example at a boot-time try/catch) and does not also call ClearEngineFault() there
// leaves the latch set, so the very next poll -- after an unrelated, successful call, to this
// engine or, worse, to a different one -- misreports a fault that was already handled. A host
// with more than one EngineContext alive must poll this latch after every entry into every
// engine (it cannot yet tell which engine set it) and must clear it at every point it handles a
// FatalEngineError, caught or polled, for the same reason: this one flag cannot distinguish "no
// engine has faulted since the last clear" from "an engine faulted and the host already dealt
// with it". A host that truly needs to tell those two apart per engine, or that cannot guarantee
// it clears the latch at every catch site, needs a per-EngineContext latch instead of this
// process-wide one.
FATAL_ENGINE_ERROR_API bool HasEngineFaulted();
FATAL_ENGINE_ERROR_API void ClearEngineFault();
