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
// FILE: EngineThread.h
// GeneralsX @feature cemlyn007 15/10/2026 GeneralsX's thread layer: every thread an engine runs is started here, through
// its host's ThreadStarter (Common/EmbeddingContract.h, docs/EMBEDDING.md).
//
// The work is split between the two sides of the embedding contract:
// - The host's ThreadStarter creates the operating-system thread, makes the engine's context current on it, and stops
//   and joins it when the engine closes or faults. No GeneralsX source creates a thread itself (the banned symbol
//   check).
// - This layer wraps the body the host runs. The wrapper gives the body a stop token that a stop request from either
//   side stops: the host's, the engine's own teardown (stopEngineThreads) or the ResourceLedger's. It checks for a stop
//   request before the body runs and, for a loop (startEngineLoop), between every step, so stopping a thread never
//   depends on the game objects it works on. And it records the thread in the engine's ResourceLedger while the body
//   runs, so that releasing the ledger after a fault stops the thread and waits for its body to return.
//
// A body that waits (a sleep, a condition wait) waits with sleepUnlessStopped or a stop-aware condition wait, so that
// a stop request wakes it. A body blocked in a call nothing can interrupt (a host name lookup) returns when that call
// does; it checks the stop request then, before it touches engine state.
#pragma once

#if RTS_ENGINE_CONTEXT

// The engine's headers may define min and max as macros, which the standard thread headers cannot be parsed with.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Common/EmbeddingContract.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace rts
{

using EngineThreadBody = std::function<void(std::stop_token)>;
// One step of a loop: false ends the loop.
using EngineThreadStep = std::function<bool(std::stop_token)>;

class EngineThread;
std::shared_ptr<EngineThread> startEngineThread(std::string name, EngineThreadBody body);

// One thread an engine started through this layer: its body's stop request and whether the body has returned.
class EngineThread
{
public:
	explicit EngineThread(std::string name);
	EngineThread(const EngineThread&) = delete;
	EngineThread& operator=(const EngineThread&) = delete;

	const std::string& name() const noexcept { return m_name; }
	// Asks the body to return, without waiting for it.
	void requestStop() noexcept;
	bool stopRequested() const noexcept;
	// Whether the body has returned, or will never run.
	bool finished() const;
	// Waits until the body has returned. A body must not wait for its own thread.
	void waitUntilFinished() const;

private:
	friend class EngineThreadRun;
	friend std::shared_ptr<EngineThread> startEngineThread(std::string name, EngineThreadBody body);

	void recordIn(ResourceLedger& ledger, ResourceLedger::Id id) noexcept;
	void finish() noexcept;

	std::string m_name;
	std::stop_source m_stop;
	mutable std::mutex m_mutex;
	mutable std::condition_variable m_finishedChanged;
	bool m_finished = false;
	ResourceLedger* m_ledger = nullptr;
	ResourceLedger::Id m_ledgerId = 0;
};

// Starts a thread named `name` for the current engine, through its ThreadStarter, that runs `body` with the engine's
// context current. `body` returns when its stop token is stopped. Throws std::logic_error when no embedded engine is
// current.
std::shared_ptr<EngineThread> startEngineThread(std::string name, EngineThreadBody body);

// As startEngineThread, for a thread that polls: the wrapper calls `step` until it returns false or a stop is
// requested, sleeping `interval` between calls, and wakes from that sleep at a stop request.
std::shared_ptr<EngineThread> startEngineLoop(std::string name, std::chrono::milliseconds interval, EngineThreadStep step);

// Sleeps for `duration` unless `stop` is stopped first. True if it slept the whole duration.
bool sleepUnlessStopped(const std::stop_token& stop, std::chrono::milliseconds duration);

// Asks every thread the current engine started to stop and waits until each body has returned: the engine's teardown
// calls it before it frees the state those threads use. The host still joins the threads.
void stopEngineThreads();

} // namespace rts

#endif
