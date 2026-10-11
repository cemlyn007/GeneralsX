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
	explicit FatalEngineError(const std::string& reason);
	virtual ~FatalEngineError() override;
};

// Process-wide. Set it before the first engine is initialised; off by default.
FATAL_ENGINE_ERROR_API void SetEngineEmbeddedMode(bool embedded);
FATAL_ENGINE_ERROR_API bool IsEngineEmbeddedMode();

// GeneralsX @bugfix cemlyn007 03/10/2026 The engine's own record of a fatal error (with RTS_ENGINE_CONTEXT, in
// the current engine context, never outside every one; without it, in the process). Two kinds, each cleared
// by its read:
// - TakeEngineFatalErrorThrown: a FatalEngineError was constructed since the last call. It says nothing
//   about which exception is leaving the engine: this one may have been swallowed earlier. A host that
//   enters an engine around its calls reads it as the call ends, before it can have caught that exception.
// - TakeEngineFatalErrorRaised: either kind, that or a fatal error that returned instead of throwing (see the
//   teardown window above), which is no FatalEngineError and so says nothing about the exception leaving the
//   engine, if there is one. It clears both.
FATAL_ENGINE_ERROR_API bool TakeEngineFatalErrorThrown();
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

// GeneralsX @feature cemlyn007 02/10/2026 Sticky fault latch: fault delivery to the host otherwise
// depends on every catch (...) between RELEASE_CRASH and the host having a
// catch (const FatalEngineError&) { throw; } in front of it. The engine sets this latch before it
// throws, so something that swallows the exception anyway (a catch (...) added later, upstream or
// in a merge) still leaves it set. Host-usage refusals and the repeat of a poisoned process (NameKeyGenerator's refuseInit) deliberately
// do not set it: no engine faulted. It is also set when the teardown window above makes the error
// return instead of throw. A host that polls HasEngineFaulted() after each call into the engine can
// therefore detect the fault even when no exception reaches it.
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
