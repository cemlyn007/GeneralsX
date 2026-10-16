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
// FILE: EngineCLibrary.cpp
// GeneralsX @feature cemlyn007 16/10/2026 Per-engine forms of the C library functions that keep process-wide state
// (see Common/EngineCLibrary.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineCLibrary.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <netdb.h>
#include <sys/socket.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace rts
{

namespace
{

// glibc's rand state: random()'s TYPE_3 generator, which a process's rand() starts as if from initstate(1, state,
// 128). random_data points into `state`, so the generator is made in place and never copied.
struct EngineRandomGenerator
{
	EngineRandomGenerator()
	{
		std::memset(&data, 0, sizeof(data));
		initstate_r(1, state, sizeof(state), &data);
	}
	EngineRandomGenerator(const EngineRandomGenerator&) = delete;
	EngineRandomGenerator& operator=(const EngineRandomGenerator&) = delete;

	std::mutex mutex;
	random_data data;
	char state[128];
};

EngineRandomGenerator& engineRandomGenerator()
{
	RTS_ENGINE_STATIC(EngineRandomGenerator, generator);
	return generator;
}

} // namespace

int engineRand()
{
	EngineRandomGenerator& generator = engineRandomGenerator();
	const std::scoped_lock lock(generator.mutex);
	int32_t value = 0;
	random_r(&generator.data, &value);
	return value;
}

void engineSrand(unsigned int seed)
{
	EngineRandomGenerator& generator = engineRandomGenerator();
	const std::scoped_lock lock(generator.mutex);
	srandom_r(seed, &generator.data);
}

char* engineStrtok(char* text, const char* separators)
{
	RTS_ENGINE_STATIC(char*, position, nullptr);
	return strtok_r(text, separators, &position);
}

std::vector<in_addr> lookUpHostAddresses(const char* name)
{
	std::vector<in_addr> addresses;
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* results = nullptr;
	if (name == nullptr || getaddrinfo(name, nullptr, &hints, &results) != 0)
		return addresses;
	for (const addrinfo* result = results; result != nullptr; result = result->ai_next)
	{
		const in_addr address = reinterpret_cast<const sockaddr_in*>(result->ai_addr)->sin_addr;
		const bool seen = std::any_of(addresses.begin(), addresses.end(),
			[&](const in_addr& other) { return other.s_addr == address.s_addr; });
		if (!seen)
			addresses.push_back(address);
	}
	freeaddrinfo(results);
	return addresses;
}

} // namespace rts

#endif
