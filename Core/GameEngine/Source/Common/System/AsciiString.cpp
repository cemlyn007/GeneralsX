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

// FILE: AsciiString.cpp
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  AsciiString.cpp
//
// Created:    Steven Johnson, October 2001
//
// Desc:       General-purpose string classes
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "WWLib/utf8.h"


// -----------------------------------------------------

/*static*/ const AsciiString AsciiString::TheEmptyString;

namespace
{

//-----------------------------------------------------------------------------
inline char* skipSeps(char* p, const char* seps)
{
	while (*p && strchr(seps, *p) != nullptr)
		++p;
	return p;
}

//-----------------------------------------------------------------------------
inline char* skipNonSeps(char* p, const char* seps)
{
	while (*p && strchr(seps, *p) == nullptr)
		++p;
	return p;
}

//-----------------------------------------------------------------------------
inline char* skipWhitespace(char* p)
{
	while (*p && isspace(*p))
		++p;
	return p;
}

//-----------------------------------------------------------------------------
inline char* skipNonWhitespace(char* p)
{
	while (*p && !isspace(*p))
		++p;
	return p;
}


//-----------------------------------------------------------------------------
struct StringCaseInfo
{
	StringCaseInfo()
		: length(0)
		, lowercaseCount(0)
		, uppercaseCount(0)
	{}

	size_t length;
	size_t lowercaseCount;
	size_t uppercaseCount;
};

static StringCaseInfo getStringCaseInfo(const char *str)
{
	StringCaseInfo info;
	const char* begin = str;
	while (*str)
	{
		info.lowercaseCount += (size_t)(bool)islower((unsigned char)*str);
		info.uppercaseCount += (size_t)(bool)isupper((unsigned char)*str);
		++str;
	}
	info.length = static_cast<size_t>(str - begin);
	return info;
}

} // namespace

// -----------------------------------------------------
AsciiString::AsciiString(const AsciiString& stringSrc) : m_data(stringSrc.m_data)
{
	// GeneralsX @performance cemlyn007 29/09/2026 No global lock: stringSrc keeps its reference while we take
	// ours, so the count cannot reach zero underneath us and a relaxed increment is enough.
	if (m_data)
		m_data->m_refCount.fetch_add(1, std::memory_order_relaxed);
	validate();
}

// -----------------------------------------------------
#ifdef RTS_DEBUG
void AsciiString::validate() const
{
	if (!m_data)
		return;
	DEBUG_ASSERTCRASH(m_data->m_refCount.load(std::memory_order_relaxed) > 0, ("m_refCount is zero"));
	DEBUG_ASSERTCRASH(m_data->m_refCount.load(std::memory_order_relaxed) < 32000, ("m_refCount is suspiciously large"));
	DEBUG_ASSERTCRASH(m_data->m_numCharsAllocated > 0, ("m_numCharsAllocated is zero"));
//	DEBUG_ASSERTCRASH(m_data->m_numCharsAllocated < 1024, ("m_numCharsAllocated suspiciously large"));
	DEBUG_ASSERTCRASH(strlen(m_data->peek())+1 <= m_data->m_numCharsAllocated,("str is too long (%d) for storage",strlen(m_data->peek())+1));
}
#endif

// -----------------------------------------------------
void AsciiString::debugIgnoreLeaks()
{
#ifdef MEMORYPOOL_DEBUG
	if (m_data)
	{
		TheDynamicMemoryAllocator->debugIgnoreLeaksForThisBlock(m_data);
	}
	else
	{
		DEBUG_LOG(("cannot ignore the leak (no data)"));
	}
#endif
}

// -----------------------------------------------------
void AsciiString::ensureUniqueBufferOfSize(int numCharsNeeded, Bool preserveData, const char* strToCopy, const char* strToCat)
{
	validate();

	const int usableNumChars = numCharsNeeded - 1;

	// GeneralsX @performance cemlyn007 29/09/2026 Copy-on-write safety without the global lock: a buffer is
	// written in place only when this string holds the sole reference. That count protects a buffer shared
	// between distinct AsciiString copies; it says nothing about one AsciiString object written by more than
	// one thread, which this class never made safe (the old global lock just serialised those writes too). A
	// process-global static AsciiString is safe to read from any engine's thread only because nothing mutates
	// it after boot (PLAN-023 Phase 5b item 2), not because of its refcount. The acquire load pairs with the
	// acq_rel decrement in releaseBuffer(), so every read the last other holder made of this buffer happens
	// before our in-place write.
	if (m_data &&
			m_data->m_refCount.load(std::memory_order_acquire) == 1 &&
			m_data->m_numCharsAllocated >= numCharsNeeded)
	{
		// no buffer manhandling is needed (it's already large enough, and unique to us)
		if (strToCopy)
		{
			// TheSuperHackers @fix Mauller 04/04/2025 Replace strcpy with safer memmove as memory regions can overlap when part of string is copied to itself
			DEBUG_ASSERTCRASH(usableNumChars <= strlen(strToCopy), ("strToCopy is too small"));
			memmove(m_data->peek(), strToCopy, usableNumChars);
			m_data->peek()[usableNumChars] = 0;
		}
		if (strToCat)
			strcat(m_data->peek(), strToCat);
		return;
	}

	DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("Cannot use dynamic memory allocator before its initialization. Check static initialization order."));
	DEBUG_ASSERTCRASH(numCharsNeeded <= MAX_LEN, ("AsciiString::ensureUniqueBufferOfSize exceeds max string length %d with requested length %d", MAX_LEN, numCharsNeeded));
	int minBytes = sizeof(AsciiStringData) + numCharsNeeded*sizeof(char);
	int actualBytes = TheDynamicMemoryAllocator->getActualAllocationSize(minBytes);
	AsciiStringData* newData = (AsciiStringData*)TheDynamicMemoryAllocator->allocateBytesDoNotZero(actualBytes, "STR_AsciiString::ensureUniqueBufferOfSize");
	newData->m_refCount.store(1, std::memory_order_relaxed);
	newData->m_numCharsAllocated = (actualBytes - sizeof(AsciiStringData))/sizeof(char);
#if defined(RTS_DEBUG)
	newData->m_debugptr = newData->peek();	// just makes it easier to read in the debugger
#endif

	if (m_data && preserveData)
		strcpy(newData->peek(), m_data->peek());
	else
		newData->peek()[0] = 0;

	// do these BEFORE releasing the old buffer, so that self-copies
	// or self-cats will work correctly.
	if (strToCopy)
	{
		DEBUG_ASSERTCRASH(usableNumChars <= strlen(strToCopy), ("strToCopy is too small"));
		strncpy(newData->peek(), strToCopy, usableNumChars);
		newData->peek()[usableNumChars] = 0;
	}
	if (strToCat)
		strcat(newData->peek(), strToCat);

	releaseBuffer();
	m_data = newData;

	validate();
}


// -----------------------------------------------------
void AsciiString::releaseBuffer()
{
	validate();
	if (m_data)
	{
		// GeneralsX @performance cemlyn007 29/09/2026 acq_rel decrement, freeing only on the 1 -> 0 transition:
		// the release half publishes our reads of the buffer to whoever frees or reuses it, and the acquire
		// half orders the free after every other holder's release.
		if (m_data->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			TheDynamicMemoryAllocator->freeBytes(m_data);
		}
		m_data = nullptr;
	}
	validate();
}

// -----------------------------------------------------
AsciiString::AsciiString(const char* s) : m_data(nullptr)
{
	//DEBUG_ASSERTCRASH(isMemoryManagerOfficiallyInited(), ("Initializing AsciiStrings prior to main (ie, as static vars) can cause memory leak reporting problems. Are you sure you want to do this?"));
	int len = s ? (int)strlen(s) : 0;
	if (len > 0)
	{
		ensureUniqueBufferOfSize(len + 1, false, s, nullptr);
	}
	validate();
}

// -----------------------------------------------------
AsciiString::AsciiString(const char* s, int len) : m_data(nullptr)
{
	if (len > 0)
	{
		ensureUniqueBufferOfSize(len + 1, false, s, nullptr);
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::set(const AsciiString& stringSrc)
{
	validate();
	if (&stringSrc != this)
	{
		// GeneralsX @performance cemlyn007 29/09/2026 No global lock: take the new reference (relaxed, as in the
		// copy constructor) before dropping the old one, so a shared buffer's count never passes through a
		// false 1 on the way.
		AsciiStringData* newData = stringSrc.m_data;
		if (newData)
			newData->m_refCount.fetch_add(1, std::memory_order_relaxed);
		releaseBuffer();
		m_data = newData;
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::set(const char* s)
{
	int len = s ? strlen(s) : 0;
	set(s, len);
}

// -----------------------------------------------------
void AsciiString::set(const char* s, int len)
{
	validate();
	if (!m_data || s != peek())
	{
		if (len > 0)
		{
			ensureUniqueBufferOfSize(len + 1, false, s, nullptr);
		}
		else
		{
			releaseBuffer();
		}
	}
	validate();
}

// -----------------------------------------------------
char*  AsciiString::getBufferForRead(Int len)
{
	validate();
	DEBUG_ASSERTCRASH(len>0, ("No need to allocate 0 len strings."));
	ensureUniqueBufferOfSize(len + 1, false, nullptr, nullptr);
	validate();
	return peek();
}

// -----------------------------------------------------
void AsciiString::translate(const UnicodeString& stringSrc)
{
	validate();
	// GeneralsX @feature bobtista 28/08/2026 Import UTF-8 conversion in place of the 7-bit-only implementation.
	// Upstream PR: https://github.com/TheSuperHackers/GeneralsGameCode/pull/2528
	const WideChar* src = stringSrc.str();
	const size_t srcLen = wcslen(src);
	const size_t dstLen = Wide_To_Utf8_Len(src, srcLen);
	if (dstLen == 0)
	{
		clear();
	}
	else
	{
		ensureUniqueBufferOfSize((Int)dstLen + 1, false, nullptr, nullptr);
		Wide_To_Utf8(peek(), dstLen + 1, src, srcLen);
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::concat(const char* s)
{
	validate();
	int addlen = strlen(s);
	if (addlen == 0)
		return;	// my, that was easy

	if (m_data)
	{
		ensureUniqueBufferOfSize(getLength() + addlen + 1, true, nullptr, s);
	}
	else
	{
		set(s);
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::trim()
{
	validate();

	if (m_data)
	{
		char *c = peek();

		//	Strip leading white space from the string.
		c = skipWhitespace(c);
		if (c != peek())
		{
			set(c);
		}

		trimEnd();
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::trimEnd()
{
	validate();

	if (m_data)
	{
		// Clip trailing white space from the string.
		const int len = strlen(peek());
		int index = len;
		while (index > 0 && isspace(getCharAt(index - 1)))
		{
			--index;
		}

		if (index < len)
		{
			truncateTo(index);
		}
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::trimEnd(const char c)
{
	validate();

	if (m_data)
	{
		// Clip trailing consecutive occurrences of c from the string.
		const int len = strlen(peek());
		int index = len;
		while (index > 0 && getCharAt(index - 1) == c)
		{
			--index;
		}

		if (index < len)
		{
			truncateTo(index);
		}
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::toLower()
{
	validate();

	if (m_data == nullptr)
		return;

	const StringCaseInfo info = getStringCaseInfo(m_data->peek());

	if (info.uppercaseCount == 0)
		return;

	ensureUniqueBufferOfSize(info.length + 1, true, nullptr, nullptr);

	char* str = m_data->peek();
	while (*str)
	{
		*str = tolower((unsigned char)*str);
		++str;
	}

	validate();
}

// -----------------------------------------------------
void AsciiString::toUpper()
{
	validate();

	if (m_data == nullptr)
		return;

	const StringCaseInfo info = getStringCaseInfo(m_data->peek());

	if (info.lowercaseCount == 0)
		return;

	ensureUniqueBufferOfSize(info.length + 1, true, nullptr, nullptr);

	char* str = m_data->peek();
	while (*str)
	{
		*str = toupper((unsigned char)*str);
		++str;
	}

	validate();
}

// -----------------------------------------------------
void AsciiString::removeLastChar()
{
	truncateBy(1);
}

// -----------------------------------------------------
void AsciiString::truncateBy(const Int charCount)
{
	validate();
	if (m_data && charCount > 0)
	{
		const size_t len = strlen(peek());
		if (len > 0)
		{
			ensureUniqueBufferOfSize(len + 1, true, nullptr, nullptr);
			size_t count = charCount;
			if (charCount > len)
			{
				count = len;
			}
			peek()[len - count] = 0;
		}
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::truncateTo(const Int maxLength)
{
	validate();
	if (m_data)
	{
		const size_t len = strlen(peek());
		if (len > maxLength)
		{
			ensureUniqueBufferOfSize(len + 1, true, nullptr, nullptr);
			peek()[maxLength] = 0;
		}
	}
	validate();
}

// -----------------------------------------------------
void AsciiString::format(AsciiString format, ...)
{
	validate();
	va_list args;
  va_start(args, format);
	format_va(format, args);
  va_end(args);
	validate();
}

// -----------------------------------------------------
void AsciiString::format(const char* format, ...)
{
	validate();
	va_list args;
  va_start(args, format);
	format_va(format, args);
  va_end(args);
	validate();
}

// -----------------------------------------------------
void AsciiString::format_va(const AsciiString& format, va_list args)
{
	format_va(format.str(), args);
}

// -----------------------------------------------------
void AsciiString::format_va(const char* format, va_list args)
{
	validate();
	char buf[MAX_FORMAT_BUF_LEN];
	const int result = vsnprintf(buf, sizeof(buf)/sizeof(char), format, args);
	if (result >= 0)
	{
		set(buf);
		validate();
	}
	else
	{
		DEBUG_CRASH(("AsciiString::format_va failed with code:%d format:\"%s\"", result, format));
	}
}

// -----------------------------------------------------
Bool AsciiString::startsWith(const char* p) const
{
	return m_data && ::startsWith(peek(), p);
}

// -----------------------------------------------------
Bool AsciiString::startsWithNoCase(const char* p) const
{
	return m_data && ::startsWithNoCase(peek(), p);
}

// -----------------------------------------------------
Bool AsciiString::endsWith(const char* p) const
{
	return m_data && ::endsWith(peek(), p);
}

// -----------------------------------------------------
Bool AsciiString::endsWithNoCase(const char* p) const
{
	return m_data && ::endsWithNoCase(peek(), p);
}

//-----------------------------------------------------------------------------
Bool AsciiString::isNone() const
{
	return m_data && stricmp(peek(), "None") == 0;
}

//-----------------------------------------------------------------------------
Bool AsciiString::nextToken(AsciiString* tok, const char* seps)
{
	if (this->isEmpty() || tok == this)
		return false;

	if (seps == nullptr)
		seps = " \n\r\t";

	char* start = skipSeps(peek(), seps);
	char* end = skipNonSeps(start, seps);

	if (end > start)
	{
		Int len = end - start;
		char* tmp = tok->getBufferForRead(len + 1);
		memcpy(tmp, start, len);
		tmp[len] = 0;

		this->set(end);

		return true;
	}
	else
	{
		this->clear();
		tok->clear();
		return false;
	}
}
