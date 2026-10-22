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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// RandomValue.h
// Random number generation system
// Author: Michael S. Booth, January 1998

#pragma once

#include "Lib/BaseType.h"

#if !RTS_ENGINE_CONTEXT
extern void InitRandom();
#endif
// GeneralsX @feature cemlyn007 16/10/2026 With RTS_ENGINE_CONTEXT every seeding names its seed: there is no seeding
// from the clock, so nothing an engine does depends on when it ran (the simulation-determinism spec). A game's seed
// comes from its set-up (skirmish, network or replay game info); before the first game an engine runs on seed 0.
extern void InitRandom( UnsignedInt seed );
extern UnsignedInt GetGameLogicRandomSeed();   ///< Get the seed (used for replays)
extern UnsignedInt GetGameLogicRandomSeedCRC();///< Get the seed (used for CRCs)
#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 21/10/2026 Seed a client random stream of six words, as InitRandom seeds the engine's, for
// a particle simulation that keeps a stream of its own (ParticleSystemManager::beginSimulations)
extern void SeedClientRandomStream( UnsignedInt seed, UnsignedInt *stream );
#endif

//--------------------------------------------------------------------------------------------------------------
