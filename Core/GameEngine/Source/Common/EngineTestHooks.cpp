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
// GeneralsX @feature cemlyn007 24/10/2026 Blocked engine threads (see Common/EngineTestHooks.h).

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/EngineTestHooks.h"

#if RTS_ENGINE_CONTEXT

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "Common/Debug.h"
#include "Common/EmbeddingContract.h"
#include "Common/EngineContext.h"
#include "Common/EngineThread.h"
#include "Common/GameEngine.h"

namespace
{

constexpr std::size_t kTestHookPoints = 4;

struct TestHooks
{
	std::optional<rts::InjectedFault> faults[kTestHookPoints];
	bool quitAtNextFrame;
	bool wholeTerrainDraws;
};

// This engine's armed hooks.
RTS_ENGINE_GLOBAL(TestHooks, theTestHooks);
#define theTestHooks RTS_ENGINE_GLOBAL_REF(theTestHooks)

std::size_t indexOf(rts::TestHookPoint point)
{
	return static_cast<std::size_t>(point);
}

void raiseInjectedFault(const rts::InjectedFault& fault)
{
	if (fault.kind == rts::InjectedFaultKind::foreignException)
		throw std::runtime_error(fault.reason);
	ReleaseCrash(fault.reason.c_str());
}

// How long a blocked thread's start may take before startBlockedEngineThreads gives up on it.
constexpr std::chrono::seconds kBlockedThreadStartLimit(60);
// How long the reader waits for its pipe between checks of its stop token.
constexpr std::chrono::milliseconds kReadPollInterval(50);

void awaitBlocked(std::future<void>& blocked, const char* thread)
{
	if (blocked.wait_for(kBlockedThreadStartLimit) != std::future_status::ready)
		throw std::runtime_error(std::string("the blocked engine thread ") + thread + " never reached its wait");
}

// Reads one byte from `descriptor`, waiting for it in polls of kReadPollInterval, and gives up at a stop request.
void readUnlessStopped(int descriptor, const std::stop_token& stop)
{
	while (!stop.stop_requested())
	{
		pollfd readable{descriptor, POLLIN, 0};
		const int ready = ::poll(&readable, 1, static_cast<int>(kReadPollInterval.count()));
		if (ready < 0 && errno != EINTR)
			return;
		if (ready > 0)
		{
			char byte = 0;
			if (::read(descriptor, &byte, 1) != 0)
				return;
		}
	}
}

std::vector<std::shared_ptr<rts::EngineThread>> startLockPair()
{
	const std::shared_ptr<std::mutex> lock = std::make_shared<std::mutex>();
	const std::shared_ptr<std::promise<void>> holding = std::make_shared<std::promise<void>>();
	const std::shared_ptr<std::promise<void>> waiting = std::make_shared<std::promise<void>>();
	const std::shared_ptr<std::promise<std::shared_ptr<rts::EngineThread>>> waiterStarted =
		std::make_shared<std::promise<std::shared_ptr<rts::EngineThread>>>();
	std::future<void> held = holding->get_future();
	std::future<void> waited = waiting->get_future();

	const std::shared_ptr<rts::EngineThread> holder = rts::startEngineThread("test lock holder",
		[lock, holding, waiter = waiterStarted->get_future().share()](std::stop_token stop) {
			const rts::EngineLock heldLock(*lock);
			holding->set_value();
			while (rts::sleepUnlessStopped(stop, std::chrono::hours(1)))
			{
			}
			if (const std::shared_ptr<rts::EngineThread> thread = waiter.get())
				thread->waitUntilFinished();
		});
	try
	{
		awaitBlocked(held, "test lock holder");
	}
	catch (...)
	{
		waiterStarted->set_value(nullptr);
		throw;
	}
	std::shared_ptr<rts::EngineThread> waiter;
	try
	{
		waiter = rts::startEngineThread("test lock waiter", [lock, waiting](std::stop_token) {
			waiting->set_value();
			const rts::EngineLock waitedLock(*lock);
		});
	}
	catch (...)
	{
		waiterStarted->set_value(nullptr);
		throw;
	}
	waiterStarted->set_value(waiter);
	awaitBlocked(waited, "test lock waiter");
	return {holder, waiter};
}

std::vector<std::shared_ptr<rts::EngineThread>> startDeviceReader()
{
	int ends[2] = {-1, -1};
	if (::pipe2(ends, O_CLOEXEC) != 0)
		throw std::system_error(errno, std::generic_category(), "the blocked reader's pipe");
	rts::ResourceLedger& ledger = *rts::currentEmbedding("the blocked reader's pipe").resourceLedger;
	const auto closeEnd = [](std::uintptr_t end) { ::close(static_cast<int>(end)); };
	try
	{
		ledger.record("pipe test device", static_cast<std::uintptr_t>(ends[0]), closeEnd);
	}
	catch (...)
	{
		::close(ends[0]);
		::close(ends[1]);
		throw;
	}
	try
	{
		ledger.record("pipe test device writer", static_cast<std::uintptr_t>(ends[1]), closeEnd);
	}
	catch (...)
	{
		::close(ends[1]);
		throw;
	}
	const std::shared_ptr<std::promise<void>> reading = std::make_shared<std::promise<void>>();
	std::future<void> readerBlocked = reading->get_future();
	const std::shared_ptr<rts::EngineThread> reader = rts::startEngineThread("test device reader",
		[descriptor = ends[0], reading](std::stop_token stop) {
			reading->set_value();
			readUnlessStopped(descriptor, stop);
		});
	awaitBlocked(readerBlocked, "test device reader");
	return {reader};
}

} // namespace

std::vector<std::shared_ptr<rts::EngineThread>> rts::startBlockedEngineThreads(BlockedEngineThreads threads)
{
	switch (threads)
	{
		case BlockedEngineThreads::lockPair:
			return startLockPair();
		case BlockedEngineThreads::deviceRead:
			break;
	}
	return startDeviceReader();
}

void rts::armInjectedFault(TestHookPoint point, InjectedFault fault)
{
	theTestHooks.faults[indexOf(point)] = std::move(fault);
}

void rts::armInjectedQuit()
{
	theTestHooks.quitAtNextFrame = true;
}

void rts::armWholeTerrainDraws(bool armed)
{
	theTestHooks.wholeTerrainDraws = armed;
}

bool rts::wholeTerrainDrawsArmed()
{
	return theTestHooks.wholeTerrainDraws;
}

void rts::strikeTestHooks(TestHookPoint point)
{
	TestHooks& hooks = theTestHooks;
	if (point == TestHookPoint::frame && hooks.quitAtNextFrame)
	{
		hooks.quitAtNextFrame = false;
		TheGameEngine->setQuitting(TRUE);
	}
	std::optional<InjectedFault>& armed = hooks.faults[indexOf(point)];
	if (!armed)
		return;
	const InjectedFault fault = std::move(*armed);
	armed.reset();
	if (!fault.caughtByCatchAll)
	{
		raiseInjectedFault(fault);
		return;
	}
	try
	{
		raiseInjectedFault(fault);
	}
	catch (...)
	{
	}
}

#endif
