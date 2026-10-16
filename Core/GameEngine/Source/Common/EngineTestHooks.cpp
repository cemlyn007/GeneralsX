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

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include "Common/Debug.h"
#include "Common/EngineContext.h"
#include "Common/GameEngine.h"

namespace
{

constexpr std::size_t kTestHookPoints = 4;

struct TestHooks
{
	std::optional<rts::InjectedFault> faults[kTestHookPoints];
	bool quitAtNextFrame;
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

} // namespace

void rts::armInjectedFault(TestHookPoint point, InjectedFault fault)
{
	theTestHooks.faults[indexOf(point)] = std::move(fault);
}

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
