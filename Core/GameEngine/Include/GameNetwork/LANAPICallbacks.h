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

///////////////////////////////////////////////////////////////////////////////////////
// FILE: LANAPICallbacks.h
// Author: Chris Huybregts, October 2001
// Description: LAN API Callbacks header
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/NameKeyGenerator.h"
#include "GameClient/Shell.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameNetwork/LANAPI.h"


// LAN API Singleton ----------------------------------------------------------------------
#if !RTS_ENGINE_CONTEXT
extern LANAPI *TheLAN;
#endif

//external declarations of the Gadgets the callbacks can use
// LanLobby
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_EXTERN_GLOBAL(NameKeyType, listboxChatWindowID);
#define listboxChatWindowID RTS_ENGINE_GLOBAL_REF(listboxChatWindowID)
#else
extern NameKeyType listboxChatWindowID;
#endif
extern GameWindow *listboxChatWindow;
extern GameWindow *listboxPlayers;
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(NameKeyType, listboxGamesID);
#define listboxGamesID RTS_ENGINE_GLOBAL_REF(listboxGamesID)
#else
extern NameKeyType listboxGamesID;
#endif
extern GameWindow *listboxGames;
//LanGame Options screen
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(NameKeyType, listboxChatWindowLanGameID);
#define listboxChatWindowLanGameID RTS_ENGINE_GLOBAL_REF(listboxChatWindowLanGameID)
#else
extern NameKeyType listboxChatWindowLanGameID;
#endif
extern GameWindow *listboxChatWindowLanGame;
extern WindowLayout *mapSelectLayout;
// ScoreScreen
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(NameKeyType, listboxChatWindowScoreScreenID);
#define listboxChatWindowScoreScreenID RTS_ENGINE_GLOBAL_REF(listboxChatWindowScoreScreenID)
#else
extern NameKeyType listboxChatWindowScoreScreenID;
#endif
extern GameWindow *listboxChatWindowScoreScreen;


//Colors used for the chat dialogs
extern const Color playerColor;
extern const Color gameColor;
extern const Color gameInProgressColor;
extern const Color chatNormalColor;
extern const Color chatActionColor;
extern const Color chatLocalNormalColor;
extern const Color chatLocalActionColor;
extern const Color chatSystemColor;
extern const Color chatSystemColor;
extern const Color acceptTrueColor;
extern const Color acceptFalseColor;


void lanUpdateSlotList();
void updateGameOptions();
void setLANPlayerTooltip(LANPlayer* player);

//Enum is used for the utility function so other windows do not need
//to know about controls on LanGameOptions window.
enum PostToLanGameType CPP_11(: Int){ SEND_GAME_OPTS = 0,
												MAP_BACK,
												POST_TO_LAN_GAME_TYPE_COUNT };
//the utility function mentioned above
void PostToLanGameOptions(PostToLanGameType post);
