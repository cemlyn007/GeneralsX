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
// FILE: ExecutableEmbedding.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's EmbeddingConfig (see ExecutableEmbedding.h)

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

#include <dlfcn.h>

namespace rts
{

// GeneralsX @feature cemlyn007 16/10/2026 The executable holds a reference of its own to the D3D8 library for the
// process's life, as an embedding host holds one while any render engine lives, so the engine closing its own
// reference at shutdown (dx8ledger.h) never unloads DXVK under the process's other code.
void holdExecutableD3D8Library()
{
#if defined(__APPLE__)
	static void* const reference = dlopen("libdxvk_d3d8.dylib", RTLD_LAZY);
#else
	static void* const reference = dlopen("libdxvk_d3d8.so", RTLD_LAZY);
#endif
	(void)reference;
}

EmbeddingConfig makeExecutableEmbedding()
{
	holdExecutableD3D8Library();
	EmbeddingConfig config;
	config.assetRoot = executableAssetRoot();
	config.userDataRoot = executableUserDataRoot();
	config.render = RenderMode::window;
	config.audio = AudioMode::on;
	config.faultSink = executableFaultSink();
	config.quitListener = executableQuitListener();
	config.threadStarter = executableThreadStarter();
	config.resourceLedger = executableResourceLedger();
	config.decisionSink = nullptr;
	return config;
}

} // namespace rts

#endif
