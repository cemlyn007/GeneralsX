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

// FILE: Display.cpp //////////////////////////////////////////////////////////
// The implementation of the Display class
// Author: Michael S. Booth, March 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/Display.h"
#include "GameClient/Mouse.h"
#include "GameClient/VideoPlayer.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GlobalLanguage.h"
#if RTS_ENGINE_CONTEXT
#include "Common/EngineContext.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "Common/Player.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "Common/PlayerList.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "Common/Radar.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "GameClient/Drawable.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "GameClient/GameClient.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "GameClient/ParticleSys.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#include "GameLogic/GhostObject.h"	// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player
#endif
//#include "GameLogic/ScriptEngine.h"
//#include "GameLogic/GameLogic.h"

/// The Display singleton instance.
#if !RTS_ENGINE_CONTEXT
Display *TheDisplay = nullptr;
#endif


Display::Display()
{
	m_viewList = nullptr;
	m_width = 0;
	m_height = 0;
	m_bitDepth = 0;
	m_windowed = FALSE;
	m_videoBuffer = nullptr;
	m_videoStream = nullptr;
	m_debugDisplayCallback = nullptr;
	m_debugDisplayUserData = nullptr;
	m_debugDisplay = nullptr;
	m_letterBoxFadeLevel = 0;
	m_letterBoxEnabled = FALSE;
	m_cinematicText = AsciiString::TheEmptyString;
	m_cinematicFont = nullptr;
	m_cinematicTextFrames = 0;

	m_currentlyPlayingMovie.clear();
	m_letterBoxFadeStartTime = 0;
	m_isBatching = FALSE;
}

/**
 * Destructor for the Display.  Destroy all views attached to it.
 */
Display::~Display()
{

	stopMovie();
	// delete all our views if present
	deleteViews();

}

/**
	* Delete all views in the Display
	*/
void Display::deleteViews()
{
	View *v, *next;

	for( v = m_viewList; v; v = next )
	{
		next = v->getNextView();
		delete v;
	}
	m_viewList = nullptr;
}

/**
 * Attach the given view to the world
 * @todo Rethink the "attachView" notion...
 */
void Display::attachView( View *view )
{
	// prepend to head of list
	m_viewList = view->prependViewToList( m_viewList );
}

/**
 * Render all views of the world
 */
void Display::drawViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->drawView();

}

/**
 * Updates all views of the world.  This forces state variables
   to refresh without actually drawing anything.
 */
void Display::updateViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->updateView();

}

void Display::stepViews()
{

	for( View *v = m_viewList; v; v = v->getNextView() )
		v->stepView();

}

#if RTS_ENGINE_CONTEXT
namespace
{

// ------------------------------------------------------------------------------------------------
// GeneralsX @feature cemlyn007 21/10/2026 For one draw as a player (Display::drawPlayerView): makes the view the
// tactical view and the display's only view, and, for a player, makes it the viewing player, so that the observed or
// local player is that player for the draw's shroud tests, tints and looks. It points the ghost objects at the player,
// gives every drawable the looks it has in the player's view (Drawable::beginViewLook) and refreshes the radar's
// objects for the player, as rts::changeObservedPlayer does, but with none of its other effects, and puts all of it
// back as it was when the draw ends.
// ------------------------------------------------------------------------------------------------
class PlayerViewDraw
{
public:
	PlayerViewDraw( View *&viewList, View *view, Player *viewer )
		: m_viewList( viewList ), m_savedViewList( viewList ), m_savedTacticalView( TheTacticalView ), m_viewer( viewer ),
			m_savedGhostIndex( TheGhostObjectManager ? TheGhostObjectManager->getLocalPlayerIndex() : 0 )
	{
		TheTacticalView = view;
		m_viewList = view;
		if( m_viewer == nullptr )
			return;
		rts::ctx()->viewingPlayer = m_viewer->getPlayerIndex();
		if( TheGhostObjectManager )
			TheGhostObjectManager->setLocalPlayerIndex( m_viewer->getPlayerIndex() );
		for( Drawable *draw = TheGameClient->firstDrawable(); draw; draw = draw->getNextDrawable() )
			draw->beginViewLook( m_viewer );
		if( TheRadar )
			TheRadar->refreshObjects();
	}

	~PlayerViewDraw()
	{
		if( m_viewer != nullptr )
		{
			for( Drawable *draw = TheGameClient->firstDrawable(); draw; draw = draw->getNextDrawable() )
				draw->endViewLook();
			if( TheGhostObjectManager )
				TheGhostObjectManager->setLocalPlayerIndex( m_savedGhostIndex );
			rts::ctx()->viewingPlayer = -1;
			if( TheRadar )
				TheRadar->refreshObjects();
		}
		m_viewList = m_savedViewList;
		TheTacticalView = m_savedTacticalView;
	}

	PlayerViewDraw( const PlayerViewDraw & ) = delete;
	PlayerViewDraw &operator=( const PlayerViewDraw & ) = delete;

private:
	View *&m_viewList;
	View *m_savedViewList;
	View *m_savedTacticalView;
	Player *m_viewer;
	Int m_savedGhostIndex;
};

}

// ------------------------------------------------------------------------------------------------
// GeneralsX @feature cemlyn007 21/10/2026 Drawing as a player (Display.h): draw the tactical view as the player sees
// the game, with the player's particle simulation (ParticleSystemManager::beginSimulations), or as the game's local
// player sees it, for a player index of -1
// ------------------------------------------------------------------------------------------------
void Display::drawPlayerView( Int playerIndex )
{
	View *view = TheTacticalView;
	Player *viewer = playerIndex < 0 ? nullptr : ThePlayerList->getNthPlayer( playerIndex );
	if( view == nullptr || (playerIndex >= 0 && viewer == nullptr) )
	{
		DEBUG_CRASH(( "Display::drawPlayerView - player %d has no view", playerIndex ));
		return;
	}
	ParticleSystemManager *const watcherParticles = TheParticleSystemManager;
	const ParticleSimulationScope particles( viewer ? watcherParticles->findSimulationOfPlayer( playerIndex ) : watcherParticles, FALSE );
	const PlayerViewDraw drawing( m_viewList, view, viewer );
	draw();
}
#endif

/// Redraw the entire display
void Display::draw()
{
	// redraw all views
	drawViews();

	// redraw the in-game user interface
	/// @todo Switch between in-game and shell interfaces

}

/** Sets screen resolution/mode*/
Bool Display::setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed )
{
	//Get old values
	UnsignedInt oldDisplayHeight=getHeight();
	UnsignedInt oldDisplayWidth=getWidth();
	Int oldViewWidth=TheTacticalView->getWidth();
	Int oldViewHeight=TheTacticalView->getHeight();
	Int oldViewOriginX,oldViewOriginY;
	TheTacticalView->getOrigin(&oldViewOriginX,&oldViewOriginY);

	setWidth(xres);
	setHeight(yres);

	//Adjust view to match previous proportions
	TheTacticalView->setWidth((Real)oldViewWidth/(Real)oldDisplayWidth*(Real)xres);
	TheTacticalView->setHeight((Real)oldViewHeight/(Real)oldDisplayHeight*(Real)yres);
	TheTacticalView->setOrigin((Real)oldViewOriginX/(Real)oldDisplayWidth*(Real)xres,
	(Real)oldViewOriginY/(Real)oldDisplayHeight*(Real)yres);
	return TRUE;
}

// Display::setWidth ==========================================================
/** Set the width of the display */
//=============================================================================
void Display::setWidth( UnsignedInt width )
{

	// set the new width
	m_width = width;

	// set the new mouse limits
	if( TheMouse )
		TheMouse->setMouseLimits();

}

// Display::setHeight =========================================================
/** Set the height of the display */
//=============================================================================
void Display::setHeight( UnsignedInt height )
{

	// se the new height
	m_height = height;

	// set the new mouse limits
	if( TheMouse )
		TheMouse->setMouseLimits();

}

//============================================================================
// Display::playMovie
//============================================================================

void Display::playMovie( AsciiString movieName)
{
	if (TheGlobalData->m_headless)
		return;

	stopMovie();



	m_videoStream = TheVideoPlayer->open( movieName );

	if ( m_videoStream == nullptr )
	{
		return;
	}

	m_currentlyPlayingMovie = movieName;

	m_videoBuffer = createVideoBuffer();
	if (	m_videoBuffer == nullptr ||
				!m_videoBuffer->allocate(	m_videoStream->width(),
													m_videoStream->height())
		)
	{
		stopMovie();
		return;
	}

}

//============================================================================
// Display::stopMovie
//============================================================================

void Display::stopMovie()
{
	delete m_videoBuffer;
	m_videoBuffer = nullptr;

	if ( m_videoStream )
	{
		m_videoStream->close();
		m_videoStream = nullptr;
	}

	if (!m_currentlyPlayingMovie.isEmpty()) {
		//TheScriptEngine->notifyOfCompletedVideo(m_currentlyPlayingMovie); // Removing this sync-error cause MDC
		m_currentlyPlayingMovie = AsciiString::TheEmptyString;
	}
}

//============================================================================
// Display::update
//============================================================================

void Display::update()
{
	if ( m_videoStream && m_videoBuffer )
	{
		if ( m_videoStream->isFrameReady())
		{
			m_videoStream->frameDecompress();
			m_videoStream->frameRender( m_videoBuffer );
			if( m_videoStream->frameIndex() != m_videoStream->frameCount() - 1)
			{
				m_videoStream->frameNext();
			}
			else
			{
				stopMovie();
			}
		}
	}
}

//============================================================================
// Display::reset
//============================================================================

void Display::reset()
{
	//Remove letterbox border that may have been enabled by a script
	m_letterBoxFadeLevel = 0;
	m_letterBoxEnabled = FALSE;
	stopMovie();

	// Reset all views that need resetting
	for( View *v = m_viewList; v; v = v->getNextView() )
		v->reset();
}

//============================================================================
// Display::isMoviePlaying
//============================================================================

Bool Display::isMoviePlaying()
{
	return m_videoStream != nullptr && m_videoBuffer != nullptr;
}

//============================================================================
// Display::setDebugDisplayCallback
//============================================================================

void Display::setDebugDisplayCallback( DebugDisplayCallback *callback, void *userData )
{
	m_debugDisplayCallback = callback;
	m_debugDisplayUserData = userData;
}

//============================================================================
// Display::getDebugDisplayCallback
//============================================================================

Display::DebugDisplayCallback *Display::getDebugDisplayCallback()
{
	return m_debugDisplayCallback;
}

void Display::beginBatch()
{
	if (m_isBatching)
	{
		return;
	}
	m_isBatching = TRUE;
	onBeginBatch();
}

void Display::endBatch()
{
	if (!m_isBatching)
	{
		return;
	}
	onFlush();
	m_isBatching = FALSE;
	onEndBatch();
}

void Display::flush()
{
	if (m_isBatching)
	{
		onFlush();
	}
}
