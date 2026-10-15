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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : wwsaveload                                                   *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwsaveload/wwsaveload.cpp                    $*
 *                                                                                             *
 *              Original Author:: Greg Hjelstrom                                               *
 *                                                                                             *
 *                      $Author:: Greg_h                                                      $*
 *                                                                                             *
 *                     $Modtime:: 3/28/00 9:18a                                               $*
 *                                                                                             *
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "wwsaveload.h"
#include "definitionmgr.h"


#if RTS_ENGINE_CONTEXT
void WWSaveLoad_Register_Twiddler_Factories();
#endif

void WWSaveLoad::Init()
{
#if RTS_ENGINE_CONTEXT
	// GeneralsX @refactor cemlyn007 14/10/2026 This engine's factories and definition manager, which upstream made, and so
	// registered, at static initialisation (docs/EMBEDDING.md)
	WWSaveLoad_Register_Twiddler_Factories();
	_TheDefinitionMgr;
#endif
}

void WWSaveLoad::Shutdown()
{
	_TheDefinitionMgr.Free_Definitions();
}


