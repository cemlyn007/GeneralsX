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
// FILE: RandomValueSeeds.h
// GeneralsX @feature cemlyn007 28/09/2026 The random generators' upstream initial seeds, in one place
// (PLAN-023 Phase 2): RandomValue.cpp's process-wide seeds (RTS_ENGINE_CONTEXT off) and the EngineContext
// fields (on) both start from them. Plain C, so that every build, VC6 included, can include it.
#pragma once

#define RTS_RANDOM_SEED_INITIAL_VALUES {0xf22d0e56U, 0x883126e9U, 0xc624dd2fU, 0x702c49cU, 0x9e353f7dU, 0x6fdf3b64U}
