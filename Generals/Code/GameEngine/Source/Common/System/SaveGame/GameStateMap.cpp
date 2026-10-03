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

// FILE: GameStateMap.cpp /////////////////////////////////////////////////////////////////////////
// Author: Colin Day, October 2002
// Desc:   Chunk in the save game file that will hold a pristine version of the map file
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"

#include <algorithm>  // std::find (m_scratchPadMaps dedupe)

#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameState.h"
#include "Common/GameStateMap.h"
#include "Common/GlobalData.h"
#include "Common/LocalFileSystem.h"
#include "Common/Xfer.h"
#include "GameClient/GameClient.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"

// GLOBALS ////////////////////////////////////////////////////////////////////////////////////////
GameStateMap *TheGameStateMap = nullptr;


// METHODS ////////////////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
GameStateMap::GameStateMap()
{

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void GameStateMap::init()
{

	m_saveDirectory = GameState::getSaveDirectory();

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
GameStateMap::~GameStateMap()
{

	//
	// clear the save directory of any temporary "scratch pad" maps that were extracted
	// from any previously loaded save game files
	//
	clearScratchPadMaps();

}

// ------------------------------------------------------------------------------------------------
/** Embed the pristine map into the xfer stream */
// ------------------------------------------------------------------------------------------------
static void embedPristineMap( AsciiString map, Xfer *xfer )
{

	// open the map file
	File *file = TheFileSystem->openFile( map.str(), File::READ | File::BINARY );
	if( file == nullptr )
	{

		DEBUG_CRASH(( "embedPristineMap - Error opening source file '%s'", map.str() ));
		throw SC_INVALID_DATA;

	}

	// how big is the map file
	Int fileSize = file->seek( 0, File::END );

	// rewind to beginning of file
	file->seek( 0, File::START );

	// allocate buffer big enough to hold the entire map file
	char *buffer = new char[ fileSize ];
	if( buffer == nullptr )
	{

		DEBUG_CRASH(( "embedPristineMap - Unable to allocate buffer for file '%s'", map.str() ));
		throw SC_INVALID_DATA;

	}

	// copy the file to the buffer
	if( file->read( buffer, fileSize ) != fileSize )
	{

		delete[] buffer;

		DEBUG_CRASH(( "embedPristineMap - Error reading from file '%s'", map.str() ));
		throw SC_INVALID_DATA;

	}

	// close the BIG file
	file->close();

	// write the contents to the save file
	DEBUG_ASSERTCRASH( xfer->getXferMode() == XFER_SAVE, ("embedPristineMap - Unsupported xfer mode") );
	xfer->beginBlock();
	xfer->xferUser( buffer, fileSize );
	xfer->endBlock();

	// delete the buffer
	delete [] buffer;

}

// ------------------------------------------------------------------------------------------------
/** Embed an "in use" map into the xfer stream.  An "in use" map is one that has already
	* been pulled out of a save game file and parked in a temporary file in the save directory */
// ------------------------------------------------------------------------------------------------
static void embedInUseMap( AsciiString map, Xfer *xfer )
{
	FILE *fp = fopen( map.str(), "rb" );

	// sanity
	if( fp == nullptr )
	{

		DEBUG_CRASH(( "embedInUseMap - Unable to open file '%s'", map.str() ));
		throw SC_INVALID_DATA;

	}

	// how big is the file
	fseek( fp, 0, SEEK_END );
	Int fileSize = ftell( fp );

	// rewind file back to start
	fseek( fp, 0, SEEK_SET );

	// GeneralsX @bugfix cemlyn007 03/10/2026 Close fp (and free buffer) on every throw path below:
	// the allocation failure, the fread failure, and xfer->xferUser/endBlock's XFER_WRITE_ERROR
	// (for example on a full disk) all now share this one catch, which closes fp and frees buffer
	// before rethrowing. Before this change none of them did; an open handle on this tracked
	// scratch-pad map fails a later DeleteFile of it on Windows, and leaks one file descriptor per
	// failed save otherwise, in a long-lived embedded host.
	char *buffer = nullptr;
	try
	{

		// allocate a buffer big enough for the entire file
		buffer = new char[ fileSize ];
		if( buffer == nullptr )
		{

			DEBUG_CRASH(( "embedInUseMap - Unable to allocate buffer for file '%s'", map.str() ));
			throw SC_INVALID_DATA;

		}

		// read the entire file
		if( fread( buffer, 1, fileSize, fp ) != fileSize )
		{

			DEBUG_CRASH(( "embedInUseMap - Error reading from file '%s'", map.str() ));
			throw SC_INVALID_DATA;

		}

		// embed file into xfer stream
		xfer->beginBlock();
		xfer->xferUser( buffer, fileSize );
		xfer->endBlock();

	}
	catch (...)
	{

		delete [] buffer;
		fclose( fp );
		throw;

	}

	// close the file
	fclose( fp );

	// delete buffer
	delete [] buffer;

}

// ------------------------------------------------------------------------------------------------
/** Extract the map from the xfer stream and save as a file with filename 'mapToSave' */
// ------------------------------------------------------------------------------------------------
static void extractAndSaveMap( AsciiString mapToSave, Xfer *xfer )
{
	UnsignedInt dataSize;

	// open handle to output file
	FILE *fp = fopen( mapToSave.str(), "w+b" );
	if( fp == nullptr )
	{

		DEBUG_CRASH(( "extractAndSaveMap - Unable to open file '%s'", mapToSave.str() ));
		throw SC_INVALID_DATA;

	}

	// GeneralsX @bugfix cemlyn007 03/10/2026 Close fp (and free buffer) on every throw path below,
	// not just the explicit DEBUG_CRASH/throw ones that already did. On a truncated save, both
	// the size read and the body read end up throwing XFER_READ_ERROR out of XferLoad: beginBlock()
	// itself never throws (on a short read it DEBUG_CRASHes and returns 0), but the
	// xferUser(buffer, 0) that follows still reaches XferLoad::xferImplementation's
	// fread(data, 0, 1, fp), which returns 0 rather than the 1 it checks for, so it throws there
	// instead. The allocation's null check below never fires either way, since new throws rather
	// than returning null: with a size of 0 it returns a valid pointer, while a corrupt (as
	// opposed to merely truncated) save can hand beginBlock() a bogus size that makes the
	// allocation itself throw ERROR_OUT_OF_MEMORY instead of XFER_READ_ERROR. Leaving fp open on
	// any of these paths meant the caller's now-tracked partial file (see m_scratchPadMaps above)
	// failed to delete on Windows (DeleteFile of an open handle fails) and leaked one file
	// descriptor per failed load everywhere else, in a long-lived embedded host.
	char *buffer = nullptr;
	try
	{

		// read data size from file
		dataSize = xfer->beginBlock();

		// allocate buffer big enough for the entire map file
		buffer = new char[ dataSize ];
		if( buffer == nullptr )
		{

			DEBUG_CRASH(( "extractAndSaveMap - Unable to allocate buffer for file '%s'", mapToSave.str() ));
			throw SC_INVALID_DATA;

		}

		// read map file
		xfer->xferUser( buffer, dataSize );

		// write contents of buffer to new file
		if( fwrite( buffer, 1, dataSize, fp ) != dataSize )
		{

			DEBUG_CRASH(( "extractAndSaveMap - Error writing to file '%s'", mapToSave.str() ));
			throw SC_INVALID_DATA;

		}

	}
	catch (...)
	{

		delete [] buffer;
		fclose( fp );
		throw;

	}

	// close the new file
	fclose( fp );

	// end of data block
	xfer->endBlock();

	// delete the buffer
	delete [] buffer;

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version
	* 2: Now storing the game mode from logic. Storing that here cause TheGameLogic->startNewGame
	*     needs to set up the player list based on it.
	*/
// ------------------------------------------------------------------------------------------------
void GameStateMap::xfer( Xfer *xfer )
{
	// version
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// get save game info
	SaveGameInfo *saveGameInfo = TheGameState->getSaveGameInfo();

	//
	// map filename, for purposes of saving we will always be saving a with a map filename
	// that refers to map in the save directory so we must always save a filename into
	// the file that is in the save directory
	//
	Bool firstSave = FALSE;  // TRUE if we haven't yet saved a pristine load of a new map
	if( xfer->getXferMode() == XFER_SAVE )
	{

		AsciiString mapLeafName = TheGameState->getMapLeafName(TheGlobalData->m_mapName);

		// construct filename to map in the save directory
		saveGameInfo->saveGameMapName = TheGameState->getFilePathInSaveDirectory(mapLeafName);

		// write map name. For cross-machine compatibility, we always write
		// it as just "Save\filename", not a full path.
		{
			AsciiString tmp = TheGameState->realMapPathToPortableMapPath(saveGameInfo->saveGameMapName);
			xfer->xferAsciiString( &tmp );
		}

		//
		// write pristine map name which is already in the member 'pristineMapName' from
		// a previous load, or in the instance where we are saving for the first time
		// and the global data map name refers to a pristine map we will copy it in there first
		//
		if (!TheGameState->isInSaveDirectory(TheGlobalData->m_mapName))
		{

			// copy the pristine name
			saveGameInfo->pristineMapName = TheGlobalData->m_mapName;

			//
			// this is also an indication that we are saving for the first time a brand new
			// map that has never been saved into this save file before (a save is also considered
			// to be a first save as long as we are writing data to disk without having loaded
			// this particular map from the save file ... so if you load USA01 for the first
			// time and save, that is a first save ... then, without quitting, if you save
			// again that is *also* considered a first save).  First save just determines
			// whether the map file we embed in the save file is taken from the maps directory
			// or from the temporary map extracted to the save directory from a load
			//
			firstSave = TRUE;

		}

		// save the pristine name
		// For cross-machine compatibility, we always write
		// it as just "Save\filename", not a full path.
		{
			AsciiString tmp = TheGameState->realMapPathToPortableMapPath(saveGameInfo->pristineMapName);
			xfer->xferAsciiString( &tmp );
		}

		if (version >= 2)
		{
			// save the game mode.
			Int gameMode = (Int)TheGameLogic->getGameMode();
			xfer->xferInt( &gameMode);
		}

	}
	else
	{

		// read the save game map name
		AsciiString tmp;
		xfer->xferAsciiString( &tmp );

		saveGameInfo->saveGameMapName = TheGameState->portableMapPathToRealMapPath(tmp);

		if (!TheGameState->isInSaveDirectory(saveGameInfo->saveGameMapName))
		{
			DEBUG_CRASH(("GameState::xfer - The map filename read from the file '%s' is not in the SAVE directory, but should be",
												 saveGameInfo->saveGameMapName.str()) );
			throw SC_INVALID_DATA;
		}

		// set this map as the map to load in the global data
		TheWritableGlobalData->m_mapName = saveGameInfo->saveGameMapName;

		// read the pristine map filename
		xfer->xferAsciiString( &saveGameInfo->pristineMapName );
		saveGameInfo->pristineMapName = TheGameState->portableMapPathToRealMapPath(saveGameInfo->pristineMapName);

		if (version >= 2)
		{
			// get the game mode.
			Int gameMode;
			xfer->xferInt(&gameMode);
			TheGameLogic->setGameMode((GameMode)gameMode);
		}

	}

	// map data
	if( xfer->getXferMode() == XFER_SAVE )
	{

		//
		// if this is a first save from a pristine map load, we need to copy the pristine
		// map into the save game file
		//
		if( firstSave == TRUE )
		{

			embedPristineMap( saveGameInfo->pristineMapName, xfer );

		}
		else
		{

			//
			// this is *NOT* a first save from a pristine map, just read the map file
			// that was extracted from the save game file during the last load and embedd
			// that into the save game file
			//
			embedInUseMap( saveGameInfo->saveGameMapName, xfer );

		}

	}
	else
	{

		//
		// take the embedded map file out of the save file, and save as its own .map file
		// in the save directory temporarily
		//
		// GeneralsX @bugfix cemlyn007 02/10/2026 Record the path before calling extractAndSaveMap,
		// not after: that function fopen("w+b")s the file before beginBlock/xferUser/fwrite; a
		// truncated save then throws XFER_READ_ERROR out of xferUser, whether the short read
		// happens in beginBlock's own header read or in the body read that follows it, while a
		// corrupt (as opposed to merely truncated) size can instead make the allocation itself
		// throw ERROR_OUT_OF_MEMORY before xferUser is ever reached -- either way leaving a
		// partial file on disk. Recording first means clearScratchPadMaps still deletes that
		// partial file even though extraction itself never returned to record it the old way. The
		// std::find guard below is purely defensive: GameState::loadGame clears m_scratchPadMaps
		// before every xfer, so within one load this vector starts empty and the guard never
		// actually matches; it only protects against a future caller that xfers more than one map
		// without that same clear.
		if( std::find( m_scratchPadMaps.begin(), m_scratchPadMaps.end(), saveGameInfo->saveGameMapName )
		    == m_scratchPadMaps.end() )
			m_scratchPadMaps.push_back( saveGameInfo->saveGameMapName );

		extractAndSaveMap( saveGameInfo->saveGameMapName, xfer );

	}

	//
	// it's important that early in the load process, we xfer the object ID counter
	// in the game logic ... this is necessary because there are flows of code that
	// create objects during the load of a save game that is *NOT FROM THE OBJECT BLOCK* of
	// code, such as loading bridges (which creates the bridge and tower objects).  We
	// are fancy and "do the right thing" in those situations, but we don't want to run
	// the risk of these objects being created and having overlapping IDs of anything
	// that we will load from the save file
	//
	ObjectID highObjectID = TheGameLogic->getObjectIDCounter();
	xfer->xferObjectID( &highObjectID );
	TheGameLogic->setObjectIDCounter( highObjectID );

	//
	// it is also equally important to xfer the drawable id counter early in the load process
	// because the act of creating objects also creates drawables, so when it comes time
	// to load the block of drawables we want to make sure that newly created drawables
	// at that time will never overlap IDs with any drawable created as part of an object
	// which then had its ID transferred in from the save file
	//
	DrawableID highDrawableID = TheGameClient->getDrawableIDCounter();
	xfer->xferDrawableID( &highDrawableID );
	TheGameClient->setDrawableIDCounter( highDrawableID );

	if (TheGameLogic->getGameMode()==GAME_SKIRMISH) {
		if (TheSkirmishGameInfo==nullptr) {
			TheSkirmishGameInfo = NEW SkirmishGameInfo;
			TheSkirmishGameInfo->init();
			TheSkirmishGameInfo->clearSlotList();
			TheSkirmishGameInfo->reset();
		}
		xfer->xferSnapshot(TheSkirmishGameInfo);
	} else {
		delete TheSkirmishGameInfo;
		TheSkirmishGameInfo = nullptr;
	}

	//
	// for loading, start a new game (flagged from a save game) ... this will load the map and all the
	// things in the map file that don't don't change (terrain, triggers, teams, script
	// definitions) etc
	//
	if( xfer->getXferMode() == XFER_LOAD ) {
		TheGameLogic->startNewGame( TRUE );
	}

}

// ------------------------------------------------------------------------------------------------
/** Delete any scratch pad maps in the save directory.  Scratch pad maps are maps that
	* were embedded in previously loaded save game files and temporarily written out as
	* their own file so that those map files could be loaded as a part of the load game
	* process */
// ------------------------------------------------------------------------------------------------
void GameStateMap::clearScratchPadMaps()
{

	// GeneralsX @bugfix cemlyn007 27/09/2026 Use the save directory cached in init(): ~GameStateMap calls
	// this at shutdown, after shutdownAll has deleted TheGameState (initialised after TheGameStateMap).
	if( m_saveDirectory.isEmpty() )
		return;

	// GeneralsX @bugfix cemlyn007 02/10/2026 Delete only the scratch-pad maps this instance's
	// extractAndSaveMap wrote (tracked in m_scratchPadMaps), instead of every *.map that happens to
	// be sitting in m_saveDirectory. m_saveDirectory is the shared, process-wide user-data Save
	// directory until PLAN-023 Phase 5's per-engine user-data root (BootConfig) lands, so another
	// live engine can have its own scratch-pad map in the same directory; deleting the whole
	// directory would remove that map out from under it (its next embedInUseMap would then fail with
	// SC_INVALID_DATA). This also only ever matches exact paths this instance wrote, so it cannot
	// be fooled by a case-variant name the way a case-insensitive directory listing could.
	//
	// GeneralsX @bugfix cemlyn007 03/10/2026 Keep an entry whose DeleteFile below failed while the
	// file is still on disk, instead of unconditionally clearing the whole vector afterwards.
	// Before per-instance tracking, the whole-directory sweep ran again on every loadGame and in
	// ~GameStateMap, so a map that failed to delete once (a transient sharing violation on
	// Windows, or EBUSY/EPERM on Linux) was retried on the next sweep. Clearing the vector
	// regardless of DeleteFile's result drops that retry: the path joins the "tracked by nobody"
	// set GameStateMap.h documents for killed or faulted engines, except now a live engine that
	// never crashed can land it there too. A later clearScratchPadMaps call, or this instance's
	// own destructor, retries whatever is left in m_scratchPadMaps.
	std::vector<AsciiString> stillPending;
	for( std::vector<AsciiString>::const_iterator it = m_scratchPadMaps.begin(); it != m_scratchPadMaps.end(); ++it )
	{

		// a scratch pad map left behind would be picked up by a later load, so say when one cannot be deleted
		if( DeleteFile( it->str() ) == 0 )
		{
			fprintf( stderr, "GameStateMap::clearScratchPadMaps - Unable to delete scratch pad map '%s'\n", it->str() );
			fflush( stderr );

			// only retry a path that is genuinely still there; a DeleteFile failure on one that is
			// already gone (another thread or process beat us to it) needs no retry
			if( TheLocalFileSystem != nullptr && TheLocalFileSystem->doesFileExist( it->str() ) )
				stillPending.push_back( *it );
		}

	}

	m_scratchPadMaps.swap( stillPending );

}
