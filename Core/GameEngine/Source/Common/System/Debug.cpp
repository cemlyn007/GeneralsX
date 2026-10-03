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

// FILE: Debug.cpp
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: Debug.cpp
//
// Created:   Steven Johnson, August 2001
//
// Desc:      Debug logging and other debug utilities
//
// ----------------------------------------------------------------------------

// SYSTEM INCLUDES
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine


// USER INCLUDES

// TheSuperHackers @feature helmutbuhler 04/10/2025
// Uncomment this to show normal logging stuff in the crc logging.
// This can be helpful for context, but can also clutter diffs because normal logs aren't necessarily
// deterministic or the same on all peers in multiplayer games.
//#define INCLUDE_DEBUG_LOG_IN_CRC_LOG

#define DEBUG_THREADSAFE
#ifdef DEBUG_THREADSAFE
#include "Common/CriticalSection.h"
#endif
#include "Common/CommandLine.h"
#include "Common/Debug.h"
#include "Common/FatalEngineError.h"
// GeneralsX @bugfix cemlyn007 02/10/2026 Guard <atomic> the same way WWLib/mutex.h does, so the
// vc6/vc6-debug/vc6-profile presets (which still build this shared Core file) keep working.
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
#include <atomic>
#endif
#include "Common/CRCDebug.h"
#include "Common/UnicodeString.h"
#include "GameClient/ClientInstance.h"
#include "GameClient/GameText.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#if defined(DEBUG_STACKTRACE) || defined(IG_DEBUG_STACKTRACE)
	#include "Common/StackDump.h"
#endif
#ifdef RTS_ENABLE_CRASHDUMP
#include "Common/MiniDumper.h"
#endif
// GeneralsX @build fbraz 24/02/2026 Include thread_compat.h directly to ensure THREAD_ID
// is available on non-Windows builds regardless of the precompiled header chain used
// (Core files may be compiled with different PreRTS.h from GeneralsMD or Generals).
#ifndef _WIN32
#include "thread_compat.h"
#endif

// Horrible reference, but we really, really need to know if we are windowed.
extern bool DX8Wrapper_IsWindowed;
extern HWND ApplicationHWnd;

extern const char *gAppPrefix; /// So WB can have a different log file name.


// ----------------------------------------------------------------------------
// DEFINES
// ----------------------------------------------------------------------------

#ifdef DEBUG_LOGGING

#if defined(RTS_DEBUG)
	#define DEBUG_FILE_NAME				"DebugLogFileD"
	#define DEBUG_FILE_NAME_PREV	"DebugLogFilePrevD"
#else
	#define DEBUG_FILE_NAME				"DebugLogFile"
	#define DEBUG_FILE_NAME_PREV	"DebugLogFilePrev"
#endif

#endif

// ----------------------------------------------------------------------------
// PRIVATE TYPES
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// PRIVATE DATA
// ----------------------------------------------------------------------------
// TheSuperHackers @info Must not use static RAII types when set in DebugInit,
// because DebugInit can be called during static module initialization before the main function is called.
#ifdef DEBUG_LOGGING
static FILE *theLogFile = nullptr;
static char theLogFileName[ _MAX_PATH ];
static char theLogFileNamePrev[ _MAX_PATH ];
#endif
#define LARGE_BUFFER	8192
static char theBuffer[ LARGE_BUFFER ];	// make it big to avoid weird overflow bugs in debug mode
static int theDebugFlags = 0;
// GeneralsX @build fbraz 24/02/2026 On UNIX GetCurrentThreadId() returns THREAD_ID (pthread_t),
// which is a pointer on macOS and unsigned long on Linux — not compatible with DWORD (uint32_t).
#ifdef _WIN32
static DWORD theMainThreadID = 0;
#else
static THREAD_ID theMainThreadID = 0;
#endif
// ----------------------------------------------------------------------------
// PUBLIC DATA
// ----------------------------------------------------------------------------

char* TheCurrentIgnoreCrashPtr = nullptr;
#ifdef DEBUG_LOGGING
UnsignedInt DebugLevelMask = 0;
const char *TheDebugLevels[DEBUG_LEVEL_MAX] = {
	"NET"
};
#endif

// ----------------------------------------------------------------------------
// PRIVATE PROTOTYPES
// ----------------------------------------------------------------------------
static const char *getCurrentTimeString();
static const char *getCurrentTickString();
static void prepBuffer(char *buffer);
#ifdef DEBUG_LOGGING
static void doLogOutput(const char *buffer);
static void doLogOutput(const char *buffer, const char *endline);
#endif
#ifdef DEBUG_CRASHING
static int doCrashBox(const char *buffer, Bool logResult);
#endif
static void whackFunnyCharacters(char *buf);
#ifdef DEBUG_STACKTRACE
static void doStackDump();
#endif

// ----------------------------------------------------------------------------
// PRIVATE FUNCTIONS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
inline Bool ignoringAsserts()
{
	if (!DX8Wrapper_IsWindowed)
		return true;
	if (TheGlobalData && TheGlobalData->m_headless)
		return true;
#ifdef DEBUG_CRASHING
	if (TheGlobalData && TheGlobalData->m_debugIgnoreAsserts)
		return true;
#endif

	return false;
}

// ----------------------------------------------------------------------------
inline HWND getThreadHWND()
{
	return (theMainThreadID == GetCurrentThreadId())?ApplicationHWnd:nullptr;
}

// ----------------------------------------------------------------------------

int MessageBoxWrapper( LPCSTR lpText, LPCSTR lpCaption, UINT uType )
{
	HWND threadHWND = getThreadHWND();
	return ::MessageBox(threadHWND, lpText, lpCaption, uType);
}

// ----------------------------------------------------------------------------
// getCurrentTimeString
/**
	Return the current time in string form
*/
// ----------------------------------------------------------------------------
static const char *getCurrentTimeString()
{
	time_t aclock;
	time(&aclock);
	struct tm *newtime = localtime(&aclock);
	return asctime(newtime);
}

// ----------------------------------------------------------------------------
// getCurrentTickString
/**
	Return the current TickCount in string form
*/
// ----------------------------------------------------------------------------
static const char *getCurrentTickString()
{
	static char TheTickString[32];
	snprintf(TheTickString, ARRAY_SIZE(TheTickString), "(T=%08lx)", ::GetTickCount());
	return TheTickString;
}

// ----------------------------------------------------------------------------
// prepBuffer
// zap the buffer and optionally prepend the tick time.
// ----------------------------------------------------------------------------
/**
	Empty the buffer passed in, then optionally prepend the current TickCount
	value in string form, depending on the setting of theDebugFlags.
*/
static void prepBuffer(char *buffer)
{
	buffer[0] = 0;
#ifdef ALLOW_DEBUG_UTILS
	if (theDebugFlags & DEBUG_FLAG_PREPEND_TIME)
	{
		strcpy(buffer, getCurrentTickString());
		strcat(buffer, " ");
	}
#endif
}

// ----------------------------------------------------------------------------
// doLogOutput
/**
	send a string directly to the log file and/or console without further processing.
*/
// ----------------------------------------------------------------------------
#ifdef DEBUG_LOGGING
static void doLogOutput(const char *buffer)
{
		doLogOutput(buffer, "\n");
}

static void doLogOutput(const char *buffer, const char *endline)
{
	// log message to file
	if (theDebugFlags & DEBUG_FLAG_LOG_TO_FILE)
	{
		if (theLogFile)
		{
			fprintf(theLogFile, "%s%s", buffer, endline);
			fflush(theLogFile);
		}
	}

	// log message to dev studio output window
	if (theDebugFlags & DEBUG_FLAG_LOG_TO_CONSOLE)
	{
		::OutputDebugString(buffer);
		::OutputDebugString(endline);
	}

#ifdef INCLUDE_DEBUG_LOG_IN_CRC_LOG
	addCRCDebugLineNoCounter("%s%s", buffer, endline);
#endif
}
#endif // DEBUG_LOGGING

// ----------------------------------------------------------------------------
// doCrashBox
/*
	present a messagebox with the given message. Depending on user selection,
	we exit the app, break into debugger, or continue execution.
*/
// ----------------------------------------------------------------------------
#ifdef DEBUG_CRASHING
static int doCrashBox(const char *buffer, Bool logResult)
{
	int result;

	if (!ignoringAsserts()) {
		result = MessageBoxWrapper(buffer, "Assertion Failure", MB_ABORTRETRYIGNORE|MB_TASKMODAL|MB_ICONWARNING|MB_DEFBUTTON3);
		//result = MessageBoxWrapper(buffer, "Assertion Failure", MB_ABORTRETRYIGNORE|MB_TASKMODAL|MB_ICONWARNING);
	}	else {
		result = IDIGNORE;
	}

	switch(result)
	{
		case IDABORT:
#ifdef DEBUG_LOGGING
			if (logResult)
				DebugLog("[Abort]");
#endif
			_exit(1);
			break;
		case IDRETRY:
#ifdef DEBUG_LOGGING
			if (logResult)
				DebugLog("[Retry]");
#endif
			::DebugBreak();
			break;
		case IDIGNORE:
#ifdef DEBUG_LOGGING
			// do nothing, just keep going
			if (logResult)
				DebugLog("[Ignore]");
#endif
			break;
	}
	return result;
}
#endif

#ifdef DEBUG_STACKTRACE
// ----------------------------------------------------------------------------
/**
	Dumps a stack trace (from the current PC) to logfile and/or console.
*/
static void doStackDump()
{
	const int STACKTRACE_SIZE	= 24;
	const int STACKTRACE_SKIP = 2;
	void* stacktrace[STACKTRACE_SIZE];

	doLogOutput("\nStack Dump:");
	::FillStackAddresses(stacktrace, STACKTRACE_SIZE, STACKTRACE_SKIP);
	::StackDumpFromAddresses(stacktrace, STACKTRACE_SIZE, doLogOutput);
}
#endif

// ----------------------------------------------------------------------------
// whackFunnyCharacters
/**
	Eliminates any undesirable nonprinting characters, aside from newline,
	replacing them with spaces.
*/
// ----------------------------------------------------------------------------
static void whackFunnyCharacters(char *buf)
{
	for (char *p = buf + strlen(buf) - 1; p >= buf; --p)
	{
		// ok, these are naughty magic numbers, but I'm guessing you know ASCII....
		if (*p >= 0 && *p < 32 && *p != 10 && *p != 13)
			*p = 32;
	}
}

// ----------------------------------------------------------------------------
// PUBLIC FUNCTIONS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// DebugInit
// ----------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_UTILS
/**
	Initialize the debug utilities. This should be called once, as near to the
	start of the app as possible, before anything else (since other code will
	probably want to make use of it).
*/
void DebugInit(int flags)
{
//	if (theDebugFlags != 0)
//		::MessageBox(nullptr, "Debug already inited", "", MB_OK|MB_APPLMODAL);

	// just quietly allow multiple calls to this, so that static ctors can call it.
	if (theDebugFlags == 0)
	{
		theDebugFlags = flags;

		theMainThreadID = GetCurrentThreadId();

	#ifdef DEBUG_LOGGING

		// TheSuperHackers @info Debug initialization can happen very early.
		// Determine the client instance id before creating the log file with an instance specific name.
		CommandLine::parseCommandLineForStartup();

		if (!rts::ClientInstance::initialize())
			return;

		char dirbuf[ _MAX_PATH ];
		::GetModuleFileName( nullptr, dirbuf, sizeof( dirbuf ) );
		if (char *pEnd = strrchr(dirbuf, '\\'))
		{
			*(pEnd + 1) = 0;
		}

		static_assert(ARRAY_SIZE(theLogFileNamePrev) >= ARRAY_SIZE(dirbuf), "Incorrect array size");
		strcpy(theLogFileNamePrev, dirbuf);
		strlcat(theLogFileNamePrev, gAppPrefix, ARRAY_SIZE(theLogFileNamePrev));
		strlcat(theLogFileNamePrev, DEBUG_FILE_NAME_PREV, ARRAY_SIZE(theLogFileNamePrev));
		if (rts::ClientInstance::getInstanceId() > 1u)
		{
			size_t offset = strlen(theLogFileNamePrev);
			snprintf(theLogFileNamePrev + offset, ARRAY_SIZE(theLogFileNamePrev) - offset, "_Instance%.2u", rts::ClientInstance::getInstanceId());
		}
		strlcat(theLogFileNamePrev, ".txt", ARRAY_SIZE(theLogFileNamePrev));

		static_assert(ARRAY_SIZE(theLogFileName) >= ARRAY_SIZE(dirbuf), "Incorrect array size");
		strcpy(theLogFileName, dirbuf);
		strlcat(theLogFileName, gAppPrefix, ARRAY_SIZE(theLogFileNamePrev));
		strlcat(theLogFileName, DEBUG_FILE_NAME, ARRAY_SIZE(theLogFileNamePrev));
		if (rts::ClientInstance::getInstanceId() > 1u)
		{
			size_t offset = strlen(theLogFileName);
			snprintf(theLogFileName + offset, ARRAY_SIZE(theLogFileName) - offset, "_Instance%.2u", rts::ClientInstance::getInstanceId());
		}
		strlcat(theLogFileName, ".txt", ARRAY_SIZE(theLogFileNamePrev));

		remove(theLogFileNamePrev);
		if (rename(theLogFileName, theLogFileNamePrev) != 0)
		{
#ifdef DEBUG_LOGGING
			DebugLog("Warning: Could not rename buffer file '%s' to '%s'. Will remove instead", theLogFileName, theLogFileNamePrev);
#endif
			if (remove(theLogFileName) != 0)
			{
#ifdef DEBUG_LOGGING
				DebugLog("Warning: Failed to remove file '%s'", theLogFileName);
#endif
			}
		}

		theLogFile = fopen(theLogFileName, "w");
		if (theLogFile != nullptr)
		{
			DebugLog("Log %s opened: %s", theLogFileName, getCurrentTimeString());
		}
	#endif
	}

}
#endif

// ----------------------------------------------------------------------------
// DebugLog
// ----------------------------------------------------------------------------
#ifdef DEBUG_LOGGING
/**
	Print a string to the log file and/or console.
*/
void DebugLog(const char *format, ...)
{
#ifdef DEBUG_THREADSAFE
	ScopedCriticalSection scopedCriticalSection(TheDebugLogCriticalSection);
#endif

	if (theDebugFlags == 0)
		MessageBoxWrapper("DebugLog - Debug not inited properly", "", MB_OK|MB_TASKMODAL);

	prepBuffer(theBuffer);

	va_list args;
	va_start(args, format);
	size_t offset = strlen(theBuffer);
	vsnprintf(theBuffer + offset, ARRAY_SIZE(theBuffer) - offset, format, args);
	va_end(args);

	if (strlen(theBuffer) >= sizeof(theBuffer))
		MessageBoxWrapper("String too long for debug buffer", "", MB_OK|MB_TASKMODAL);

	whackFunnyCharacters(theBuffer);
	doLogOutput(theBuffer);
}

/**
	Print a string with no modifications to the log file and/or console.
*/
void DebugLogRaw(const char *format, ...)
{
#ifdef DEBUG_THREADSAFE
	ScopedCriticalSection scopedCriticalSection(TheDebugLogCriticalSection);
#endif

	if (theDebugFlags == 0)
		MessageBoxWrapper("DebugLogRaw - Debug not inited properly", "", MB_OK|MB_TASKMODAL);

	theBuffer[0] = 0;

	va_list args;
	va_start(args, format);
	vsnprintf(theBuffer, ARRAY_SIZE(theBuffer), format, args);
	va_end(args);

	if (strlen(theBuffer) >= sizeof(theBuffer))
		MessageBoxWrapper("String too long for debug buffer", "", MB_OK|MB_TASKMODAL);

	doLogOutput(theBuffer, "");
}

const char* DebugGetLogFileName()
{
	return theLogFileName;
}

const char* DebugGetLogFileNamePrev()
{
	return theLogFileNamePrev;
}

#endif

// ----------------------------------------------------------------------------
// DebugCrash
// ----------------------------------------------------------------------------
#ifdef DEBUG_CRASHING
/**
	Print a character string to the log file and/or console, then halt execution
	while presenting the user with an exit/debug/ignore dialog containing the same
	text message.

	TheSuperHackers @tweak Now shows a message box without any logging when debug was not yet initialized.
*/
void DebugCrash(const char *format, ...)
{
	// Note: You might want to make this thread safe, but we cannot. The reason is that
	// there is an implicit requirement on other threads that the message loop be running.

	// make it not static so that it'll be thread-safe.
	// make it big to avoid weird overflow bugs in debug mode
	char theCrashBuffer[ LARGE_BUFFER ];

	prepBuffer(theCrashBuffer);
	strlcat(theCrashBuffer, "ASSERTION FAILURE: ", ARRAY_SIZE(theCrashBuffer));

	va_list arg;
	va_start(arg, format);
	size_t offset =  strlen(theCrashBuffer);
	vsnprintf(theCrashBuffer + offset, ARRAY_SIZE(theCrashBuffer) - offset, format, arg);
	va_end(arg);

	whackFunnyCharacters(theCrashBuffer);

	const bool useLogging = theDebugFlags != 0;

	if (useLogging)
	{
#ifdef DEBUG_LOGGING
		if (ignoringAsserts())
		{
			doLogOutput("**** CRASH IN FULL SCREEN - Auto-ignored, CHECK THIS LOG!");
		}
		doLogOutput(theCrashBuffer);
#endif
#ifdef DEBUG_STACKTRACE
		if (!(TheGlobalData && TheGlobalData->m_debugIgnoreStackTrace))
		{
			doStackDump();
		}
#endif
	}

	strlcat(theCrashBuffer, "\n\nAbort->exception; Retry->debugger; Ignore->continue", ARRAY_SIZE(theCrashBuffer));

	const int result = doCrashBox(theCrashBuffer, useLogging);

	if (result == IDIGNORE && TheCurrentIgnoreCrashPtr != nullptr)
	{
		int yn;
		if (!ignoringAsserts())
		{
			yn = MessageBoxWrapper("Ignore this crash from now on?", "", MB_YESNO|MB_TASKMODAL);
		}
		else
		{
			yn = IDYES;
		}
		if (yn == IDYES)
			*TheCurrentIgnoreCrashPtr = 1;
		if( TheKeyboard )
			TheKeyboard->resetKeys();
		if( TheMouse )
			TheMouse->reset();
	}

}
#endif

// ----------------------------------------------------------------------------
// DebugShutdown
// ----------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_UTILS
/**
	Shut down the debug utilities. This should be called once, as near to the
	end of the app as possible, after everything else (since other code will
	probably want to make use of it).
*/
void DebugShutdown()
{
#ifdef DEBUG_LOGGING
	if (theLogFile)
	{
		DebugLog("Log closed: %s", getCurrentTimeString());
		fclose(theLogFile);
	}
	theLogFile = nullptr;
#endif
	theDebugFlags = 0;
}

// ----------------------------------------------------------------------------
// DebugGetFlags
// ----------------------------------------------------------------------------
/**
	Get the current values for the flags passed to DebugInit. Most code will never
	need to use this; the most common usage would be to temporarily enable or disable
	the DEBUG_FLAG_PREPEND_TIME bit for complex logfile messages.
*/
int DebugGetFlags()
{
	return theDebugFlags;
}

// ----------------------------------------------------------------------------
// DebugSetFlags
// ----------------------------------------------------------------------------
/**
	Set the current values for the flags passed to DebugInit. Most code will never
	need to use this; the most common usage would be to temporarily enable or disable
	the DEBUG_FLAG_PREPEND_TIME bit for complex logfile messages.
*/
void DebugSetFlags(int flags)
{
	theDebugFlags = flags;
}

#endif	// ALLOW_DEBUG_UTILS

#ifdef DEBUG_PROFILE
// ----------------------------------------------------------------------------
SimpleProfiler::SimpleProfiler()
{
	QueryPerformanceFrequency((LARGE_INTEGER*)&m_freq);
	m_startThisSession = 0;
	m_totalThisSession = 0;
	m_totalAllSessions = 0;
	m_numSessions = 0;
}

// ----------------------------------------------------------------------------
void SimpleProfiler::start()
{
	DEBUG_ASSERTCRASH(m_startThisSession == 0, ("already started"));
	QueryPerformanceCounter((LARGE_INTEGER*)&m_startThisSession);
}

// ----------------------------------------------------------------------------
void SimpleProfiler::stop()
{
	if (m_startThisSession != 0)
	{
		__int64 stop;
		QueryPerformanceCounter((LARGE_INTEGER*)&stop);
		m_totalThisSession = stop - m_startThisSession;
		m_totalAllSessions += stop - m_startThisSession;
		m_startThisSession = 0;
		++m_numSessions;
	}
}

// ----------------------------------------------------------------------------
void SimpleProfiler::stopAndLog(const char *msg, int howOftenToLog, int howOftenToResetAvg)
{
	stop();
	// howOftenToResetAvg==0 means "never reset"
	if (howOftenToResetAvg > 0 && m_numSessions >= howOftenToResetAvg)
	{
		m_numSessions = 0;
		m_totalAllSessions = 0;
		DEBUG_LOG(("%s: reset averages",msg));
	}
	DEBUG_ASSERTLOG(m_numSessions % howOftenToLog != 0, ("%s: %f msec, total %f msec, avg %f msec",msg,getTime(),getTotalTime(),getAverageTime()));
}

// ----------------------------------------------------------------------------
double SimpleProfiler::getTime()
{
	stop();
	return (double)m_totalThisSession / (double)m_freq * 1000.0;
}

// ----------------------------------------------------------------------------
int SimpleProfiler::getNumSessions()
{
	stop();
	return m_numSessions;
}

// ----------------------------------------------------------------------------
double SimpleProfiler::getTotalTime()
{
	stop();
	if (!m_numSessions)
		return 0.0;

	return (double)m_totalAllSessions * 1000.0 / ((double)m_freq);
}

// ----------------------------------------------------------------------------
double SimpleProfiler::getAverageTime()
{
	stop();
	if (!m_numSessions)
		return 0.0;

	return (double)m_totalAllSessions * 1000.0 / ((double)m_freq * (double)m_numSessions);
}

#endif	// DEBUG_PROFILE

// ----------------------------------------------------------------------------
// ReleaseCrash
// ----------------------------------------------------------------------------
/**
	Halt the application, EVEN IN FINAL RELEASE BUILDS. This should be called
	only when a crash is guaranteed by continuing, and no meaningful continuation
	of processing is possible, even by throwing an exception.
*/

	#define RELEASECRASH_FILE_NAME				"ReleaseCrashInfo.txt"
	#define RELEASECRASH_FILE_NAME_PREV		"ReleaseCrashInfoPrev.txt"

	static FILE *theReleaseCrashLogFile = nullptr;

	static void releaseCrashLogOutput(const char *buffer)
	{
		if (theReleaseCrashLogFile)
		{
			fprintf(theReleaseCrashLogFile, "%s\n", buffer);
			fflush(theReleaseCrashLogFile);
		}
	}


static void TriggerMiniDump()
{
#ifdef RTS_ENABLE_CRASHDUMP
	if (TheMiniDumper && TheMiniDumper->IsInitialized())
	{
		// Create both minimal and full memory dumps
		TheMiniDumper->TriggerMiniDump(DumpType_Minimal);
		TheMiniDumper->TriggerMiniDump(DumpType_Full);
	}

	MiniDumper::shutdownMiniDumper();
#endif
}


// GeneralsX @feature cemlyn007 28/09/2026 Embedded mode (see FatalEngineError.h)
// GeneralsX @bugfix cemlyn007 03/10/2026 Pre-C++11/VC6 builds have no std::atomic<bool> (see the
// <atomic> guard above); a plain volatile Bool is the same fallback WWLib/mutex.h uses for its
// FastCriticalSectionClass flag on those presets. The only host that ever stores true,
// rlgenerals' launcher, does so from its own single boot/engine thread: once before init() and
// again, storing false, before a normal teardown (ReleaseCrash reads the flag up to twice per
// crash and ReleaseCrashLocalized up to three times, and the host's own test hooks read it on
// every init()/loadGame(), but always from that same thread). A VC6/retail build links no
// embedded host at all, so this flag there is read-only in practice (always false); the
// volatile bool fallback exists so such presets still link, not because they observe the flag
// from a second thread.
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
static std::atomic<bool> theEngineEmbeddedMode(false);
#else
static volatile bool theEngineEmbeddedMode = false;
#endif

// GeneralsX @bugfix cemlyn007 02/10/2026 `= default` is C++11; VC6 cannot parse it, so give the
// destructor an explicit empty body instead (see the <atomic> guard above for the same reasoning).
FatalEngineError::~FatalEngineError() {}

void SetEngineEmbeddedMode(bool embedded)
{
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
	theEngineEmbeddedMode.store(embedded);
#else
	theEngineEmbeddedMode = embedded;
#endif
}

bool IsEngineEmbeddedMode()
{
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
	return theEngineEmbeddedMode.load();
#else
	return theEngineEmbeddedMode;
#endif
}

// GeneralsX @feature cemlyn007 02/10/2026 Sticky fault latch (see FatalEngineError.h): set before
// the throw below, so a catch (...) that swallows the exception anyway still leaves a trace a
// polling host can find. Same VC6/pre-C++11 fallback as theEngineEmbeddedMode above.
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
static std::atomic<bool> theEngineHasFaulted(false);
#else
static volatile bool theEngineHasFaulted = false;
#endif

bool HasEngineFaulted()
{
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
	return theEngineHasFaulted.load();
#else
	return theEngineHasFaulted;
#endif
}

void ClearEngineFault()
{
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
	theEngineHasFaulted.store(false);
#else
	theEngineHasFaulted = false;
#endif
}

// GeneralsX @bugfix cemlyn007 02/10/2026 Single place for the embedded-mode throw, used by both
// ReleaseCrash and ReleaseCrashLocalized's two throw sites each, so a future change (for example
// the sticky fault latch above) only needs to touch one function instead of drifting across four
// copies.
static void ThrowIfEmbedded(const char *reason)
{
	if (IsEngineEmbeddedMode()) {
#if !(defined(_MSC_VER) && _MSC_VER < 1300)
		theEngineHasFaulted.store(true);
#else
		theEngineHasFaulted = true;
#endif
		throw FatalEngineError(reason ? reason : "");
	}
}

void ReleaseCrash(const char *reason)
{
	/// do additional reporting on the crash, if possible

	// GeneralsX @bugfix cemlyn007 02/10/2026 Embedded mode: skip the process-wide UI and minidump
	// side effects (ShowWindow, TriggerMiniDump -> MiniDumper::shutdownMiniDumper for the whole
	// process) entirely. A host embedding this engine keeps other engines and the process running
	// after the throw below, so hiding the app window or tearing down the process-wide minidumper
	// here would affect them too.
	if (!IsEngineEmbeddedMode()) {
		if (!DX8Wrapper_IsWindowed) {
			if (ApplicationHWnd) {
				ShowWindow(ApplicationHWnd, SW_HIDE);
			}
		}

		TriggerMiniDump();
	}

	char prevbuf[ _MAX_PATH ];
	char curbuf[ _MAX_PATH ];

	if (TheGlobalData==nullptr) {
		// GeneralsX @bugfix cemlyn007 28/09/2026 Embedded mode: there is no crash file to write, but
		// the caller still must not carry on, so hand the error to the host (see FatalEngineError.h).
		ThrowIfEmbedded(reason);
		return; // We are shutting down, and TheGlobalData has been freed.  jba. [4/15/2003]
	}

	strlcpy(prevbuf, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(prevbuf));
	strlcat(prevbuf, RELEASECRASH_FILE_NAME_PREV, ARRAY_SIZE(prevbuf));
	strlcpy(curbuf, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(curbuf));
	strlcat(curbuf, RELEASECRASH_FILE_NAME, ARRAY_SIZE(curbuf));

 	remove(prevbuf);
	if (rename(curbuf, prevbuf) != 0)
	{
#ifdef DEBUG_LOGGING
		DebugLog("Warning: Could not rename buffer file '%s' to '%s'. Will remove instead", curbuf, prevbuf);
#endif
		if (remove(curbuf) != 0)
		{
#ifdef DEBUG_LOGGING
			DebugLog("Warning: Failed to remove file '%s'", curbuf);
#endif
		}
	}

	theReleaseCrashLogFile = fopen(curbuf, "w");
	if (theReleaseCrashLogFile)
	{
		fprintf(theReleaseCrashLogFile, "Release Crash at %s; Reason %s\n", getCurrentTimeString(), reason);
		fprintf(theReleaseCrashLogFile, "\nLast error:\n%s\n\nCurrent stack:\n", g_LastErrorDump.str());
		const int STACKTRACE_SIZE	= 12;
		const int STACKTRACE_SKIP = 6;
		void* stacktrace[STACKTRACE_SIZE];
		::FillStackAddresses(stacktrace, STACKTRACE_SIZE, STACKTRACE_SKIP);
		::StackDumpFromAddresses(stacktrace, STACKTRACE_SIZE, releaseCrashLogOutput);

		fflush(theReleaseCrashLogFile);
		fclose(theReleaseCrashLogFile);
		theReleaseCrashLogFile = nullptr;
	}

	// GeneralsX @feature cemlyn007 28/09/2026 Embedded mode: hand the error to the host instead of
	// showing a message box and exiting the process.
	ThrowIfEmbedded(reason);

	if (!DX8Wrapper_IsWindowed) {
		if (ApplicationHWnd) {
			ShowWindow(ApplicationHWnd, SW_HIDE);
		}
	}

#if defined(RTS_DEBUG)
	/* static */ char buff[8192]; // not so static so we can be threadsafe
	snprintf(buff, 8192, "Sorry, a serious error occurred. (%s)", reason);
	if (!(TheGlobalData && TheGlobalData->m_headless))
	{
		::MessageBox(nullptr, buff, "Technical Difficulties...", MB_OK|MB_SYSTEMMODAL|MB_ICONERROR);
	}
#else
// crash error messaged changed 3/6/03 BGC
//	::MessageBox(nullptr, "Sorry, a serious error occurred.", "Technical Difficulties...", MB_OK|MB_TASKMODAL|MB_ICONERROR);
//	::MessageBox(nullptr, "You have encountered a serious error.  Serious errors can be caused by many things including viruses, overheated hardware and hardware that does not meet the minimum specifications for the game. Please visit the forums at www.generals.ea.com for suggested courses of action or consult your manual for Technical Support contact information.", "Technical Difficulties...", MB_OK|MB_TASKMODAL|MB_ICONERROR);

// crash error message changed again 8/22/03 M Lorenzen... made this message box modal to the system so it will appear on top of any task-modal windows, splash-screen, etc.
	if (!(TheGlobalData && TheGlobalData->m_headless))
	{
		::MessageBox(nullptr, "You have encountered a serious error.  Serious errors can be caused by many things including viruses, overheated hardware and hardware that does not meet the minimum specifications for the game. Please visit the forums at www.generals.ea.com for suggested courses of action or consult your manual for Technical Support contact information.",
			"Technical Difficulties...",
			MB_OK|MB_SYSTEMMODAL|MB_ICONERROR);
	}


#endif

	_exit(1);
}

void ReleaseCrashLocalized(const AsciiString& p, const AsciiString& m)
{
	if (!TheGameText) {
		ReleaseCrash(m.str());
		// This won't ever return
		return;
	}

	// GeneralsX @bugfix cemlyn007 03/10/2026 Take the minidump here, before the two
	// TheGameText->fetch() calls and the translate() below, not after: those can themselves fault
	// on corrupt state (a damaged string table, heap corruption), and the dump exists to capture
	// state at the point of the original fatal error, not whatever is left once two more lookups
	// and a string translation have run on top of it. This restores ReleaseCrash's own ordering
	// and main's; only the MessageBox/ShowWindow/stderr block below needs the fetched text, so that
	// part alone stays after the fetches, inside its own embedded-mode check.
	if (!IsEngineEmbeddedMode()) {
		TriggerMiniDump();
	}

	UnicodeString prompt = TheGameText->fetch(p);
	UnicodeString mesg = TheGameText->fetch(m);

	AsciiString reason;
	reason.translate(mesg);

	/// do additional reporting on the crash, if possible

	// GeneralsX @bugfix cemlyn007 02/10/2026 Embedded mode: skip the process-wide UI side effects
	// entirely, same reasoning as ReleaseCrash above (the minidump itself is taken above, before
	// the fetches, which embedded mode also skips).
	if (!IsEngineEmbeddedMode()) {
		// GeneralsX @build BenderAI 12/02/2026 Platform-specific crash reporting
		// Windows: Native MessageBox dialogs
		// Linux: Console output (crash dialogs would need SDL_ShowSimpleMessageBox)
		#ifdef _WIN32
		extern const Bool TheSystemIsUnicode;

			if (!DX8Wrapper_IsWindowed) {
			if (ApplicationHWnd) {
				ShowWindow(ApplicationHWnd, SW_HIDE);
			}
		}

		if (!(TheGlobalData && TheGlobalData->m_headless))
		{
			if (TheSystemIsUnicode)
			{
				::MessageBoxW(nullptr, mesg.str(), prompt.str(), MB_OK|MB_SYSTEMMODAL|MB_ICONERROR);
			}
			else
			{
				// However, if we're using the default version of the message box, we need to
				// translate the string into an AsciiString
				AsciiString promptA, mesgA;
				promptA.translate(prompt);
				mesgA.translate(mesg);
				//Make sure main window is not TOP_MOST
				::SetWindowPos(ApplicationHWnd, HWND_NOTOPMOST, 0, 0, 0, 0,SWP_NOSIZE |SWP_NOMOVE);
				::MessageBoxA(nullptr, mesgA.str(), promptA.str(), MB_OK|MB_TASKMODAL|MB_ICONERROR);
			}
		}
		#else
		// Linux: Output to stderr (game will crash anyway after this)
		fprintf(stderr, "FATAL ERROR: %s\n%s\n", prompt.str(), mesg.str());
		#endif
	}

	// GeneralsX @bugfix cemlyn007 28/09/2026 Embedded mode: with no TheGlobalData there is no crash
	// file path, so hand the error straight to the host (see FatalEngineError.h).
	if (TheGlobalData == nullptr) {
		ThrowIfEmbedded(reason.str());
	}

	char prevbuf[ _MAX_PATH ];
	char curbuf[ _MAX_PATH ];

	strlcpy(prevbuf, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(prevbuf));
	strlcat(prevbuf, RELEASECRASH_FILE_NAME_PREV, ARRAY_SIZE(prevbuf));
	strlcpy(curbuf, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(curbuf));
	strlcat(curbuf, RELEASECRASH_FILE_NAME, ARRAY_SIZE(curbuf));

 	remove(prevbuf);
	if (rename(curbuf, prevbuf) != 0)
	{
#ifdef DEBUG_LOGGING
		DebugLog("Warning: Could not rename buffer file '%s' to '%s'. Will remove instead", curbuf, prevbuf);
#endif
		if (remove(curbuf) != 0)
		{
#ifdef DEBUG_LOGGING
			DebugLog("Warning: Failed to remove file '%s'", curbuf);
#endif
		}
	}

	theReleaseCrashLogFile = fopen(curbuf, "w");
	if (theReleaseCrashLogFile)
	{
		fprintf(theReleaseCrashLogFile, "Release Crash at %s; Reason %ls\n", getCurrentTimeString(), mesg.str());

		const int STACKTRACE_SIZE	= 12;
		const int STACKTRACE_SKIP = 6;
		void* stacktrace[STACKTRACE_SIZE];
		::FillStackAddresses(stacktrace, STACKTRACE_SIZE, STACKTRACE_SKIP);
		::StackDumpFromAddresses(stacktrace, STACKTRACE_SIZE, releaseCrashLogOutput);

		fflush(theReleaseCrashLogFile);
		fclose(theReleaseCrashLogFile);
		theReleaseCrashLogFile = nullptr;
	}

	// GeneralsX @feature cemlyn007 28/09/2026 Embedded mode: hand the error to the host instead of
	// exiting the process (see FatalEngineError.h).
	ThrowIfEmbedded(reason.str());

	_exit(1);
}
