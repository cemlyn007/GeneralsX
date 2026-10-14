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

// FILE: PopupLadderSelect.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	August 2002
//
//	Filename: 	PopupLadderSelect.cpp
//
//	author:		Matthew D. Campbell
//
//	purpose:	Contains the Callbacks for the Ladder Select Popup
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/encrypt.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameText.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/MapUtil.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameNetwork/GameSpy/LadderDefs.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
//#include "GameNetwork/GameSpy/PeerThread.h"
#include "GameNetwork/GameSpyOverlay.h"


//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

static NameKeyType parentID = NAMEKEY_INVALID;
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(NameKeyType, listboxLadderSelectID, NAMEKEY_INVALID);
#define listboxLadderSelectID RTS_ENGINE_GLOBAL_REF(listboxLadderSelectID)
#else
static NameKeyType listboxLadderSelectID = NAMEKEY_INVALID;
#endif
static NameKeyType listboxLadderDetailsID = NAMEKEY_INVALID;
static NameKeyType staticTextLadderNameID = NAMEKEY_INVALID;
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonOkID, NAMEKEY_INVALID);
#define buttonOkID RTS_ENGINE_GLOBAL_REF(buttonOkID)
#else
static NameKeyType buttonOkID = NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonCancelID, NAMEKEY_INVALID);
#define buttonCancelID RTS_ENGINE_GLOBAL_REF(buttonCancelID)
#else
static NameKeyType buttonCancelID = NAMEKEY_INVALID;
#endif

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, parent, nullptr);
#define parent RTS_ENGINE_GLOBAL_REF(parent)
#else
static GameWindow *parent = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, listboxLadderSelect, nullptr);
#define listboxLadderSelect RTS_ENGINE_GLOBAL_REF(listboxLadderSelect)
#else
static GameWindow *listboxLadderSelect = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, listboxLadderDetails, nullptr);
#define listboxLadderDetails RTS_ENGINE_GLOBAL_REF(listboxLadderDetails)
#else
static GameWindow *listboxLadderDetails = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, staticTextLadderName, nullptr);
#define staticTextLadderName RTS_ENGINE_GLOBAL_REF(staticTextLadderName)
#else
static GameWindow *staticTextLadderName = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, buttonOk, nullptr);
#define buttonOk RTS_ENGINE_GLOBAL_REF(buttonOk)
#else
static GameWindow *buttonOk = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, buttonCancel, nullptr);
#define buttonCancel RTS_ENGINE_GLOBAL_REF(buttonCancel)
#else
static GameWindow *buttonCancel = nullptr;
#endif

// password entry popup
static NameKeyType passwordParentID = NAMEKEY_INVALID;
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonPasswordOkID, NAMEKEY_INVALID);
#define buttonPasswordOkID RTS_ENGINE_GLOBAL_REF(buttonPasswordOkID)
#else
static NameKeyType buttonPasswordOkID = NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonPasswordCancelID, NAMEKEY_INVALID);
#define buttonPasswordCancelID RTS_ENGINE_GLOBAL_REF(buttonPasswordCancelID)
#else
static NameKeyType buttonPasswordCancelID = NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, textEntryPasswordID, NAMEKEY_INVALID);
#define textEntryPasswordID RTS_ENGINE_GLOBAL_REF(textEntryPasswordID)
#else
static NameKeyType textEntryPasswordID = NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, passwordParent, nullptr);
#define passwordParent RTS_ENGINE_GLOBAL_REF(passwordParent)
#else
static GameWindow *passwordParent = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, textEntryPassword, nullptr);
#define textEntryPassword RTS_ENGINE_GLOBAL_REF(textEntryPassword)
#else
static GameWindow *textEntryPassword = nullptr;
#endif

// incorrect password popup
static NameKeyType badPasswordParentID = NAMEKEY_INVALID;
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonBadPasswordOkID, NAMEKEY_INVALID);
#define buttonBadPasswordOkID RTS_ENGINE_GLOBAL_REF(buttonBadPasswordOkID)
#else
static NameKeyType buttonBadPasswordOkID = NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, badPasswordParent, nullptr);
#define badPasswordParent RTS_ENGINE_GLOBAL_REF(badPasswordParent)
#else
static GameWindow *badPasswordParent = nullptr;
#endif

#if RTS_ENGINE_CONTEXT
#undef staticTextLadderName
#undef listboxLadderDetails
#endif
static void updateLadderDetails( Int ladderID, GameWindow *staticTextLadderName, GameWindow *listboxLadderDetails );
#if RTS_ENGINE_CONTEXT
#define staticTextLadderName RTS_ENGINE_GLOBAL_REF(staticTextLadderName)
#define listboxLadderDetails RTS_ENGINE_GLOBAL_REF(listboxLadderDetails)
#endif

void PopulateQMLadderComboBox();
void PopulateCustomLadderComboBox();

void PopulateQMLadderListBox( GameWindow *win );
void PopulateCustomLadderListBox( GameWindow *win );

void HandleQMLadderSelection(Int ladderID);
void HandleCustomLadderSelection(Int ladderID);

void CustomMatchHideHostPopup(Bool hide);

static void populateLadderComboBox()
{
	// only one of these will do any work...
	PopulateQMLadderComboBox();
	PopulateCustomLadderComboBox();
}

static void populateLadderListBox()
{
	// only one of these will do any work...
	PopulateQMLadderListBox(listboxLadderSelect);
	PopulateCustomLadderListBox(listboxLadderSelect);

	Int selIndex, selID;
	GadgetListBoxGetSelected(listboxLadderSelect, &selIndex);
	if (selIndex < 0)
		return;
	// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
	selID = static_cast<Int>(reinterpret_cast<intptr_t>(GadgetListBoxGetItemData(listboxLadderSelect, selIndex)));
	if (!selID)
		return;
	updateLadderDetails(selID, staticTextLadderName, listboxLadderDetails);
}

static void handleLadderSelection( Int ladderID )
{
	// only one of these will do any work...
	HandleQMLadderSelection(ladderID);
	HandleCustomLadderSelection(ladderID);
}


enum PasswordMode CPP_11(: Int)
{
	PASS_NONE,
	PASS_ENTRY,
	PASS_ERROR
};

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(PasswordMode, s_currentMode, PASS_NONE);
#define s_currentMode RTS_ENGINE_GLOBAL_REF(s_currentMode)
#else
static PasswordMode s_currentMode = PASS_NONE;
#endif
static void setPasswordMode(PasswordMode mode)
{
	s_currentMode = mode;
	switch(mode)
	{
		case PASS_NONE:
			if (passwordParent)
				passwordParent->winHide(TRUE);
			if (badPasswordParent)
				badPasswordParent->winHide(TRUE);
			if (buttonOk)
				buttonOk->winEnable(TRUE);
			if (buttonCancel)
				buttonCancel->winEnable(TRUE);
			if (textEntryPassword)
				textEntryPassword->winEnable(FALSE);
			if (listboxLadderSelect)
				listboxLadderSelect->winEnable(TRUE);
			TheWindowManager->winSetFocus(listboxLadderSelect);
			break;
		case PASS_ENTRY:
			if (passwordParent)
				passwordParent->winHide(FALSE);
			if (badPasswordParent)
				badPasswordParent->winHide(TRUE);
			if (buttonOk)
				buttonOk->winEnable(FALSE);
			if (buttonCancel)
				buttonCancel->winEnable(FALSE);
			if (textEntryPassword)
			{
				textEntryPassword->winEnable(TRUE);
				GadgetTextEntrySetText(textEntryPassword, UnicodeString::TheEmptyString);
			}
			if (listboxLadderSelect)
				listboxLadderSelect->winEnable(FALSE);
			TheWindowManager->winSetFocus(textEntryPassword);
			break;
		case PASS_ERROR:
			if (passwordParent)
				passwordParent->winHide(TRUE);
			if (badPasswordParent)
				badPasswordParent->winHide(FALSE);
			if (buttonOk)
				buttonOk->winEnable(FALSE);
			if (buttonCancel)
				buttonCancel->winEnable(FALSE);
			if (textEntryPassword)
				textEntryPassword->winEnable(FALSE);
			if (listboxLadderSelect)
				listboxLadderSelect->winEnable(FALSE);
			TheWindowManager->winSetFocus(parent);
			break;
	}
}

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/** Initialize the menu */
//-------------------------------------------------------------------------------------------------
void PopupLadderSelectInit( WindowLayout *layout, void *userData )
{
	parentID = NAMEKEY("PopupLadderSelect.wnd:Parent");
	parent = TheWindowManager->winGetWindowFromId(nullptr, parentID);

	listboxLadderSelectID = NAMEKEY("PopupLadderSelect.wnd:ListBoxLadderSelect");
	listboxLadderSelect = TheWindowManager->winGetWindowFromId(parent, listboxLadderSelectID);

	listboxLadderDetailsID = NAMEKEY("PopupLadderSelect.wnd:ListBoxLadderDetails");
	listboxLadderDetails = TheWindowManager->winGetWindowFromId(parent, listboxLadderDetailsID);

	staticTextLadderNameID = NAMEKEY("PopupLadderSelect.wnd:StaticTextLadderName");
	staticTextLadderName = TheWindowManager->winGetWindowFromId(parent, staticTextLadderNameID);

	buttonOkID = NAMEKEY("PopupLadderSelect.wnd:ButtonOk");
	buttonCancelID = NAMEKEY("PopupLadderSelect.wnd:ButtonCancel");

	buttonOk = TheWindowManager->winGetWindowFromId(parent, buttonOkID);
	buttonCancel = TheWindowManager->winGetWindowFromId(parent, buttonCancelID);

	TheWindowManager->winSetFocus( parent );
	TheWindowManager->winSetModal( parent );

	// password entry popup
	passwordParentID = NAMEKEY("PopupLadderSelect.wnd:PasswordParent");
	passwordParent = TheWindowManager->winGetWindowFromId(parent, passwordParentID);
	buttonPasswordOkID = NAMEKEY("PopupLadderSelect.wnd:ButtonPasswordOk");
	buttonPasswordCancelID = NAMEKEY("PopupLadderSelect.wnd:ButtonPasswordCancel");
	textEntryPasswordID = NAMEKEY("PopupLadderSelect.wnd:PasswordEntry");
	textEntryPassword = TheWindowManager->winGetWindowFromId(parent, textEntryPasswordID);

	// bad password popup
	badPasswordParentID = NAMEKEY("PopupLadderSelect.wnd:BadPasswordParent");
	badPasswordParent = TheWindowManager->winGetWindowFromId(parent, badPasswordParentID);
	buttonBadPasswordOkID = NAMEKEY("PopupLadderSelect.wnd:ButtonBadPasswordOk");

	setPasswordMode(PASS_NONE);

	CustomMatchHideHostPopup(TRUE);

	// populate list box (based on whether we're in custom or quickmatch)
	populateLadderListBox();
}

//-------------------------------------------------------------------------------------------------
/** Input callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PopupLadderSelectInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;
//			if (buttonPushed)
//				break;

			switch( key )
			{

				// ----------------------------------------------------------------------------------------
				case KEY_ESC:
				{

					//
					// send a simulated selected event to the parent window of the
					// back/exit button
					//
					if( BitIsSet( state, KEY_STATE_UP ) )
					{
						switch (s_currentMode)
						{
						case PASS_NONE:
							// re-select whatever was chosen before
							populateLadderComboBox();
							GameSpyCloseOverlay(GSOVERLAY_LADDERSELECT);
							break;
						case PASS_ENTRY:
						case PASS_ERROR:
							setPasswordMode(PASS_NONE);
							break;
						}

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;

}

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Int, ladderIndex, 0);
#define ladderIndex RTS_ENGINE_GLOBAL_REF(ladderIndex)
#else
static Int ladderIndex = 0;
#endif
void ladderSelectedCallback()
{
	handleLadderSelection( ladderIndex );

	// update combo box
	populateLadderComboBox();

	// tear down overlay
	GameSpyCloseOverlay( GSOVERLAY_LADDERSELECT );
}

//-------------------------------------------------------------------------------------------------
/** System callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PopupLadderSelectSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
  switch( msg )
	{
		// --------------------------------------------------------------------------------------------
		case GWM_CREATE:
		{
			break;
		}
    //---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
		{
			parent = nullptr;
			listboxLadderSelect = nullptr;
			listboxLadderDetails = nullptr;
			CustomMatchHideHostPopup(FALSE);
			break;
		}

    //----------------------------------------------------------------------------------------------
    case GWM_INPUT_FOCUS:
		{
			// if we're given the opportunity to take the keyboard focus we must say we want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = TRUE;
			break;
		}
    //----------------------------------------------------------------------------------------------
    case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();
			if (controlID == buttonOkID)
			{
				// save selection
				Int selectPos = -1;
				GadgetListBoxGetSelected( listboxLadderSelect, &selectPos );
				if (selectPos < 0)
					break;

			// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
			ladderIndex = static_cast<Int>(reinterpret_cast<intptr_t>(GadgetListBoxGetItemData( listboxLadderSelect, selectPos, 0 )));
				const LadderInfo *li = TheLadderList->findLadderByIndex( ladderIndex );
				if (li && li->cryptedPassword.isNotEmpty())
				{
					// need password asking
					setPasswordMode(PASS_ENTRY);
				}
				else
				{
					ladderSelectedCallback();
				}
			}
			else if (controlID == buttonCancelID)
			{
				// reset what had been
				populateLadderComboBox();

				// tear down overlay
				GameSpyCloseOverlay( GSOVERLAY_LADDERSELECT );
			}
			else if (controlID == buttonPasswordOkID)
			{
				const LadderInfo *li = TheLadderList->findLadderByIndex( ladderIndex );
				if (!li || li->cryptedPassword.isEmpty())
				{
					// eh?  something's not right.  just pretend they typed something wrong...
					setPasswordMode(PASS_ERROR);
					break;
				}

				AsciiString pass;
				pass.translate(GadgetTextEntryGetText(textEntryPassword));
				if ( pass.isNotEmpty() ) // password ok
				{
					AsciiString cryptPass = EncryptString(pass.str());
					DEBUG_LOG(("pass is %s, crypted pass is %s, comparing to %s",
						pass.str(), cryptPass.str(), li->cryptedPassword.str()));
					if (cryptPass == li->cryptedPassword)
						ladderSelectedCallback();
					else
						setPasswordMode(PASS_ERROR);
				}
				else
				{
					setPasswordMode(PASS_ERROR);
				}
			}
			else if (controlID == buttonPasswordCancelID)
			{
				setPasswordMode(PASS_NONE);
			}
			else if (controlID == buttonBadPasswordOkID)
			{
				setPasswordMode(PASS_NONE);
			}
			break;
		}

    //---------------------------------------------------------------------------------------------
		case GLM_SELECTED:
		{
			Int selIndex, selID;
			GadgetListBoxGetSelected(listboxLadderSelect, &selIndex);
			if (selIndex < 0)
				break;

		// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
		selID = static_cast<Int>(reinterpret_cast<intptr_t>(GadgetListBoxGetItemData(listboxLadderSelect, selIndex)));
		}

    //---------------------------------------------------------------------------------------------
		case GLM_DOUBLE_CLICKED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();
			Int selectPos = (Int)mData2;
			GadgetListBoxSetSelected(control, &selectPos);

      if( controlID == listboxLadderSelectID )
			{
				TheWindowManager->winSendSystemMsg( parent, GBM_SELECTED,
																					(WindowMsgData)buttonOk, buttonOk->winGetWindowId() );
			}
			break;
		}

    //---------------------------------------------------------------------------------------------
		case GEM_EDIT_DONE:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();
			if (controlID == textEntryPasswordID)
			{
				TheWindowManager->winSendSystemMsg( parent, GBM_SELECTED,
																					(WindowMsgData)(TheWindowManager->winGetWindowFromId(passwordParent, buttonPasswordOkID)), buttonPasswordOkID );
			}
			break;
		}

		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}


//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

#if RTS_ENGINE_CONTEXT
#undef staticTextLadderName
#undef listboxLadderDetails
#endif
static void updateLadderDetails( Int selID, GameWindow *staticTextLadderName, GameWindow *listboxLadderDetails )
{
	if (!staticTextLadderName || !listboxLadderDetails)
		return;

	GadgetStaticTextSetText(staticTextLadderName, UnicodeString::TheEmptyString);
	GadgetListBoxReset(listboxLadderDetails);

	const LadderInfo *info = TheLadderList->findLadderByIndex(selID);
	if (!info)
		return;

	UnicodeString line;
	Color color = GameMakeColor( 255, 255, 255, 255 );
	Color captionColor = GameMakeColor( 0, 255, 255, 255 );

	// name
	line.format(TheGameText->fetch("GUI:LadderNameAndSize"), info->name.str(), info->playersPerTeam, info->playersPerTeam);
	GadgetStaticTextSetText(staticTextLadderName, line);

	// location
	if (!info->location.isEmpty())
		GadgetListBoxAddEntryText(listboxLadderDetails, info->location, captionColor, -1);

	// homepage
	line.format(TheGameText->fetch("GUI:LadderURL"), info->homepageURL.str());
	GadgetListBoxAddEntryText(listboxLadderDetails, line, captionColor, -1);

	// description
	if (!info->description.isEmpty())
		GadgetListBoxAddEntryText(listboxLadderDetails, info->description, color, -1);

	// requires password?
	if (info->cryptedPassword.isNotEmpty())
	{
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch("GUI:LadderHasPassword"), captionColor, -1);
	}

	// wins limits
	if (info->minWins)
	{
		line.format(TheGameText->fetch("GUI:LadderMinWins"), info->minWins);
		GadgetListBoxAddEntryText(listboxLadderDetails, line, captionColor, -1);
	}
	if (info->maxWins)
	{
		line.format(TheGameText->fetch("GUI:LadderMaxWins"), info->maxWins);
		GadgetListBoxAddEntryText(listboxLadderDetails, line, captionColor, -1);
	}

	// random factions?
	if (info->randomFactions)
	{
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch("GUI:LadderRandomFactions"), captionColor, -1);
	}
	else
	{
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch("GUI:LadderFactions"), captionColor, -1);
	}

	// factions
	AsciiStringList validFactions = info->validFactions;
	AsciiStringListIterator it = validFactions.begin();
	for (; it != validFactions.end(); ++it)
	{
		AsciiString marker;
		marker.format("INI:Faction%s", it->str());
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch(marker), color, -1);
	}

	// random maps?
	if (info->randomMaps)
	{
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch("GUI:LadderRandomMaps"), captionColor, -1);
	}
	else
	{
		GadgetListBoxAddEntryText(listboxLadderDetails, TheGameText->fetch("GUI:LadderMaps"), captionColor, -1);
	}

	// maps
	AsciiStringList validMaps = info->validMaps;
	for (it = validMaps.begin(); it != validMaps.end(); ++it)
	{
		const MapMetaData *md = TheMapCache->findMap(*it);
		if (md)
		{
			GadgetListBoxAddEntryText(listboxLadderDetails, md->m_displayName, color, -1);
		}
	}
}
#if RTS_ENGINE_CONTEXT
#define staticTextLadderName RTS_ENGINE_GLOBAL_REF(staticTextLadderName)
#define listboxLadderDetails RTS_ENGINE_GLOBAL_REF(listboxLadderDetails)
#endif

static void closeRightClickMenu(GameWindow *win)
{

	if(win)
	{
		WindowLayout *winLay = win->winGetLayout();
		if(!winLay)
			return;
		winLay->destroyWindows();
		deleteInstance(winLay);
		winLay = nullptr;

	}
}
void RCGameDetailsMenuInit( WindowLayout *layout, void *userData )
{
}

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 The function's own buttonOkID hides the file's from here on.
#undef buttonOkID
#endif
WindowMsgHandledType RCGameDetailsMenuSystem( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{

#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_STATIC(NameKeyType, ladderInfoID, NAMEKEY_INVALID);
	RTS_ENGINE_STATIC(NameKeyType, buttonOkID, NAMEKEY_INVALID);
#else
	static NameKeyType ladderInfoID = NAMEKEY_INVALID;
	static NameKeyType buttonOkID = NAMEKEY_INVALID;
#endif
	switch( msg )
	{

		case GWM_CREATE:
			{
				ladderInfoID = NAMEKEY("RCGameDetailsMenu.wnd:ButtonLadderDetails");
				buttonOkID = NAMEKEY("PopupLadderDetails.wnd:ButtonOk");
				break;
			}

		case GGM_CLOSE:
			{
				closeRightClickMenu(window);
				//rcMenu = nullptr;
				break;
			}

		case GWM_DESTROY:
			{
				break;
			}

		case GBM_SELECTED:
			{
				GameWindow *control = (GameWindow *)mData1;
				Int controlID = control->winGetWindowId();
			// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast
			Int selectedID = static_cast<Int>(reinterpret_cast<intptr_t>(window->winGetUserData()));
				if (controlID == ladderInfoID)
				{
					StagingRoomMap *srm = TheGameSpyInfo->getStagingRoomList();
					StagingRoomMap::iterator srmIt = srm->find(selectedID);
					if (srmIt != srm->end())
					{
						GameSpyStagingRoom *theRoom = srmIt->second;
						if (!theRoom)
							break;
						const LadderInfo *linfo = TheLadderList->findLadder(theRoom->getLadderIP(), theRoom->getLadderPort());
						if (linfo)
						{
							WindowLayout *rcLayout = TheWindowManager->winCreateLayout("Menus/PopupLadderDetails.wnd");
							if (!rcLayout)
								break;

							GameWindow *rcMenu = rcLayout->getFirstWindow();
							rcMenu->winGetLayout()->runInit();
							rcMenu->winBringToTop();
							rcMenu->winHide(FALSE);

				// GeneralsX @build BenderAI 12/02/2026 64-bit safe pointer cast (int to pointer)
				rcMenu->winSetUserData(reinterpret_cast<void*>(static_cast<intptr_t>(selectedID)));				TheWindowManager->winSetLoneWindow(rcMenu);

				GameWindow *st = TheWindowManager->winGetWindowFromId(nullptr,
					NAMEKEY("PopupLadderDetails.wnd:StaticTextLadderName"));							GameWindow *lb = TheWindowManager->winGetWindowFromId(nullptr,
								NAMEKEY("PopupLadderDetails.wnd:ListBoxLadderDetails"));
							updateLadderDetails(selectedID, st, lb);
						}
					}
				}
				break;
			}
		default:
			return MSG_IGNORED;

	}
	return MSG_HANDLED;
}
