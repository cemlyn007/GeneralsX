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

#include "saveloadstatus.h"
#include "WWLib/mutex.h"

#define MAX_STATUS_TEXT_ID 2

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_GLOBAL(CriticalSectionClass, text_mutex);
#define text_mutex RTS_ENGINE_GLOBAL_REF(text_mutex)
#else
static CriticalSectionClass text_mutex;
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(StringClass[MAX_STATUS_TEXT_ID], status_text);
#define status_text RTS_ENGINE_GLOBAL_REF(status_text)
#else
static StringClass status_text[MAX_STATUS_TEXT_ID];
#endif

void SaveLoadStatus::Set_Status_Text(const char* text,int id)
{
	CriticalSectionClass::LockClass m(text_mutex);
	WWASSERT(id<MAX_STATUS_TEXT_ID);
	status_text[id]=text;
	if (id==0) status_text[1]="";
}

void SaveLoadStatus::Get_Status_Text(StringClass& text, int id)
{
	CriticalSectionClass::LockClass m(text_mutex);
	WWASSERT(id<MAX_STATUS_TEXT_ID);
	text=status_text[id];
}

#if RTS_ENGINE_CONTEXT
RTS_ENGINE_GLOBAL(int, status_count);
#define status_count RTS_ENGINE_GLOBAL_REF(status_count)
#else
static	int	status_count;
#endif
void	SaveLoadStatus::Reset_Status_Count()
{
	status_count = 0;
}

void	SaveLoadStatus::Inc_Status_Count()
{
	status_count++;
}

int	SaveLoadStatus::Get_Status_Count()
{
	return status_count;
}

