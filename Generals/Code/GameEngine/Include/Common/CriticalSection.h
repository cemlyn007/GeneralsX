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

// CriticalSection.h ///////////////////////////////////////////////////////
// Utility class to use critical sections in areas of code.
// Author: JohnM And MattC, August 13, 2002

#pragma once

#include "Common/PerfTimer.h"

// TheSuperHackers @build felipebraz 10/02/2026 Phase 1.5
// Include threads_compat for Linux CRITICAL_SECTION support
#ifdef _UNIX
#include "threads_compat.h"
#endif

#ifdef PERF_TIMERS
extern PerfGather TheCritSecPerfGather;
#endif

class CriticalSection
{
	CRITICAL_SECTION m_windowsCriticalSection;

	public:
		CriticalSection()
		{
			#ifdef PERF_TIMERS
			AutoPerfGather a(TheCritSecPerfGather);
			#endif
			InitializeCriticalSection( &m_windowsCriticalSection );
		}

		virtual ~CriticalSection()
		{
			#ifdef PERF_TIMERS
			AutoPerfGather a(TheCritSecPerfGather);
			#endif
			DeleteCriticalSection( &m_windowsCriticalSection );
		}

	public:	// Use these when entering/exiting a critical section.
		void enter()
		{
			#ifdef PERF_TIMERS
			AutoPerfGather a(TheCritSecPerfGather);
			#endif
			EnterCriticalSection( &m_windowsCriticalSection );
		}

		void exit()
		{
			#ifdef PERF_TIMERS
			AutoPerfGather a(TheCritSecPerfGather);
			#endif
			LeaveCriticalSection( &m_windowsCriticalSection );
		}
};

class ScopedCriticalSection
{
	private:
		CriticalSection *m_cs;

	public:
		ScopedCriticalSection( CriticalSection *cs ) : m_cs(cs)
		{
			if (m_cs)
				m_cs->enter();
		}

		virtual ~ScopedCriticalSection()
		{
			if (m_cs)
				m_cs->exit();
		}
};

// These should be null on creation then non-null in WinMain or equivalent.
// This allows us to be silently non-threadsafe for WB and other single-threaded apps.
// GeneralsX @fix cemlyn007 03/10/2026 That "silently non-threadsafe" remark no longer applies to
// TheAsciiStringCriticalSection/TheUnicodeStringCriticalSection: AsciiString and UnicodeString stopped
// reading them, so null or non-null makes no difference to either type any more. It still applies to
// TheDmaCriticalSection/TheMemoryPoolCriticalSection/TheDebugLogCriticalSection below. WinMain/SDL3Main
// still install the string sections, unused, so the host code and upstream merges are unchanged.
// GeneralsX @performance cemlyn007 29/09/2026 AsciiString's and UnicodeString's buffer reference counts
// are atomic, so a buffer shared between distinct string copies is safe to free without either section.
// That does not extend to one string object written by more than one thread: a process-global static
// AsciiString/UnicodeString must not be assigned or mutated after the first engine's boot (priming) instead
// (PLAN-023 Phase 5b item 2).
extern CriticalSection *TheAsciiStringCriticalSection;
extern CriticalSection *TheUnicodeStringCriticalSection;
extern CriticalSection *TheDmaCriticalSection;
extern CriticalSection *TheMemoryPoolCriticalSection;
extern CriticalSection *TheDebugLogCriticalSection;
