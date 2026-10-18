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

// FILE: PopupReplay.cpp /////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   Generals
//
// File name: PopupReplay.cpp
//
// Created:   Matthew D. Campbell, November 2002
//
// Desc:      the Replay Save window control
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/LocalFileSystem.h"
#include "Common/MessageStream.h"
#include "Common/Recorder.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Shell.h"
#include "GameLogic/GameLogic.h"


// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(NameKeyType, buttonBackKey, NAMEKEY_INVALID);
#define buttonBackKey RTS_ENGINE_GLOBAL_REF(buttonBackKey)
#else
static NameKeyType buttonBackKey					= NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, buttonSaveKey, NAMEKEY_INVALID);
#define buttonSaveKey RTS_ENGINE_GLOBAL_REF(buttonSaveKey)
#else
static NameKeyType buttonSaveKey					= NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, listboxGamesKey, NAMEKEY_INVALID);
#define listboxGamesKey RTS_ENGINE_GLOBAL_REF(listboxGamesKey)
#else
static NameKeyType listboxGamesKey				= NAMEKEY_INVALID;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(NameKeyType, textEntryReplayNameKey, NAMEKEY_INVALID);
#define textEntryReplayNameKey RTS_ENGINE_GLOBAL_REF(textEntryReplayNameKey)
#else
static NameKeyType textEntryReplayNameKey = NAMEKEY_INVALID;
#endif

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, parent, nullptr);
#define parent RTS_ENGINE_GLOBAL_REF(parent)
#else
static GameWindow *parent = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, replaySavedParent, nullptr);
#define replaySavedParent RTS_ENGINE_GLOBAL_REF(replaySavedParent)
#else
static GameWindow *replaySavedParent = nullptr;
#endif

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(time_t, s_fileSavePopupStartTime, 0);
#define s_fileSavePopupStartTime RTS_ENGINE_GLOBAL_REF(s_fileSavePopupStartTime)
#else
static time_t s_fileSavePopupStartTime = 0;
#endif
static const time_t s_fileSavePopupDuration = 1000;

// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
extern void PopulateReplayFileListbox(GameWindow *listbox);
extern void ScoreScreenEnableControls(Bool enable);
extern UnicodeString GetReplayFilenameFromListbox(GameWindow *listbox, Int index);
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL(std::string, LastReplayFileName);
#define LastReplayFileName RTS_ENGINE_GLOBAL_REF(LastReplayFileName)
#else
extern std::string LastReplayFileName;
#endif

//-------------------------------------------------------------------------------------------------
/** Show or hide the "Replay Saved" popup */
//-------------------------------------------------------------------------------------------------
void ShowReplaySavedPopup(Bool show)
{
	if (replaySavedParent != nullptr) {
		if (show) {
			replaySavedParent->winHide(FALSE);
		} else {
			replaySavedParent->winHide(TRUE);
		}
	}
}

// ------------------------------------------------------------------------------------------------
/** Close the save/load menu */
// ------------------------------------------------------------------------------------------------
static void closeSaveMenu( GameWindow *window )
{
	WindowLayout *layout = window->winGetLayout();

	if( layout )
		layout->hide( TRUE );

}

//-------------------------------------------------------------------------------------------------
/** Initialize the SaveLoad menu */
//-------------------------------------------------------------------------------------------------
void PopupReplayInit( WindowLayout *layout, void *userData )
{

  // get ids for our children controls
	buttonBackKey					 = NAMEKEY( "PopupReplay.wnd:ButtonBack" );
	buttonSaveKey					 = NAMEKEY( "PopupReplay.wnd:ButtonSave" );
	listboxGamesKey				 = NAMEKEY( "PopupReplay.wnd:ListboxGames" );
	textEntryReplayNameKey = NAMEKEY( "PopupReplay.wnd:TextEntryReplayName" );

	//set keyboard focus to main parent and set modal
	NameKeyType parentID = TheNameKeyGenerator->nameToKey("PopupReplay.wnd:PopupReplayMenu");
	parent = TheWindowManager->winGetWindowFromId( nullptr, parentID );
	TheWindowManager->winSetFocus( parent );

	NameKeyType replaySavedParentID = TheNameKeyGenerator->nameToKey("PopupReplay.wnd:PopupReplaySaved");
	replaySavedParent = TheWindowManager->winGetWindowFromId( nullptr, replaySavedParentID);
	if (replaySavedParent == nullptr) {
		DEBUG_CRASH(("replaySavedParent == nullptr"));
	}

	ShowReplaySavedPopup(FALSE);

	// enable the menu action buttons
	GameWindow *buttonFrame = TheWindowManager->winGetWindowFromId( parent, NAMEKEY( "PopupReplay.wnd:MenuButtonFrame" ) );
	buttonFrame->winEnable( TRUE );

	// get the listbox that will have the save games in it
	GameWindow *listboxGames = TheWindowManager->winGetWindowFromId( nullptr, listboxGamesKey );
	DEBUG_ASSERTCRASH( listboxGames != nullptr, ("PopupReplayInit - Unable to find games listbox") );

	// populate the listbox with the save games on disk
	PopulateReplayFileListbox(listboxGames);

	GameWindow *textEntryReplayName = TheWindowManager->winGetWindowFromId( parent, textEntryReplayNameKey );
	GadgetTextEntrySetText(textEntryReplayName, UnicodeString::TheEmptyString);
	TheWindowManager->winSetFocus( textEntryReplayName );

	//Disable the button immediately as the code above us starts off with an empty string.
	GameWindow *control = TheWindowManager->winGetWindowFromId( parent, buttonSaveKey );
	if( control )
	{
		control->winEnable( FALSE );
	}

}

//-------------------------------------------------------------------------------------------------
/** SaveLoad menu shutdown method */
//-------------------------------------------------------------------------------------------------
void PopupReplayShutdown( WindowLayout *layout, void *userData )
{
	parent = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** SaveLoad menu update method */
//-------------------------------------------------------------------------------------------------
void PopupReplayUpdate( WindowLayout *layout, void *userData )
{

	if (s_fileSavePopupStartTime != 0)
	{
		// the replay save confirmation popup is up
		// check to see if its time to take it down.
		if ((timeGetTime() - s_fileSavePopupStartTime) >= s_fileSavePopupDuration)
		{
			ShowReplaySavedPopup(FALSE);

			// close the save/load menu
			closeSaveMenu( parent );
			ScoreScreenEnableControls(TRUE);

			// reset the timer to 0 cause we have to.
			s_fileSavePopupStartTime = 0;
		}
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WindowMsgHandledType PopupReplayInput( GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;

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
						GameWindow *button = TheWindowManager->winGetWindowFromId( parent, buttonBackKey );
						TheWindowManager->winSendSystemMsg( window, GBM_SELECTED,
																								(WindowMsgData)button, buttonBackKey );

					}

					// don't let key fall through anywhere else
					return MSG_HANDLED;

				}

			}

		}

	}

	return MSG_IGNORED;
}

static void reallySaveReplay();
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(std::string, replayPath);
#define replayPath RTS_ENGINE_GLOBAL_REF(replayPath)
#else
static std::string replayPath;
#endif

// ------------------------------------------------------------------------------------------------
/** Save the replay */
// ------------------------------------------------------------------------------------------------
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(GameWindow*, messageBoxWin, nullptr);
#define messageBoxWin RTS_ENGINE_GLOBAL_REF(messageBoxWin)
#else
static GameWindow *messageBoxWin = nullptr;
#endif
static void saveReplay( UnicodeString filename )
{
	AsciiString translated;
	if (filename == TheGameText->fetch("GUI:LastReplay"))
	{
		translated = TheRecorder->getLastReplayFileName();
	}
	else
	{
		translated.translate(filename);
	}

	AsciiString fullPath = TheRecorder->getReplayDir();
	fullPath.concat(translated);
	fullPath.concat(TheRecorder->getReplayExtention());

	replayPath = fullPath.str();
	messageBoxWin = nullptr;
	if (TheLocalFileSystem->doesFileExist(fullPath.str()))
	{
		messageBoxWin = MessageBoxOkCancel(TheGameText->fetch("GUI:OverwriteReplayTitle"), TheGameText->fetch("GUI:OverwriteReplay"), reallySaveReplay, nullptr);
	}
	else
	{
		reallySaveReplay();
	}
}
void reallySaveReplay()
{
	AsciiString filename = replayPath.c_str();

	AsciiString oldFilename;
	oldFilename = TheRecorder->getReplayDir();
	oldFilename.concat(LastReplayFileName.c_str());
	oldFilename.concat(TheRecorder->getReplayExtention());

	if (oldFilename == filename)
		return;

	if (TheLocalFileSystem->doesFileExist(filename.str()))
	{
		if(DeleteFile(filename.str()) == 0)
		{
			wchar_t buffer[1024];
			FormatMessageW ( FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), 0, buffer, ARRAY_SIZE(buffer), nullptr);
			UnicodeString errorStr;
			errorStr.set(buffer);
			errorStr.trim();
			if(messageBoxWin)
			{
				TheWindowManager->winUnsetModal(messageBoxWin);
				messageBoxWin = nullptr;
			}
			MessageBoxOk(TheGameText->fetch("GUI:Error"),errorStr, nullptr);

			// get the listbox that will have the save games in it
			GameWindow *listboxGames = TheWindowManager->winGetWindowFromId( parent, listboxGamesKey );
			DEBUG_ASSERTCRASH( listboxGames != nullptr, ("reallySaveReplay - Unable to find games listbox") );

			// populate the listbox with the save games on disk
			PopulateReplayFileListbox(listboxGames);
			return;
		}
	}

	// copy the replay to the right place
	if(CopyFile(oldFilename.str(),filename.str(), FALSE) == 0)
	{
		wchar_t buffer[1024];
		FormatMessageW( FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(), 0, buffer, ARRAY_SIZE(buffer), nullptr);
		UnicodeString errorStr;
		errorStr.set(buffer);
		errorStr.trim();
		if(messageBoxWin)
		{
			TheWindowManager->winUnsetModal(messageBoxWin);
			messageBoxWin = nullptr;
		}
		MessageBoxOk(TheGameText->fetch("GUI:Error"),errorStr, nullptr);
		return;
	}

	// get the listbox that will have the save games in it
	GameWindow *listboxGames = TheWindowManager->winGetWindowFromId( parent, listboxGamesKey );
	DEBUG_ASSERTCRASH( listboxGames != nullptr, ("reallySaveReplay - Unable to find games listbox") );

	// populate the listbox with the save games on disk
	PopulateReplayFileListbox(listboxGames);

	ShowReplaySavedPopup(TRUE);
	s_fileSavePopupStartTime = timeGetTime();
}

//-------------------------------------------------------------------------------------------------
/** SaveLoad menu system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType PopupReplaySystem( GameWindow *window, UnsignedInt msg,
																				 WindowMsgData mData1, WindowMsgData mData2 )
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

			break;

		}

    //----------------------------------------------------------------------------------------------
    case GWM_INPUT_FOCUS:
		{

			// if we're givin the opportunity to take the keyboard focus we must say we want it
			if( mData1 == TRUE )
				*(Bool *)mData2 = TRUE;

			break;

		}

		// --------------------------------------------------------------------------------------------
		case GLM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;

			GameWindow *listboxGames = TheWindowManager->winGetWindowFromId( window, listboxGamesKey );
			DEBUG_ASSERTCRASH( listboxGames != nullptr, ("PopupReplaySystem - Unable to find games listbox") );

			//
			// handle games listbox, when certain items are selected in the listbox only some
			// commands are available
			//
			if( control == listboxGames )
			{
				int rowSelected = mData2;
				if (rowSelected >= 0)
				{
					UnicodeString filename;
					filename = GadgetListBoxGetText(listboxGames, rowSelected);
					GameWindow *textEntryReplayName = TheWindowManager->winGetWindowFromId( window, textEntryReplayNameKey );
					DEBUG_ASSERTCRASH( textEntryReplayName != nullptr, ("PopupReplaySystem - Unable to find text entry") );
					GadgetTextEntrySetText(textEntryReplayName, filename);
				}
			}

			break;

		}

    //---------------------------------------------------------------------------------------------
		case GEM_EDIT_DONE:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

      if( controlID == textEntryReplayNameKey )
      {
				UnicodeString filename = GadgetTextEntryGetText( control );
				if (filename.isEmpty())
					break;

				saveReplay(filename);

      }

			break;

		}
    //---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

      if( controlID == buttonSaveKey )
      {
				// get the filename, and see if we are overwriting
				GameWindow *textEntryReplayName = TheWindowManager->winGetWindowFromId( window, textEntryReplayNameKey );
				DEBUG_ASSERTCRASH( textEntryReplayName != nullptr, ("PopupReplaySystem - Unable to find text entry") );

				UnicodeString filename = GadgetTextEntryGetText( textEntryReplayName );
				if (filename.isEmpty())
					break;

				saveReplay(filename);

      }
			else if( controlID == buttonBackKey )
			{

				// close the save/load menu
				closeSaveMenu( window );
				ScoreScreenEnableControls(TRUE);

			}

			break;

		}

		case GEM_UPDATE_TEXT:
		{
			//Kris:
			//Enable or disable the save button -- disabled when empty.
			GameWindow *control = TheWindowManager->winGetWindowFromId( parent, textEntryReplayNameKey );
			if( control )
			{
				UnicodeString filename;
				filename.set( GadgetTextEntryGetText( control ) );
				control = TheWindowManager->winGetWindowFromId( parent, buttonSaveKey );
				if( control )
				{
					if( filename.isEmpty() )
					{
						control->winEnable( FALSE );
					}
					else
					{
						control->winEnable( TRUE );
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
