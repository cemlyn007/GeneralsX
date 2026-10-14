/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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
#include "Lib/BaseType.h"

namespace rts
{

// TheSuperHackers @feature Adds support for launching multiple game clients and keeping track of their instance id.

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
#define s_instanceIndex RTS_ENGINE_GLOBAL_REF(s_instanceIndex)
#endif
#if RTS_ENGINE_CONTEXT
#define s_isMultiInstance RTS_ENGINE_GLOBAL_REF(s_isMultiInstance)
#endif
#if RTS_ENGINE_CONTEXT
#define s_mutexHandle RTS_ENGINE_GLOBAL_REF(s_mutexHandle)
#endif
class ClientInstance
{
public:
	// Can be called N times, but is initialized just once.
	static bool initialize();

	static bool isInitialized();

	static bool isMultiInstance();

	// Change multi instance on runtime. Must be called before initialize.
	static void setMultiInstance(bool v);

	// Skips using the primary instance. Must be called before initialize.
	// Useful when the new process is not meant to collide with another normal Generals process.
	static void skipPrimaryInstance();

	// Returns the instance index of this game client. Starts at 0.
	static UnsignedInt getInstanceIndex();

	// Returns the instance id of this game client. Starts at 1.
	static UnsignedInt getInstanceId();

	// Returns the instance name of the first game client.
	static const char* getFirstInstanceName();

private:
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(HANDLE, s_mutexHandle);
#else
	static HANDLE s_mutexHandle;
#endif
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(UnsignedInt, s_instanceIndex);
#else
	static UnsignedInt s_instanceIndex;
#endif
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(Bool, s_isMultiInstance);
#else
	static Bool s_isMultiInstance;
#endif
};

} // namespace rts
#if RTS_ENGINE_CONTEXT
#undef s_instanceIndex
#if RTS_ENGINE_CONTEXT
#undef s_isMultiInstance
#endif
#if RTS_ENGINE_CONTEXT
#undef s_mutexHandle
#endif
#endif
