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
// FILE: MainMenuUtils.cpp
// Author: Matthew D. Campbell, Sept 2002
// Description: GameSpy version check, patch download, etc utils
///////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <cstdint>
#include <fcntl.h>

//#include "Common/Registry.h"
#include "Common/UserPreferences.h"
#include "Common/version.h"
#include "GameClient/GameText.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Shell.h"
#include "GameLogic/ScriptEngine.h"

#include "GameClient/ShellHooks.h"

#include "gamespy/ghttp/ghttp.h"

#include "GameNetwork/DownloadManager.h"
#include "GameNetwork/GameSpy/BuddyThread.h"
#include "GameNetwork/GameSpy/MainMenuUtils.h"
#include "GameNetwork/GameSpy/PeerDefs.h"
#include "GameNetwork/GameSpy/PeerThread.h"

#include "WWDownload/Registry.h"
#include "WWDownload/urlBuilder.h"
#if RTS_ENGINE_CONTEXT
#include "Common/EngineThread.h" // GeneralsX @feature cemlyn007 15/10/2026 rts::startEngineThread
#include "Common/EngineSockets.h" // GeneralsX @feature cemlyn007 16/10/2026 rts::resolveEngineHostName
// GeneralsX @feature cemlyn007 15/10/2026 Its files are opened and closed through the engine's file layer (Common/EngineFiles.h)
#include "Common/EngineFiles.h"
#endif


///////////////////////////////////////////////////////////////////////////////////////

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(Bool, checkingForPatchBeforeGameSpy, FALSE);
#define checkingForPatchBeforeGameSpy RTS_ENGINE_GLOBAL_REF(checkingForPatchBeforeGameSpy)
#else
static Bool checkingForPatchBeforeGameSpy = FALSE;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Int, checksLeftBeforeOnline, 0);
#define checksLeftBeforeOnline RTS_ENGINE_GLOBAL_REF(checksLeftBeforeOnline)
#else
static Int checksLeftBeforeOnline = 0;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Int, timeThroughOnline, 0); // used to avoid having old callbacks cause problems
#define timeThroughOnline RTS_ENGINE_GLOBAL_REF(timeThroughOnline)
#else
static Int timeThroughOnline = 0; // used to avoid having old callbacks cause problems
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, mustDownloadPatch, FALSE);
#define mustDownloadPatch RTS_ENGINE_GLOBAL_REF(mustDownloadPatch)
#else
static Bool mustDownloadPatch = FALSE;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, cantConnectBeforeOnline, FALSE);
#define cantConnectBeforeOnline RTS_ENGINE_GLOBAL_REF(cantConnectBeforeOnline)
#else
static Bool cantConnectBeforeOnline = FALSE;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(std::list<QueuedDownload>, queuedDownloads);
#define queuedDownloads RTS_ENGINE_GLOBAL_REF(queuedDownloads)
#else
static std::list<QueuedDownload> queuedDownloads;
#endif

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(char*, MOTDBuffer, nullptr);
#define MOTDBuffer RTS_ENGINE_GLOBAL_REF(MOTDBuffer)
#else
static char *MOTDBuffer = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(char*, configBuffer, nullptr);
#define configBuffer RTS_ENGINE_GLOBAL_REF(configBuffer)
#else
static char *configBuffer = nullptr;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL_DEFINE(GameWindow*, onlineCancelWindow, nullptr);
#define onlineCancelWindow RTS_ENGINE_GLOBAL_REF(onlineCancelWindow)
#else
GameWindow *onlineCancelWindow = nullptr;
#endif

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, s_asyncDNSThreadDone, TRUE);
#define s_asyncDNSThreadDone RTS_ENGINE_GLOBAL_REF(s_asyncDNSThreadDone)
#else
static Bool s_asyncDNSThreadDone = TRUE;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, s_asyncDNSThreadSucceeded, FALSE);
#define s_asyncDNSThreadSucceeded RTS_ENGINE_GLOBAL_REF(s_asyncDNSThreadSucceeded)
#else
static Bool s_asyncDNSThreadSucceeded = FALSE;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, s_asyncDNSLookupInProgress, FALSE);
#define s_asyncDNSLookupInProgress RTS_ENGINE_GLOBAL_REF(s_asyncDNSLookupInProgress)
#else
static Bool s_asyncDNSLookupInProgress = FALSE;
#endif
#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 15/10/2026 The lookup thread, started through the engine's ThreadStarter
RTS_ENGINE_GLOBAL(std::shared_ptr<rts::EngineThread>, s_asyncDNSThreadHandle);
#define s_asyncDNSThreadHandle RTS_ENGINE_GLOBAL_REF(s_asyncDNSThreadHandle)
#else
static HANDLE s_asyncDNSThreadHandle = nullptr;
#endif
enum {
	LOOKUP_INPROGRESS,
	LOOKUP_FAILED,
	LOOKUP_SUCCEEDED,
};

///////////////////////////////////////////////////////////////////////////////////////

static void startOnline();
static void reallyStartPatchCheck();

///////////////////////////////////////////////////////////////////////////////////////

// someone has hit a button allowing downloads to start
void StartDownloadingPatches()
{
	if (queuedDownloads.empty())
	{
		HandleCanceledDownload();
		return;
	}

	WindowLayout *layout;
	layout = TheWindowManager->winCreateLayout( "Menus/DownloadMenu.wnd" );
	layout->runInit();
	layout->hide( FALSE );
	layout->bringForward();
	HandleCanceledDownload(FALSE);
	DEBUG_ASSERTCRASH(TheDownloadManager, ("No download manager!"));
	if (TheDownloadManager)
	{
		std::list<QueuedDownload>::iterator it = queuedDownloads.begin();
		while (it != queuedDownloads.end())
		{
			QueuedDownload q = *it;
			TheDownloadManager->queueFileForDownload(q.server, q.userName, q.password,
				q.file, q.localFile, q.regKey, q.tryResume);
			queuedDownloads.pop_front();
			it = queuedDownloads.begin();
		}
		TheDownloadManager->downloadNextQueuedFile();
	}
}

///////////////////////////////////////////////////////////////////////////////////////

// user agrees to patch before going online
static void patchBeforeOnlineCallback()
{
	StartDownloadingPatches();
}

// user doesn't want to patch before going online
static void noPatchBeforeOnlineCallback()
{
	queuedDownloads.clear();
	if (mustDownloadPatch || cantConnectBeforeOnline)
	{
		// go back to normal
		HandleCanceledDownload();
	}
	else
	{
		// clear out unneeded downloads and go on
		startOnline();
	}
}

///////////////////////////////////////////////////////////////////////////////////////

static Bool hasWriteAccess()
{
#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 15/10/2026 The probe asked whether the game directory, the working directory upstream, can be
	// written for a patch. With RTS_ENGINE_CONTEXT that is the engine's asset root, which is read-only (Common/EngineFiles.h).
	return false;
#else
	const char* filename = "PatchAccessTest.txt";

	remove(filename);

	int handle = open( filename, O_CREAT | O_RDWR, S_IREAD | S_IWRITE);
	if (handle == -1)
	{
		return false;
	}

	close(handle);
	remove(filename);

	unsigned int val;
	if (!GetUnsignedIntFromRegistry("", "Version", val))
	{
		return false;
	}

	if (!SetUnsignedIntInRegistry("", "Version", val))
	{
		return false;
	}

	return true;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////

static void startOnline()
{
	checkingForPatchBeforeGameSpy = FALSE;

	DEBUG_ASSERTCRASH(checksLeftBeforeOnline==0, ("starting online with pending callbacks"));
	if (onlineCancelWindow)
	{
		TheWindowManager->winDestroy(onlineCancelWindow);
		onlineCancelWindow = nullptr;
	}

	if (cantConnectBeforeOnline)
	{
		MessageBoxOk(TheGameText->fetch("GUI:CannotConnectToServservTitle"),
			TheGameText->fetch("GUI:CannotConnectToServserv"),
			noPatchBeforeOnlineCallback);
		return;
	}
	if (!queuedDownloads.empty())
	{
		if (!hasWriteAccess())
		{
			MessageBoxOk(TheGameText->fetch("GUI:Error"),
				TheGameText->fetch("GUI:MustHaveAdminRights"),
				noPatchBeforeOnlineCallback);
		}
		else if (mustDownloadPatch)
		{
			MessageBoxOkCancel(TheGameText->fetch("GUI:PatchAvailable"),
				TheGameText->fetch("GUI:MustPatchForOnline"),
				patchBeforeOnlineCallback, noPatchBeforeOnlineCallback);
		}
		else
		{
			MessageBoxYesNo(TheGameText->fetch("GUI:PatchAvailable"),
				TheGameText->fetch("GUI:CanPatchForOnline"),
				patchBeforeOnlineCallback, noPatchBeforeOnlineCallback);
		}
		return;
	}

	TheScriptEngine->signalUIInteract(TheShellHookNames[SHELL_SCRIPT_HOOK_MAIN_MENU_ONLINE_SELECTED]);

	DEBUG_ASSERTCRASH( !TheGameSpyBuddyMessageQueue, ("TheGameSpyBuddyMessageQueue exists!") );
	DEBUG_ASSERTCRASH( !TheGameSpyPeerMessageQueue, ("TheGameSpyPeerMessageQueue exists!") );
	DEBUG_ASSERTCRASH( !TheGameSpyInfo, ("TheGameSpyInfo exists!") );
	SetUpGameSpy(MOTDBuffer, configBuffer);

	delete[] MOTDBuffer;
	MOTDBuffer = nullptr;

	delete[] configBuffer;
	configBuffer = nullptr;

#ifdef ALLOW_NON_PROFILED_LOGIN
	UserPreferences pref;
	pref.load("GameSpyLogin.ini");
	UserPreferences::const_iterator it = pref.find("useProfiles");
	if (it != pref.end() && it->second.compareNoCase("yes") == 0)
#endif // ALLOW_NON_PROFILED_LOGIN
		TheShell->push( "Menus/GameSpyLoginProfile.wnd" );
#ifdef ALLOW_NON_PROFILED_LOGIN
	else
		TheShell->push( "Menus/GameSpyLoginQuick.wnd" );
#endif // ALLOW_NON_PROFILED_LOGIN
}

///////////////////////////////////////////////////////////////////////////////////////

static void queuePatch(Bool mandatory, AsciiString downloadURL)
{
	QueuedDownload q;
	Bool success = TRUE;

	AsciiString connectionType;
	success &= downloadURL.nextToken(&connectionType, ":");

	AsciiString server;
	success &= downloadURL.nextToken(&server, ":/");

	AsciiString user;
	success &= downloadURL.nextToken(&user, ":@");

	AsciiString pass;
	success &= downloadURL.nextToken(&pass, "@/");

	AsciiString filePath;
	success &= downloadURL.nextToken(&filePath, "");

	if (!success && user.isNotEmpty())
	{
		// no user/pass combo - move the file into it's proper place
		filePath = user;
		user = ""; // LFeenanEA - Credentials removed as per Security requirements
		pass = "";
		success = TRUE;
	}

	AsciiString fileStr = filePath;
	const char *s = filePath.reverseFind('/');
	if (s)
		fileStr = s+1;
	AsciiString fileName = "patches\\";
	fileName.concat(fileStr);

	DEBUG_LOG(("download URL split: %d [%s] [%s] [%s] [%s] [%s] [%s]",
		success, connectionType.str(), server.str(), user.str(), pass.str(),
		filePath.str(), fileName.str()));

	if (!success)
		return;

	q.file = filePath;
	q.localFile = fileName;
	q.password = pass;
	q.regKey = "";
	q.server = server;
	q.tryResume = TRUE;
	q.userName = user;

	std::list<QueuedDownload>::iterator it = queuedDownloads.begin();
	while (it != queuedDownloads.end())
	{
		if (it->localFile == q.localFile)
			return; // don't add it if it exists already (because we can check multiple times)
		++it;
	}

	queuedDownloads.push_back(q);
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool motdCallback( GHTTPRequest request, GHTTPResult result,
															char * buffer, GHTTPByteCount bufferLen, void * param )
{
	Int run = (uintptr_t)param;
	if (run != timeThroughOnline)
	{
		DEBUG_CRASH(("Old callback being called!"));
		return GHTTPTrue;
	}

	delete[] MOTDBuffer;
	MOTDBuffer = NEW char[bufferLen];
	memcpy(MOTDBuffer, buffer, bufferLen);
	MOTDBuffer[bufferLen-1] = 0;

	--checksLeftBeforeOnline;
	DEBUG_ASSERTCRASH(checksLeftBeforeOnline>=0, ("Too many callbacks"));
	if (onlineCancelWindow && !checksLeftBeforeOnline)
	{
		TheWindowManager->winDestroy(onlineCancelWindow);
		onlineCancelWindow = nullptr;
	}

	DEBUG_LOG(("------- Got MOTD before going online -------"));
	DEBUG_LOG(("%s", (MOTDBuffer)?MOTDBuffer:""));
	DEBUG_LOG(("--------------------------------------------"));

	if (!checksLeftBeforeOnline)
		startOnline();

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool configCallback( GHTTPRequest request, GHTTPResult result,
																char * buffer, GHTTPByteCount bufferLen, void * param )
{
	Int run = (uintptr_t)param;
	if (run != timeThroughOnline)
	{
		DEBUG_CRASH(("Old callback being called!"));
		return GHTTPTrue;
	}

	delete[] configBuffer;
	configBuffer = nullptr;

	if (result != GHTTPSuccess || bufferLen < 100)
	{
		if (!checkingForPatchBeforeGameSpy)
			return GHTTPTrue;
		--checksLeftBeforeOnline;
		if (onlineCancelWindow && !checksLeftBeforeOnline)
		{
			TheWindowManager->winDestroy(onlineCancelWindow);
			onlineCancelWindow = nullptr;
		}
		cantConnectBeforeOnline = TRUE;
		if (!checksLeftBeforeOnline)
		{
			startOnline();
		}
		return GHTTPTrue;
	}

	configBuffer = NEW char[bufferLen];
	memcpy(configBuffer, buffer, bufferLen);
	configBuffer[bufferLen-1] = 0;

	AsciiString fname;
	fname.format("%sGeneralsOnline/Config.txt", TheGlobalData->getPath_UserData().str());
#if RTS_ENGINE_CONTEXT
	FILE *fp = rts::openEngineFile(fname.str(), "wb");
#else
	FILE *fp = fopen(fname.str(), "wb");
#endif
	if (fp)
	{
		fwrite(configBuffer, bufferLen, 1, fp);
#if RTS_ENGINE_CONTEXT
		rts::closeEngineFile(fp);
#else
		fclose(fp);
#endif
	}

	--checksLeftBeforeOnline;
	DEBUG_ASSERTCRASH(checksLeftBeforeOnline>=0, ("Too many callbacks"));
	if (onlineCancelWindow && !checksLeftBeforeOnline)
	{
		TheWindowManager->winDestroy(onlineCancelWindow);
		onlineCancelWindow = nullptr;
	}

	DEBUG_LOG(("Got Config before going online"));

	if (!checksLeftBeforeOnline)
		startOnline();

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool configHeadCallback( GHTTPRequest request, GHTTPResult result,
																		char * buffer, GHTTPByteCount bufferLen, void * param )
{
	Int run = (uintptr_t)param;
	if (run != timeThroughOnline)
	{
		DEBUG_CRASH(("Old callback being called!"));
		return GHTTPTrue;
	}

	DEBUG_LOG(("HTTP head resp: res=%d, len=%d, buf=[%s]", result, bufferLen, buffer));

	if (result == GHTTPSuccess)
	{
		DEBUG_LOG(("Headers are [%s]", ghttpGetHeaders( request )));

		AsciiString headers(ghttpGetHeaders( request ));
		AsciiString line;
		while (headers.nextToken(&line, "\n\r"))
		{
			AsciiString key, val;
			line.nextToken(&key, ": ");
			line.nextToken(&val, ": \r\n");

			if (key.compare("Content-Length") == 0 && val.isNotEmpty())
			{
				Int serverLen = atoi(val.str());
				Int fileLen = 0;
				AsciiString fname;
				fname.format("%sGeneralsOnline/Config.txt", TheGlobalData->getPath_UserData().str());
#if RTS_ENGINE_CONTEXT
				FILE *fp = rts::openEngineFile(fname.str(), "rb");
#else
				FILE *fp = fopen(fname.str(), "rb");
#endif
				if (fp)
				{
					fseek(fp, 0, SEEK_END);
					fileLen = ftell(fp);
#if RTS_ENGINE_CONTEXT
					rts::closeEngineFile(fp);
#else
					fclose(fp);
#endif
				}

				if (serverLen == fileLen)
				{
					// we don't need to download the MOTD again
					--checksLeftBeforeOnline;
					DEBUG_ASSERTCRASH(checksLeftBeforeOnline>=0, ("Too many callbacks"));
					if (onlineCancelWindow && !checksLeftBeforeOnline)
					{
						TheWindowManager->winDestroy(onlineCancelWindow);
						onlineCancelWindow = nullptr;
					}

					delete[] configBuffer;
					configBuffer = nullptr;

					AsciiString fname;
					fname.format("%sGeneralsOnline/Config.txt", TheGlobalData->getPath_UserData().str());
#if RTS_ENGINE_CONTEXT
					FILE *fp = rts::openEngineFile(fname.str(), "rb");
#else
					FILE *fp = fopen(fname.str(), "rb");
#endif
					if (fp)
					{
						configBuffer = NEW char[fileLen];
						fread(configBuffer, fileLen, 1, fp);
						configBuffer[fileLen-1] = 0;
#if RTS_ENGINE_CONTEXT
						rts::closeEngineFile(fp);
#else
						fclose(fp);
#endif

						DEBUG_LOG(("Got Config before going online"));

						if (!checksLeftBeforeOnline)
							startOnline();

						return GHTTPTrue;
					}
				}
			}
		}
	}

	// we need to download the MOTD again
	std::string gameURL, mapURL;
	std::string configURL, motdURL;
	FormatURLFromRegistry(gameURL, mapURL, configURL, motdURL);
	ghttpGet( configURL.c_str(), GHTTPFalse, configCallback, param );

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool gamePatchCheckCallback( GHTTPRequest request, GHTTPResult result, char * buffer, GHTTPByteCount bufferLen, void * param )
{
	Int run = (uintptr_t)param;
	if (run != timeThroughOnline)
	{
		DEBUG_CRASH(("Old callback being called!"));
		return GHTTPTrue;
	}

	--checksLeftBeforeOnline;
	DEBUG_ASSERTCRASH(checksLeftBeforeOnline>=0, ("Too many callbacks"));

	DEBUG_LOG(("Result=%d, buffer=[%s], len=%d", result, buffer, bufferLen));
	if (result != GHTTPSuccess)
	{
		if (!checkingForPatchBeforeGameSpy)
			return GHTTPTrue;
		cantConnectBeforeOnline = TRUE;
		if (!checksLeftBeforeOnline)
		{
			startOnline();
		}
		return GHTTPTrue;
	}

	AsciiString message = buffer;
	AsciiString line;
	while (message.nextToken(&line, "\r\n"))
	{
		AsciiString type, req, url;
		Bool ok = TRUE;
		ok &= line.nextToken(&type, " ");
		ok &= line.nextToken(&req, " ");
		ok &= line.nextToken(&url, " ");
		if (ok && type == "patch")
		{
			DEBUG_LOG(("Saw a patch: %d/[%s]", atoi(req.str()), url.str()));
			queuePatch( atoi(req.str()), url );
			if (atoi(req.str()))
			{
				mustDownloadPatch = TRUE;
			}
		}
		else if (ok && type == "server")
		{
		}
	}

	if (!checksLeftBeforeOnline)
	{
		startOnline();
	}

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CancelPatchCheckCallbackAndReopenDropdown()
{
	HandleCanceledDownload();
	CancelPatchCheckCallback();
}

void CancelPatchCheckCallback()
{
	s_asyncDNSLookupInProgress = FALSE;
	HandleCanceledDownload(FALSE); // don't dropdown
	checkingForPatchBeforeGameSpy = FALSE;
	checksLeftBeforeOnline = 0;
	if (onlineCancelWindow)
	{
		TheWindowManager->winDestroy(onlineCancelWindow);
		onlineCancelWindow = nullptr;
	}
	queuedDownloads.clear();

	delete[] MOTDBuffer;
	MOTDBuffer = nullptr;

	delete[] configBuffer;
	configBuffer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool overallStatsCallback( GHTTPRequest request, GHTTPResult result, char * buffer, GHTTPByteCount bufferLen, void * param )
{
	DEBUG_LOG(("overallStatsCallback() - Result=%d, len=%d", result, bufferLen));
	if (result != GHTTPSuccess)
	{
		return GHTTPTrue;
	}

#if RTS_GENERALS
	OverallStats USA, China, GLA;
	AsciiString message = buffer;

	Int state = STATS_MAX; // STATS_MAX == none
	AsciiString line;
	OverallStats *stats = nullptr;
	while (message.nextToken(&line, "\n"))
	{
		line.trim();
		line.toLower();
		if (strstr(line.str(), "today"))
		{
			state = STATS_TODAY;
		}
		else if (strstr(line.str(), "yesterday"))
		{
			state = STATS_YESTERDAY;
		}
		else if (strstr(line.str(), "all time"))
		{
			state = STATS_ALLTIME;
		}
		else if (strstr(line.str(), "last week"))
		{
			state = STATS_LASTWEEK;
		}
		else if (state != STATS_MAX && strstr(line.str(), "usa"))
		{
			stats = &USA;
		}
		else if (state != STATS_MAX && strstr(line.str(), "china"))
		{
			stats = &China;
		}
		else if (state != STATS_MAX && strstr(line.str(), "gla"))
		{
			stats = &GLA;
		}

		if (stats)
		{
			AsciiString totalLine, winsLine, lossesLine;
			message.nextToken(&totalLine, "\n");
			message.nextToken(&winsLine, "\n");
			message.nextToken(&lossesLine, "\n");
			while (totalLine.isNotEmpty() && !isdigit(totalLine.getCharAt(0)))
			{
				totalLine = totalLine.str()+1;
			}
			while (winsLine.isNotEmpty() && !isdigit(winsLine.getCharAt(0)))
			{
				winsLine = winsLine.str()+1;
			}
			while (lossesLine.isNotEmpty() && !isdigit(lossesLine.getCharAt(0)))
			{
				lossesLine = lossesLine.str()+1;
			}
			if (totalLine.isNotEmpty() && winsLine.isNotEmpty() && lossesLine.isNotEmpty())
			{
				stats->wins[state] = atoi(winsLine.str());
				stats->losses[state] = atoi(lossesLine.str());
			}

			stats = nullptr;
		}
	}

	HandleOverallStats(USA, China, GLA);
#elif RTS_ZEROHOUR
	HandleOverallStats( buffer, bufferLen );
#endif

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

static GHTTPBool numPlayersOnlineCallback( GHTTPRequest request, GHTTPResult result, char * buffer, GHTTPByteCount bufferLen, void * param )
{
	DEBUG_LOG(("numPlayersOnlineCallback() - Result=%d, buffer=[%s], len=%d", result, buffer, bufferLen));
	if (result != GHTTPSuccess)
	{
		return GHTTPTrue;
	}

	AsciiString message = buffer;
	message.trim();
	const char *s = message.reverseFind('\\');
	if (!s)
	{
		return GHTTPTrue;
	}

	if (*s == '\\')
		++s;

	DEBUG_LOG(("Message was '%s', trimmed to '%s'=%d", buffer, s, atoi(s)));
	HandleNumPlayersOnline(atoi(s));

	return GHTTPTrue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CheckOverallStats()
{
#if RTS_GENERALS
	const char *const url = "http://gamestats.gamespy.com/ccgenerals/display.html";
#elif RTS_ZEROHOUR
	const char *const url = "http://gamestats.gamespy.com/ccgenzh/display.html";
#endif
	ghttpGet(url, GHTTPFalse, overallStatsCallback, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////

void CheckNumPlayersOnline()
{
#if RTS_GENERALS
	const char *const url = "http://launch.gamespyarcade.com/software/launch/arcadecount2.dll?svcname=ccgenerals";
#elif RTS_ZEROHOUR
	const char *const url = "http://launch.gamespyarcade.com/software/launch/arcadecount2.dll?svcname=ccgenzh";
#endif
	ghttpGet(url, GHTTPFalse, numPlayersOnlineCallback, nullptr);
}

///////////////////////////////////////////////////////////////////////////////////////

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 15/10/2026 The lookup runs on a thread started through the engine's ThreadStarter
// (Common/EngineThread.h), with its own copy of the host name (the caller's is a local array).
// GeneralsX @feature cemlyn007 16/10/2026 It asks the name servers through the engine's socket layer
// (rts::resolveEngineHostName) instead of gethostbyname, which cannot be interrupted, so a stop request ends the lookup
// within a poll interval; the thread then leaves the results alone.
static void asyncGethostbynameThreadBody( const std::string& name, const std::stop_token& stop )
{
	const std::optional<in_addr> address = rts::resolveEngineHostName( name.c_str(), stop );
	if (stop.stop_requested())
	{
		return;
	}

	s_asyncDNSThreadSucceeded = address ? TRUE : FALSE;
	s_asyncDNSThreadDone = TRUE;
}
#else
DWORD WINAPI asyncGethostbynameThreadFunc( void * szName )
{
	HOSTENT *he = gethostbyname( (const char *)szName );

	if (he)
	{
		s_asyncDNSThreadSucceeded = TRUE;
	}
	else
	{
		s_asyncDNSThreadSucceeded = FALSE;
	}

	s_asyncDNSThreadDone = TRUE;
	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////

int asyncGethostbyname(char * szName)
{
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_STATIC(int, stat, 0);
#else
	static int            stat = 0;
#endif
#if !RTS_ENGINE_CONTEXT
	static unsigned long  threadid;
#endif

	if( stat == 0 )
	{
		/* Kick off gethostname thread */
		s_asyncDNSThreadDone = FALSE;
#if RTS_ENGINE_CONTEXT
		s_asyncDNSThreadHandle = rts::startEngineThread( "async DNS lookup", [name = std::string(szName)]( std::stop_token stop )
		{
			asyncGethostbynameThreadBody( name, stop );
		} );
#else
		s_asyncDNSThreadHandle = CreateThread( nullptr, 0, asyncGethostbynameThreadFunc, szName, 0, &threadid );
#endif

		if( s_asyncDNSThreadHandle == nullptr )
		{
			return( LOOKUP_FAILED );
		}
		stat = 1;
	}
	if( stat == 1 )
	{
		if( s_asyncDNSThreadDone )
		{
			/* Thread finished */
			stat = 0;
			s_asyncDNSLookupInProgress = FALSE;
			s_asyncDNSThreadHandle = nullptr;
			return( (s_asyncDNSThreadSucceeded)?LOOKUP_SUCCEEDED:LOOKUP_FAILED );
		}
	}

	return( LOOKUP_INPROGRESS );
}

///////////////////////////////////////////////////////////////////////////////////////

// GameSpy's HTTP SDK has had at least 1 crash bug, so we're going to just bail and
// never try again if they crash us.  We won't be able to get back online again (we'll
// time out) but at least we'll live.
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(Bool, isHttpOk, TRUE);
#define isHttpOk RTS_ENGINE_GLOBAL_REF(isHttpOk)
#else
static Bool isHttpOk = TRUE;
#endif

void HTTPThinkWrapper()
{
	if (s_asyncDNSLookupInProgress)
	{
		Char hostname[] = "servserv.generals.ea.com";
		Int ret = asyncGethostbyname(hostname);
		switch(ret)
		{
		case LOOKUP_FAILED:
			cantConnectBeforeOnline = TRUE;
			startOnline();
			break;
		case LOOKUP_SUCCEEDED:
			reallyStartPatchCheck();
			break;
		}
	}

	if (isHttpOk)
	{
		try
		{
			ghttpThink();
		}
		catch (...)
		{
			isHttpOk = FALSE; // we can't abort the login, since we might be done with the
												// required checks and are fetching extras.  If it is a required
												// check, we'll time out normally.
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void StopAsyncDNSCheck()
{
	if (s_asyncDNSThreadHandle)
	{
#if RTS_ENGINE_CONTEXT
		// GeneralsX @feature cemlyn007 15/10/2026 A thread is never terminated: it is asked to stop, and leaves the
		// lookup's results alone when its lookup returns
		s_asyncDNSThreadHandle->requestStop();
#else
		MAYBE_UNUSED Int res = TerminateThread(s_asyncDNSThreadHandle, 0);
		(void)res;
		DEBUG_ASSERTCRASH(res, ("Could not terminate the Async DNS Lookup thread!"));	// Thread still not killed!
#endif
	}
	s_asyncDNSThreadHandle = nullptr;
	s_asyncDNSLookupInProgress = FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////

void StartPatchCheck()
{
	checkingForPatchBeforeGameSpy = TRUE;
	cantConnectBeforeOnline = FALSE;
	timeThroughOnline++;
	checksLeftBeforeOnline = 0;

	onlineCancelWindow = MessageBoxCancel(TheGameText->fetch("GUI:CheckingForPatches"),
		TheGameText->fetch("GUI:CheckingForPatches"), CancelPatchCheckCallbackAndReopenDropdown);

	s_asyncDNSLookupInProgress = TRUE;
	Char hostname[] = "servserv.generals.ea.com";
	Int ret = asyncGethostbyname(hostname);
	switch(ret)
	{
	case LOOKUP_FAILED:
		cantConnectBeforeOnline = TRUE;
		startOnline();
		break;
	case LOOKUP_SUCCEEDED:
		reallyStartPatchCheck();
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////////////

static void reallyStartPatchCheck()
{
	checksLeftBeforeOnline = 4;

	std::string gameURL, mapURL;
	std::string configURL, motdURL;

	FormatURLFromRegistry(gameURL, mapURL, configURL, motdURL);

	// check for a patch first
	DEBUG_LOG(("Game patch check: [%s]", gameURL.c_str()));
	DEBUG_LOG(("Map patch check: [%s]", mapURL.c_str()));
	DEBUG_LOG(("Config: [%s]", configURL.c_str()));
	DEBUG_LOG(("MOTD: [%s]", motdURL.c_str()));
	ghttpGet(gameURL.c_str(), GHTTPFalse, gamePatchCheckCallback, reinterpret_cast<void*>(std::uintptr_t(timeThroughOnline)));
	ghttpGet(mapURL.c_str(), GHTTPFalse, gamePatchCheckCallback, reinterpret_cast<void*>(std::uintptr_t(timeThroughOnline)));
	ghttpHead(configURL.c_str(), GHTTPFalse, configHeadCallback, reinterpret_cast<void*>(std::uintptr_t(timeThroughOnline)));
	ghttpGet(motdURL.c_str(), GHTTPFalse, motdCallback, reinterpret_cast<void*>(std::uintptr_t(timeThroughOnline)));

	// check total game stats
	CheckOverallStats();

	// check the users online
	CheckNumPlayersOnline();
}

///////////////////////////////////////////////////////////////////////////////////////
