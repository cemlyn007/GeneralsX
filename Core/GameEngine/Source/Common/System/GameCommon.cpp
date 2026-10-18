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

// GameCommon.h
// Part of header detangling
// John McDonald, Aug 2002

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GameCommon.h"

const char *const TheVeterancyNames[] =
{
	"REGULAR",
	"VETERAN",
	"ELITE",
	"HEROIC",
	nullptr
};
static_assert(ARRAY_SIZE(TheVeterancyNames) == LEVEL_COUNT + 1, "Incorrect array size");

// GeneralsX @feature cemlyn007 16/10/2026 The names of GameDifficulty's values, which an embedding host's vocabulary
// generator reads, so that its difficulty enumeration cannot drift from the engine's.
const char *const TheDifficultyNames[] =
{
	"EASY",
	"NORMAL",
	"HARD",
	nullptr
};
static_assert(ARRAY_SIZE(TheDifficultyNames) == DIFFICULTY_COUNT + 1, "Incorrect array size");

// GeneralsX @feature cemlyn007 16/10/2026 The names of CellShroudStatus's values, which an embedding host's vocabulary
// generator reads, so that its shroud state enumeration cannot drift from the engine's.
const char *const TheCellShroudStatusNames[] =
{
	"CLEAR",
	"FOGGED",
	"SHROUDED",
	nullptr
};
static_assert(ARRAY_SIZE(TheCellShroudStatusNames) == CELLSHROUD_SHROUDED + 2, "Incorrect array size");

const char *const TheRelationshipNames[] =
{
	"ENEMIES",
	"NEUTRAL",
	"ALLIES",
	nullptr
};
static_assert(ARRAY_SIZE(TheRelationshipNames) == RELATIONSHIP_COUNT + 1, "Incorrect array size");

//-------------------------------------------------------------------------------------------------
// TheSuperHackers @todo DO NOT USE THIS FUNCTION! Use WWMath::Normalize_Angle instead. Delete this.
Real normalizeAngle(Real angle)
{
	DEBUG_ASSERTCRASH(!_isnan(angle), ("Angle is NAN in normalizeAngle!"));

	if( _isnan(angle) )
		return 0;// ARGH!!!! Don't assert and then not handle it!  Error bad!  Fix error!

	while (angle > PI)
		angle -= 2*PI;

	while (angle <= -PI)
		angle += 2*PI;

	return angle;
}

