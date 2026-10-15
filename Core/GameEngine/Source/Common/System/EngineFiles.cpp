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
// FILE: EngineFiles.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The engine's file layer (see Common/EngineFiles.h)

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT

#include "Common/EngineFiles.h"

#include "Common/EmbeddingContract.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace rts
{

namespace
{

// The ledger ids of the files this engine has open, by file. Destroyed after the engine's other per-engine statics,
// so that a file one of their destructors closes is still found.
struct OpenEngineFiles
{
	std::mutex mutex;
	std::unordered_map<std::FILE*, ResourceLedger::Id> ids;
};

OpenEngineFiles& openEngineFiles()
{
	RTS_ENGINE_STATIC_LAST(OpenEngineFiles, files);
	return files;
}

ResourceLedger& engineLedger()
{
	return *currentEmbedding("the file layer's resource ledger").resourceLedger;
}

bool writes(const char* mode)
{
	return std::strpbrk(mode, "wa+") != nullptr;
}

bool liesUnder(const std::filesystem::path& path, const std::filesystem::path& root)
{
	if (root.empty())
		return false;
	const std::filesystem::path normalRoot = root.lexically_normal();
	auto rootPart = normalRoot.begin();
	auto pathPart = path.begin();
	for (; rootPart != normalRoot.end(); ++rootPart, ++pathPart)
	{
		if (rootPart->empty())
			continue;
		if (pathPart == path.end() || *pathPart != *rootPart)
			return false;
	}
	return true;
}

void refuseWrite(const std::filesystem::path& resolved, const char* what)
{
	std::fprintf(stderr, "rts::%s - '%s' is not under the engine's user data root ('%s'): the write is refused\n", what,
		resolved.string().c_str(), engineUserDataRoot().string().c_str());
	std::fflush(stderr);
	errno = EACCES;
}

} // namespace

std::filesystem::path engineAssetRoot()
{
	return currentEmbedding("the asset root").assetRoot;
}

std::filesystem::path engineUserDataRoot()
{
	return currentEmbedding("the user data root").userDataRoot;
}

std::filesystem::path resolveEnginePath(const char* path)
{
	std::string text(path != nullptr ? path : "");
#ifndef _WIN32
	std::replace(text.begin(), text.end(), '\\', '/');
#endif
	std::filesystem::path resolved(std::move(text));
	if (resolved.is_relative())
		resolved = engineAssetRoot() / resolved;
	return resolved.lexically_normal();
}

bool isEngineWritablePath(const std::filesystem::path& resolved)
{
	return liesUnder(resolved.lexically_normal(), engineUserDataRoot());
}

std::FILE* openEngineFile(const char* path, const char* mode)
{
	const std::filesystem::path resolved = resolveEnginePath(path);
	if (writes(mode) && !isEngineWritablePath(resolved))
	{
		refuseWrite(resolved, "openEngineFile");
		return nullptr;
	}

	ResourceLedger& ledger = engineLedger();
	std::FILE* const file = std::fopen(resolved.c_str(), mode);
	if (file == nullptr)
		return nullptr;

	try
	{
		const ResourceLedger::Id id = ledger.record("file " + resolved.string(), reinterpret_cast<std::uintptr_t>(file),
			[](std::uintptr_t handle) { std::fclose(reinterpret_cast<std::FILE*>(handle)); });
		OpenEngineFiles& files = openEngineFiles();
		const std::scoped_lock lock(files.mutex);
		files.ids[file] = id;
	}
	catch (...)
	{
		std::fclose(file);
		throw;
	}
	return file;
}

int closeEngineFile(std::FILE* file)
{
	if (file == nullptr)
		return EOF;

	ResourceLedger& ledger = engineLedger();
	OpenEngineFiles& files = openEngineFiles();
	bool recorded = false;
	ResourceLedger::Id id = 0;
	{
		const std::scoped_lock lock(files.mutex);
		const auto found = files.ids.find(file);
		if (found != files.ids.end())
		{
			recorded = true;
			id = found->second;
			files.ids.erase(found);
		}
	}
	if (recorded)
		ledger.forget(id);
	return std::fclose(file);
}

int removeEngineFile(const char* path)
{
	const std::filesystem::path resolved = resolveEnginePath(path);
	if (!isEngineWritablePath(resolved))
	{
		refuseWrite(resolved, "removeEngineFile");
		return -1;
	}
	return std::remove(resolved.c_str());
}

} // namespace rts

#endif
