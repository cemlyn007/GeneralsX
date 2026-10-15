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

// FILE: GameSpyChat.h //////////////////////////////////////////////////////
// Generals GameSpy Chat
// Author: Matthew D. Campbell, February 2002

#pragma once

#include "gamespy/peer/peer.h"

class GameWindow;
class WindowLayout;

Bool GameSpySendChat(UnicodeString message, Bool isEmote, GameWindow *playerListbox = nullptr);
void GameSpyAddText( UnicodeString message, GameSpyColors color = GSCOLOR_DEFAULT );

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_EXTERN_GLOBAL(GameWindow*, progressTextWindow);
#define progressTextWindow RTS_ENGINE_GLOBAL_REF(progressTextWindow)
#else
extern GameWindow *progressTextWindow;				///< Text box on the progress screen
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(GameWindow*, quickmatchTextWindow);
#define quickmatchTextWindow RTS_ENGINE_GLOBAL_REF(quickmatchTextWindow)
#else
extern GameWindow *quickmatchTextWindow;			///< Text box on the quickmatch screen
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(GameWindow*, quickmatchTextWindow);
#define quickmatchTextWindow RTS_ENGINE_GLOBAL_REF(quickmatchTextWindow)
#else
extern GameWindow *quickmatchTextWindow;			///< Text box on the quickmatch screen
#endif
extern GameWindow *listboxLobbyChat;					///< Chat box on the custom lobby screen
extern GameWindow *listboxLobbyPlayers;				///< Player box on the custom lobby screen
extern GameWindow *listboxLobbyGames;					///< Game box on the custom lobby screen
extern GameWindow *listboxLobbyChatChannels;	///< Chat channel box on the custom lobby screen
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(GameWindow*, listboxGameSetupChat);
#define listboxGameSetupChat RTS_ENGINE_GLOBAL_REF(listboxGameSetupChat)
#else
extern GameWindow *listboxGameSetupChat;			///< Chat box on the custom game setup screen
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(WindowLayout*, WOLMapSelectLayout);
#define WOLMapSelectLayout RTS_ENGINE_GLOBAL_REF(WOLMapSelectLayout)
#else
extern WindowLayout *WOLMapSelectLayout;			///< Map selection overlay
#endif

void RoomMessageCallback(PEER peer, RoomType roomType,
												 const char * nick, const char * message,
												 MessageType messageType, void * param);		///< Called when a message arrives in a room.

void PlayerMessageCallback(PEER peer, const char * nick,
													 const char * message, MessageType messageType,
													 void * param);														///< Called when a private message is received from another player.
