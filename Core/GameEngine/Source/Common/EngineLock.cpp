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
// FILE: EngineLock.cpp
// GeneralsX @feature cemlyn007 24/10/2026 The thread layer's stop-aware waits (see Common/EngineThread.h): the current
// engine thread's stop token, sleepUnlessStopped, EngineLock and EngineCondition. They need nothing else of the engine,
// so code built on its own with the per-engine statics (scripts/cpp/engine_context_unit_test.py) links them too.

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineThread.h"

namespace rts
{

namespace
{

// How long an engine thread's EngineLock waits before it tries a held mutex again.
constexpr std::chrono::milliseconds contendedLockRetry(1);

// The stop token of the engine thread running on this thread, while its body runs.
constinit thread_local const std::stop_token* t_engineThreadStop = nullptr;

} // namespace

CurrentEngineThreadStop::CurrentEngineThreadStop(const std::stop_token& stop) noexcept : m_previous(t_engineThreadStop)
{
	t_engineThreadStop = &stop;
}

CurrentEngineThreadStop::~CurrentEngineThreadStop()
{
	t_engineThreadStop = m_previous;
}

bool sleepUnlessStopped(const std::stop_token& stop, std::chrono::milliseconds duration)
{
	std::mutex mutex;
	std::condition_variable_any wake;
	std::unique_lock<std::mutex> lock(mutex);
	wake.wait_for(lock, stop, duration, [] { return false; });
	return !stop.stop_requested();
}

const char* EngineThreadStopped::what() const noexcept
{
	return "the engine thread was asked to stop while it waited";
}

std::stop_token currentEngineThreadStop() noexcept
{
	return t_engineThreadStop != nullptr ? *t_engineThreadStop : std::stop_token();
}

EngineLock::EngineLock(std::mutex& mutex) : m_mutex(mutex)
{
	lock();
}

EngineLock::~EngineLock()
{
	unlock();
}

void EngineLock::lock()
{
	const std::stop_token* const stop = t_engineThreadStop;
	if (stop == nullptr)
	{
		m_mutex.lock();
		m_owned = true;
		return;
	}
	while (!m_mutex.try_lock())
	{
		if (!sleepUnlessStopped(*stop, contendedLockRetry))
			throw EngineThreadStopped();
	}
	m_owned = true;
}

void EngineLock::lockToFinish()
{
	m_mutex.lock();
	m_owned = true;
}

void EngineLock::unlock() noexcept
{
	if (!m_owned)
		return;
	m_owned = false;
	m_mutex.unlock();
}

void EngineCondition::notifyAll() noexcept
{
	m_changed.notify_all();
}

void EngineCondition::wait(EngineLock& lock, const std::function<bool()>& ready)
{
	const std::stop_token* const stop = t_engineThreadStop;
	if (stop == nullptr)
	{
		m_changed.wait(lock, ready);
		return;
	}
	if (!m_changed.wait(lock, *stop, ready))
		throw EngineThreadStopped();
}

} // namespace rts

#endif // RTS_ENGINE_CONTEXT
