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
// FILE: EngineFiles.h
// GeneralsX @feature cemlyn007 15/10/2026 The engine's file layer: the one place GeneralsX opens files, rooted at the
// current engine's directories and recorded in its ResourceLedger (Common/EmbeddingContract.h, docs/EMBEDDING.md).
//
// With RTS_ENGINE_CONTEXT, GeneralsX never uses the process's working directory. Every path it forms itself lies under
// the current engine's asset root or its user data root (EmbeddingConfig::assetRoot, ::userDataRoot):
// - a relative path names a file under the asset root, as it named one under the working directory upstream, which the
//   game executable was started in its game directory for;
// - an absolute path is kept as it is: one under the user data root (GlobalData::getPath_UserData()), or one the host
//   handed the engine (a replay to play back).
// The asset root is read-only: a write (or a removal) anywhere but under the user data root is refused, with errno
// EACCES, and nothing is created.
//
// Each file this layer opens is recorded in the engine's ResourceLedger as it is opened and forgotten as this layer
// closes it, so that the host can close whatever a faulted engine still holds without running the engine's code.
// A file opened here must be closed here (closeEngineFile), never with fclose. Calling any of these with no embedded
// engine current throws std::logic_error (rts::currentEmbedding).
#pragma once

#if RTS_ENGINE_CONTEXT

#include <cstdio>
#include <filesystem>

namespace rts
{

// The current engine's asset root and user data root, as its EmbeddingConfig names them.
std::filesystem::path engineAssetRoot();
std::filesystem::path engineUserDataRoot();

// The file `path` names for the current engine: separators normalised ('\\' is one too, except on Windows), and a
// relative path placed under the asset root. The result is lexically normal, so ".." cannot climb out of a root
// unnoticed by isEngineWritablePath.
std::filesystem::path resolveEnginePath(const char* path);

// Whether the engine may write `resolved` (a path from resolveEnginePath): only one under the user data root.
bool isEngineWritablePath(const std::filesystem::path& resolved);

// fopen() through the roots: `path` is resolved with resolveEnginePath, and a mode that writes ('w', 'a' or '+') is
// refused, returning null with errno EACCES, unless the path is writable. The open file is recorded in the engine's
// ResourceLedger until closeEngineFile closes it.
std::FILE* openEngineFile(const char* path, const char* mode);

// fclose() for a file openEngineFile opened: forgets it in the ledger, then closes it.
int closeEngineFile(std::FILE* file);

// remove() through the roots: refused, returning -1 with errno EACCES, unless the path is writable.
int removeEngineFile(const char* path);

} // namespace rts

#endif
