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

///// EstablishConnectionsWindow.cpp /////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


#include "GameClient/GameWindowManager.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/EstablishConnectionsMenu.h"
#include "GameNetwork/GUIUtil.h"
#include "GameNetwork/GameSpy/StagingRoomGameInfo.h"

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(WindowLayout*, establishConnectionsLayout);
#define establishConnectionsLayout RTS_ENGINE_GLOBAL_REF(establishConnectionsLayout)
#else
static WindowLayout *establishConnectionsLayout;
#endif

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(NameKeyType, buttonQuitID, NAMEKEY_INVALID);
#define buttonQuitID RTS_ENGINE_GLOBAL_REF(buttonQuitID)
#else
static NameKeyType buttonQuitID = NAMEKEY_INVALID;
#endif
static NameKeyType staticPlayer1NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer2NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer3NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer4NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer5NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer6NameID = NAMEKEY_INVALID;
static NameKeyType staticPlayer7NameID = NAMEKEY_INVALID;

static NameKeyType staticPlayer1StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer2StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer3StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer4StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer5StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer6StatusID = NAMEKEY_INVALID;
static NameKeyType staticPlayer7StatusID = NAMEKEY_INVALID;

static GameWindow *buttonQuitWindow = nullptr;
static GameWindow *staticPlayer1Name = nullptr;
static GameWindow *staticPlayer2Name = nullptr;
static GameWindow *staticPlayer3Name = nullptr;
static GameWindow *staticPlayer4Name = nullptr;
static GameWindow *staticPlayer5Name = nullptr;
static GameWindow *staticPlayer6Name = nullptr;
static GameWindow *staticPlayer7Name = nullptr;

static GameWindow *staticPlayer1Status = nullptr;
static GameWindow *staticPlayer2Status = nullptr;
static GameWindow *staticPlayer3Status = nullptr;
static GameWindow *staticPlayer4Status = nullptr;
static GameWindow *staticPlayer5Status = nullptr;
static GameWindow *staticPlayer6Status = nullptr;
static GameWindow *staticPlayer7Status = nullptr;

static const char *layoutFilename = "GameSpyGameOptionsMenu.wnd";
static const char *parentName = "GameSpyGameOptionsMenuParent";
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 Tables no engine writes are constant (docs/EMBEDDING.md).
static const char * const gadgetsToHide[] =
#else
static const char *gadgetsToHide[] =
#endif
{
	"MapWindow",
	"StaticTextGameName",
	"StaticTextTeam",
	"StaticTextFaction",
	"StaticTextColor",
	"StaticTextPlayers",
	"TextEntryMapDisplay",
	"ButtonSelectMap",
	"ButtonStart",
	"StaticTextMapPreview",
	nullptr
};
#if RTS_ENGINE_CONTEXT
static const char * const perPlayerGadgetsToHide[] =
#else
static const char *perPlayerGadgetsToHide[] =
#endif
{
	"ComboBoxTeam",
	"ComboBoxColor",
	"ComboBoxPlayerTemplate",
	"ComboBoxPlayer",
	"ButtonAccept",
	"GenericPing",
	//"ButtonStartPosition",
	nullptr
};

static const char *qmlayoutFilename = "WOLQuickMatchMenu.wnd";
static const char *qmparentName = "WOLQuickMatchMenuParent";
#if RTS_ENGINE_CONTEXT
static const char * const qmgadgetsToHide[] =
#else
static const char *qmgadgetsToHide[] =
#endif
{
	"StaticTextTitle",
	"ButtonBack",
	"ButtonOptions",
	"ButtonBuddies",
	"ButtonWiden",
	"ButtonStop",
	"ButtonStart",
	nullptr
};
#if RTS_ENGINE_CONTEXT
static const char * const qmperPlayerGadgetsToHide[] =
#else
static const char *qmperPlayerGadgetsToHide[] =
#endif
{
	//"ButtonStartPosition",
	nullptr
};

static void showGameSpyGameOptionsUnderlyingGUIElements( Bool show )
{
	ShowUnderlyingGUIElements( show, layoutFilename, parentName, gadgetsToHide, perPlayerGadgetsToHide );

}
static void showGameSpyQMUnderlyingGUIElements( Bool show )
{
	ShowUnderlyingGUIElements( show, qmlayoutFilename, qmparentName, qmgadgetsToHide, qmperPlayerGadgetsToHide );
}

static void InitEstablishConnectionsDialog() {
	buttonQuitID = TheNameKeyGenerator->nameToKey( "EstablishConnectionsScreen.wnd:ButtonQuit" );
	buttonQuitWindow = TheWindowManager->winGetWindowFromId(nullptr, buttonQuitID);
}

void ShowEstablishConnectionsWindow() {
	if (establishConnectionsLayout == nullptr) {
		establishConnectionsLayout = TheWindowManager->winCreateLayout( "Menus/EstablishConnectionsScreen.wnd" );
		InitEstablishConnectionsDialog();
	}
	establishConnectionsLayout->hide(FALSE);
	TheWindowManager->winSetFocus(establishConnectionsLayout->getFirstWindow());
	if (!TheGameSpyGame->isQMGame())
	{
		showGameSpyGameOptionsUnderlyingGUIElements(FALSE);
	}
	else
	{
		showGameSpyQMUnderlyingGUIElements(FALSE);
	}
}

void HideEstablishConnectionsWindow() {
	if (establishConnectionsLayout == nullptr) {
//		establishConnectionsLayout = TheWindowManager->winCreateLayout( "Menus/EstablishConnectionsScreen.wnd" );
//		InitEstablishConnectionsDialog();
		return;
	}
//	establishConnectionsLayout->hide(TRUE);
//	establishConnectionsLayout->hide(TRUE);
//	TheWindowManager->winDestroy(establishConnectionsLayout);
	establishConnectionsLayout->destroyWindows();
	deleteInstance(establishConnectionsLayout);
	establishConnectionsLayout = nullptr;
	if (!TheGameSpyGame->isQMGame())
	{
		showGameSpyGameOptionsUnderlyingGUIElements(TRUE);
	}
	else
	{
		showGameSpyQMUnderlyingGUIElements(TRUE);
	}
}

WindowMsgHandledType EstablishConnectionsControlInput(GameWindow *window, UnsignedInt msg,
																											WindowMsgData mData1, WindowMsgData mData2) {

	return MSG_IGNORED;
}

WindowMsgHandledType EstablishConnectionsControlSystem(GameWindow *window, UnsignedInt msg,
																											 WindowMsgData mData1, WindowMsgData mData2) {

	switch (msg) {
		case GBM_SELECTED:
			{
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();

				if (controlID == buttonQuitID) {
					TheEstablishConnectionsMenu->abortGame();
				}
				break;
			}
	}
	return MSG_HANDLED;
}
