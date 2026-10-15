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
// FILE: ExecutableThreadStarter.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's ThreadStarter (see ExecutableEmbedding.h)
//
// It starts each thread its engine asks for with the engine's context current, owns the thread, and at shutdown
// (stopExecutableThreads) asks every thread to stop and joins it. GeneralsX's thread layer (Common/EngineThread.h)
// records each thread in the engine's ResourceLedger, the executable's (ExecutableResourceLedger.cpp), and gives the
// body the stop token this starter stops.

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

#include "Common/EngineContext.h"

#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include <pthread.h>

namespace rts
{

namespace
{

class ExecutableThreadStarter final : public ThreadStarter
{
public:
	ExecutableThreadStarter() = default;
	ExecutableThreadStarter(const ExecutableThreadStarter&) = delete;
	ExecutableThreadStarter& operator=(const ExecutableThreadStarter&) = delete;
	~ExecutableThreadStarter() override { stopAll(); }

	void startThread(std::string name, ThreadBody body) override
	{
		EngineContext* const context = ctx();
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_threads.emplace_back([context, name = std::move(name), body = std::move(body)](std::stop_token stop) {
			pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
			const Scope scope(context);
			body(std::move(stop));
		});
	}

	void stopAll() noexcept
	{
		std::vector<std::jthread> threads;
		{
			const std::lock_guard<std::mutex> lock(m_mutex);
			threads.swap(m_threads);
		}
		for (std::jthread& thread : threads)
			thread.request_stop();
		for (std::jthread& thread : threads)
		{
			if (thread.joinable())
				thread.join();
		}
	}

private:
	std::mutex m_mutex;
	std::vector<std::jthread> m_threads;
};

ExecutableThreadStarter& theExecutableThreadStarter()
{
	static ExecutableThreadStarter starter;
	return starter;
}

} // namespace

ThreadStarter* executableThreadStarter()
{
	return &theExecutableThreadStarter();
}

void stopExecutableThreads() noexcept
{
	theExecutableThreadStarter().stopAll();
}

} // namespace rts

#endif
