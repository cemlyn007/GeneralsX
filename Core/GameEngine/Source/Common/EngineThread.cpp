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
// FILE: EngineThread.cpp
// GeneralsX @feature cemlyn007 15/10/2026 GeneralsX's thread layer (see Common/EngineThread.h)
// GeneralsX @feature cemlyn007 24/10/2026 Stop-aware locks and conditions for engine threads live in EngineLock.cpp, which
// needs nothing else of the engine (see Common/EngineThread.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineThread.h"

#include <utility>
#include <vector>

namespace rts
{

namespace
{

// The threads one engine started, so that its teardown can stop them.
struct EngineThreadList
{
	std::mutex mutex;
	std::vector<std::shared_ptr<EngineThread>> threads;
};

} // namespace

RTS_ENGINE_GLOBAL(EngineThreadList, s_engineThreads);
#define s_engineThreads RTS_ENGINE_GLOBAL_REF(s_engineThreads)

// The body the host's ThreadStarter runs: shared by every copy of the std::function the host holds, and destroyed with
// the last one. A body the host drops without running counts as finished, so that nothing waits for it.
class EngineThreadRun
{
public:
	EngineThreadRun(std::shared_ptr<EngineThread> thread, EngineThreadBody body)
		: m_thread(std::move(thread)), m_body(std::move(body))
	{
	}
	~EngineThreadRun() { m_thread->finish(); }
	EngineThreadRun(const EngineThreadRun&) = delete;
	EngineThreadRun& operator=(const EngineThreadRun&) = delete;

	void operator()(const std::stop_token& hostStop)
	{
		EngineThread& thread = *m_thread;
		const std::stop_callback forwardHostStop(hostStop, [&thread] { thread.requestStop(); });
		struct FinishOnExit
		{
			EngineThread& thread;
			~FinishOnExit() { thread.finish(); }
		} finishOnExit{thread};
		if (thread.stopRequested())
			return;
		const std::stop_token stop = thread.m_stop.get_token();
		const CurrentEngineThreadStop current(stop);
		try
		{
			m_body(stop);
		}
		catch (const EngineThreadStopped&)
		{
		}
	}

private:
	std::shared_ptr<EngineThread> m_thread;
	EngineThreadBody m_body;
};

EngineThread::EngineThread(std::string name) : m_name(std::move(name)) {}

void EngineThread::requestStop() noexcept
{
	m_stop.request_stop();
}

bool EngineThread::stopRequested() const noexcept
{
	return m_stop.stop_requested();
}

bool EngineThread::finished() const
{
	const std::lock_guard<std::mutex> lock(m_mutex);
	return m_finished;
}

void EngineThread::waitUntilFinished() const
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_finishedChanged.wait(lock, [this] { return m_finished; });
}

void EngineThread::recordIn(ResourceLedger& ledger, ResourceLedger::Id id) noexcept
{
	const std::lock_guard<std::mutex> lock(m_mutex);
	m_ledger = &ledger;
	m_ledgerId = id;
}

// Drops the thread's ledger record before it counts as finished: a ledger releasing the record waits for that, so
// the ledger is still there when the record is dropped.
void EngineThread::finish() noexcept
{
	ResourceLedger* ledger = nullptr;
	ResourceLedger::Id ledgerId = 0;
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		if (m_finished)
			return;
		ledger = std::exchange(m_ledger, nullptr);
		ledgerId = m_ledgerId;
	}
	if (ledger != nullptr)
	{
		try
		{
			ledger->forget(ledgerId);
		}
		catch (...)
		{
		}
	}
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_finished = true;
	}
	m_finishedChanged.notify_all();
}

std::shared_ptr<EngineThread> startEngineThread(std::string name, EngineThreadBody body)
{
	const EmbeddingConfig& embedding = currentEmbedding("ThreadStarter");
	std::shared_ptr<EngineThread> thread = std::make_shared<EngineThread>(name);
	ResourceLedger& ledger = *embedding.resourceLedger;
	const ResourceLedger::Id ledgerId = ledger.record("thread " + name, reinterpret_cast<std::uintptr_t>(thread.get()),
		[thread](std::uintptr_t) {
			thread->requestStop();
			thread->waitUntilFinished();
		});
	thread->recordIn(ledger, ledgerId);
	{
		EngineThreadList& list = s_engineThreads;
		const std::lock_guard<std::mutex> lock(list.mutex);
		std::erase_if(list.threads, [](const std::shared_ptr<EngineThread>& started) { return started->finished(); });
		list.threads.push_back(thread);
	}
	const std::shared_ptr<EngineThreadRun> run = std::make_shared<EngineThreadRun>(thread, std::move(body));
	embedding.threadStarter->startThread(std::move(name), [run](std::stop_token stop) { (*run)(stop); });
	return thread;
}

std::shared_ptr<EngineThread> startEngineLoop(std::string name, std::chrono::milliseconds interval, EngineThreadStep step)
{
	return startEngineThread(std::move(name), [interval, step = std::move(step)](std::stop_token stop) {
		while (!stop.stop_requested() && step(stop) && sleepUnlessStopped(stop, interval))
		{
		}
	});
}

void stopEngineThreads()
{
	std::vector<std::shared_ptr<EngineThread>> threads;
	{
		EngineThreadList& list = s_engineThreads;
		const std::lock_guard<std::mutex> lock(list.mutex);
		threads.swap(list.threads);
	}
	for (const std::shared_ptr<EngineThread>& thread : threads)
		thread->requestStop();
	for (const std::shared_ptr<EngineThread>& thread : threads)
		thread->waitUntilFinished();
}

} // namespace rts

#endif
