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
// FILE: ExecutableRoots.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's asset and user data roots (see ExecutableEmbedding.h), read
// once at start-up, as paths: nothing changes the working directory.
// - The asset root is the working directory the executable was started in, as upstream reads its assets from there, or
//   the directory `-setCwd <path>` names, which upstream changed into (relative to the start-up working directory).
// - The user data root is where upstream keeps its user data: on Linux $XDG_DATA_HOME/GeneralsX/GeneralsZH, or
//   ~/.local/share/GeneralsX/GeneralsZH without it; on macOS ~/Library/Application Support/GeneralsX/GeneralsZH. It is
//   made if it does not exist yet; one that cannot be made is reported on stderr, as upstream reports it, and the
//   engine's writes to it then fail.

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include <system_error>

extern int __argc;
extern char** __argv;

namespace rts
{

std::filesystem::path executableAssetRoot()
{
	const std::filesystem::path startup = std::filesystem::current_path();
	std::filesystem::path root = startup;
	for (int i = 1; i < __argc; ++i)
	{
		if (strcasecmp(__argv[i], "-useCwd") == 0)
			root = startup;
		else if (strcasecmp(__argv[i], "-setCwd") == 0 && i + 1 < __argc)
			root = startup / __argv[++i];
	}
	return root.lexically_normal();
}

std::filesystem::path executableUserDataRoot()
{
	std::filesystem::path base;
	const char* home = std::getenv("HOME");
#if defined(__APPLE__)
	base = home != nullptr ? std::filesystem::path(home) / "Library" / "Application Support" : std::filesystem::path(".");
#else
	const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
	if (xdgDataHome != nullptr)
		base = xdgDataHome;
	else if (home != nullptr)
		base = std::filesystem::path(home) / ".local" / "share";
	else
		base = ".";
#endif
	const std::filesystem::path root = std::filesystem::absolute(base / "GeneralsX" / "GeneralsZH").lexically_normal();

	std::error_code createError;
	std::filesystem::create_directories(root, createError);
	std::error_code statError;
	if (!std::filesystem::is_directory(root, statError))
	{
		const std::error_code& reported = createError ? createError : statError;
		std::fprintf(stderr, "cannot use \"%s\" as the default user data directory%s%s\n", root.string().c_str(),
			reported ? ": " : " (not a directory)", reported ? reported.message().c_str() : "");
		std::fflush(stderr);
	}
	return root;
}

} // namespace rts

#endif
