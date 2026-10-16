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
// FILE: EngineCLibrary.h
// GeneralsX @feature cemlyn007 16/10/2026 Per-engine forms of the C library functions that keep process-wide state
// (docs/EMBEDDING.md)
//
// A C library function with a hidden static buffer, position or generator shares it between every engine in the
// process, which the static storage check cannot see, since the storage is the C library's. The banned symbol check
// bans those functions in GeneralsX (rule "process-wide C library state"); GeneralsX calls their reentrant forms
// (localtime_r, asctime_r, gmtime_r, strtok_r, getaddrinfo) directly, and these where the state itself must be kept:
// each engine keeps its own, in a per-engine static (Common/EngineStatic.h). Calling one with no engine current is
// refused, as any per-engine static is.
#pragma once

#if RTS_ENGINE_CONTEXT

// The engine's headers may define min and max as macros, which the standard headers cannot be parsed with.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <netinet/in.h>

#include <vector>
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace rts
{

// rand() for the current engine. Each engine has its own generator, glibc's (rand is random: the additive feedback
// generator with 128 bytes of state), seeded with 1 until engineSrand seeds it, as a process's rand is. So an engine
// draws exactly the sequence rand() gives a fresh single-engine process, whatever other engines draw. The engine's
// threads share its generator, as a process's threads share rand's.
int engineRand();

// srand() for the current engine: reseeds its generator alone.
void engineSrand(unsigned int seed);

// strtok() for the current engine: the position a null `text` continues from is the engine's own, shared by all its
// callers as strtok's is by a process's.
char* engineStrtok(char* text, const char* separators);

// The IPv4 addresses of host `name`, as gethostbyname gives them, in a list of the caller's own (getaddrinfo), or
// none if the name does not resolve.
std::vector<in_addr> lookUpHostAddresses(const char* name);

} // namespace rts

#endif
