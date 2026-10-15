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
// FILE: ExecutableEmbedding.h
// GeneralsX @feature cemlyn007 15/10/2026 The game executable as the host of its one engine (docs/EMBEDDING.md)
//
// With RTS_ENGINE_CONTEXT, GeneralsX reaches its services through its context's EmbeddingConfig. The executable makes its
// engine's context with this one, whose services behave as the upstream game does: the process is the engine's, so a
// fault ends it. Each service lives in its own Executable*.cpp file.
#pragma once

#if RTS_ENGINE_CONTEXT

#include "Common/EmbeddingContract.h"

namespace rts
{

// The executable's EmbeddingConfig for its one engine, its roots taken from the command line and the process.
EmbeddingConfig makeExecutableEmbedding();

FaultSink* executableFaultSink();
QuitListener* executableQuitListener();
ThreadStarter* executableThreadStarter();
// GeneralsX @feature cemlyn007 15/10/2026 Asks every thread the executable started to stop and joins it, before its
// engine's context goes.
void stopExecutableThreads() noexcept;
ResourceLedger* executableResourceLedger();
std::filesystem::path executableAssetRoot();
std::filesystem::path executableUserDataRoot();

} // namespace rts

#endif
