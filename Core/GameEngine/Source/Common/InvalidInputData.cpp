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
// FILE: InvalidInputData.cpp
// GeneralsX @feature cemlyn007 15/10/2026 See Common/InvalidInputData.h. The destructor is defined here, so the class's
// type information is emitted once, with default visibility.

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/InvalidInputData.h"

#include <utility>

namespace rts
{

InvalidInputData::InvalidInputData(std::string path_, std::string reason_)
	: std::runtime_error("invalid data in " + path_ + ": " + reason_), path(std::move(path_)), reason(std::move(reason_))
{
}

InvalidInputData::~InvalidInputData() = default;

} // namespace rts
