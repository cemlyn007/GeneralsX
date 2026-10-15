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

namespace rts
{

EmbeddingConfig makeExecutableEmbedding()
{
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
