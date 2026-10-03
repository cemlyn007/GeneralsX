/*
**	Command & Conquer Generals(tm)
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

// FILE: GameStateMap.h ///////////////////////////////////////////////////////////////////////////
// Author: Colin Day, October 2002
// Desc:   Chunk in the save game file that will hold a pristine version of the map file
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/AsciiString.h"
#include "Common/Snapshot.h"
#include "Common/SubsystemInterface.h"

#include <vector>

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Xfer;

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class GameStateMap : public SubsystemInterface,
										 public Snapshot
{

public:

	GameStateMap();
	virtual ~GameStateMap() override;

	// subsystem interface methods
	virtual void init() override;
	virtual void reset() override { }
	virtual void update() override { }

	// snapshot methods
	virtual void crc( Xfer *xfer ) override { }
	virtual void xfer( Xfer *xfer ) override;
	virtual void loadPostProcess() override { }

	void clearScratchPadMaps();		///< clear any scratch pad maps from the save directory

protected:

	// GeneralsX @bugfix cemlyn007 27/09/2026 Resolved once in init() so the teardown cleanup in
	// ~GameStateMap depends on no other subsystem still being alive.
	AsciiString m_saveDirectory;

	// GeneralsX @bugfix cemlyn007 02/10/2026 Paths this instance's extractAndSaveMap wrote (xfer,
	// XFER_LOAD). clearScratchPadMaps deletes only these, not every *.map in m_saveDirectory: two
	// engines can resolve the same shared user-data Save dir (PLAN-023 R1's per-engine
	// user_data_dir is a later phase), and deleting the whole directory would remove a scratch-pad
	// map another live engine's embedInUseMap still needs.
	//
	// This does not make two engines safe to save or load the *same map leaf* at once: the scratch
	// path is named after the map leaf alone (getFilePathInSaveDirectory/portableMapPathToRealMapPath,
	// GameStateMap.cpp), so two engines loading saves of that map still resolve to the identical
	// path in the shared Save dir and write over each other's extracted copy. That is the common
	// case for N training engines on one map. A real fix needs a per-engine scratch name or
	// subdirectory, which also needs a per-engine user_data_dir to be worth doing (otherwise two
	// engines could still race on the same real file while both think they own it); until then,
	// engines sharing a user-data dir must not load or save the same map concurrently.
	//
	// Known gap: this instance-scoped tracking cleans up only the maps *this* GameStateMap
	// extracted (clearScratchPadMaps, this instance's next loadGame, or its own destructor). A map
	// left by a process that was killed before destruction, or by an embedded engine that faulted
	// (deliberately leaked, never destroyed, after a fatal error -- see the launcher's fault latch),
	// is tracked by nobody and stays in the shared Save dir indefinitely. The old whole-directory
	// sweep this replaced would have removed those too, at the cost of also removing another live
	// engine's still-needed scratch map. A real fix wants the same per-engine scratch subdirectory
	// noted above, swept wholesale by whichever engine owns it on next start.
	std::vector<AsciiString> m_scratchPadMaps;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern GameStateMap *TheGameStateMap;
