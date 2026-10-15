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
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's asset and user data roots (see ExecutableEmbedding.h): the
// working directory it was started in, as upstream reads its assets from, until task 5.3 of baseline-engine-specs roots
// GeneralsX's file system at them.

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

namespace rts
{

std::filesystem::path executableAssetRoot()
{
	return std::filesystem::current_path();
}

std::filesystem::path executableUserDataRoot()
{
	return std::filesystem::path();
}

} // namespace rts

#endif
