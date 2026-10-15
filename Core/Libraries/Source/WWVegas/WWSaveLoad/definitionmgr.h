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
 *                 Project Name : WWSaveLoad                                                   *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwsaveload/definitionmgr.h                   $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 8/24/01 5:13p                                               $*
 *                                                                                             *
 *                    $Revision:: 24                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "saveload.h"
#include "saveloadsubsystem.h"
#include "saveloadids.h"
#include "WWDebug/wwdebug.h"
#include "WWLib/wwstring.h"
#include "WWLib/hashtemplate.h"
#include "WWLib/Vector.h"


// Forward declarations
class DefinitionClass;

//////////////////////////////////////////////////////////////////////////////////
//	Global declarations
//////////////////////////////////////////////////////////////////////////////////
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
class DefinitionMgrClass;
RTS_ENGINE_EXTERN_GLOBAL(DefinitionMgrClass, _TheDefinitionMgr);
#define _TheDefinitionMgr RTS_ENGINE_GLOBAL_REF(_TheDefinitionMgr)
#else
extern class DefinitionMgrClass	_TheDefinitionMgr;
#endif

//////////////////////////////////////////////////////////////////////////////////
//
//	DefinitionMgrClass
//
//////////////////////////////////////////////////////////////////////////////////
#if RTS_ENGINE_CONTEXT
#define DefinitionHash RTS_ENGINE_GLOBAL_REF(DefinitionHash)
#endif
#if RTS_ENGINE_CONTEXT
#define _DefinitionCount RTS_ENGINE_GLOBAL_REF(_DefinitionCount)
#endif
#if RTS_ENGINE_CONTEXT
#define _MaxDefinitionCount RTS_ENGINE_GLOBAL_REF(_MaxDefinitionCount)
#endif
#if RTS_ENGINE_CONTEXT
#define _SortedDefinitionArray RTS_ENGINE_GLOBAL_REF(_SortedDefinitionArray)
#endif
class DefinitionMgrClass : public SaveLoadSubSystemClass
{
public:

	/////////////////////////////////////////////////////////////////////
	//	Public constructors/destructors
	/////////////////////////////////////////////////////////////////////
	DefinitionMgrClass ();
	virtual ~DefinitionMgrClass () override;

	/////////////////////////////////////////////////////////////////////
	//	Public methods
	/////////////////////////////////////////////////////////////////////

	// From SaveLoadSubSystemClass
	virtual uint32					Chunk_ID () const override;

	// Type identification
	static DefinitionClass *	Find_Definition (uint32 id, bool twiddle = true);
	static DefinitionClass *	Find_Named_Definition (const char *name, bool twiddle = true);
	static DefinitionClass *	Find_Typed_Definition (const char *name, uint32 class_id, bool twiddle = true);
   static void                List_Available_Definitions ();
   static void                List_Available_Definitions (int superclass_id);
	static uint32					Get_New_ID (uint32 class_id);

	// Definition registration
	static void						Register_Definition (DefinitionClass *definition);
	static void						Unregister_Definition (DefinitionClass *definition);

	//
	// Definition enumeration
	//
	typedef enum
	{
		ID_CLASS			= 1,
		ID_SUPERCLASS,
	} ID_TYPE;

	static DefinitionClass *	Get_First ();
	static DefinitionClass *	Get_First (uint32 id, ID_TYPE type = ID_CLASS);
	static DefinitionClass *	Get_Next (DefinitionClass *curr_def);
	static DefinitionClass *	Get_Next (DefinitionClass *curr_def, uint32 id, ID_TYPE type = ID_CLASS);

	static void						Free_Definitions ();

protected:

	/////////////////////////////////////////////////////////////////////
	//	Protected methods
	/////////////////////////////////////////////////////////////////////

	// From SaveLoadSubSystemClass
	virtual bool					Contains_Data () const override;
	virtual bool					Save (ChunkSaveClass &csave) override;
	virtual bool					Load (ChunkLoadClass &cload) override;
	virtual const char*			Name () const override { return "DefinitionMgrClass"; }

	// Persistence methods
	bool								Save_Objects (ChunkSaveClass &csave);
	bool								Load_Objects (ChunkLoadClass &cload);
	bool								Save_Variables (ChunkSaveClass &csave);
	bool								Load_Variables (ChunkLoadClass &cload);

private:
#if RTS_ENGINE_CONTEXT
	using DefinitionHash_rtsEngineType = HashTemplateClass<StringClass, DynamicVectorClass<DefinitionClass*>*>*;
	RTS_ENGINE_CLASS_STATIC(DefinitionHash_rtsEngineType, DefinitionHash);
#else
	static HashTemplateClass<StringClass, DynamicVectorClass<DefinitionClass*>*>* DefinitionHash;
#endif

	/////////////////////////////////////////////////////////////////////
	//	Private methods
	/////////////////////////////////////////////////////////////////////
	static void						Prepare_Definition_Array ();
	static int __cdecl			fnCompareDefinitionsCallback (const void *elem1, const void *elem2);

	/////////////////////////////////////////////////////////////////////
	//	Static member data
	/////////////////////////////////////////////////////////////////////
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(DefinitionClass**, _SortedDefinitionArray);
#else
	static DefinitionClass **	_SortedDefinitionArray;
#endif
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(int, _MaxDefinitionCount);
#else
	static int						_MaxDefinitionCount;
#endif
#if RTS_ENGINE_CONTEXT
	RTS_ENGINE_CLASS_STATIC(int, _DefinitionCount);
#else
	static int						_DefinitionCount;
#endif

	/////////////////////////////////////////////////////////////////////
	//	Friend classes
	/////////////////////////////////////////////////////////////////////
	friend class DefinitionClass;
};

/////////////////////////////////////////////////////////////////////
//	Chunk_ID
/////////////////////////////////////////////////////////////////////
inline uint32
DefinitionMgrClass::Chunk_ID () const
{
	return CHUNKID_SAVELOAD_DEFMGR;
}

/////////////////////////////////////////////////////////////////////
//	Contains_Data
/////////////////////////////////////////////////////////////////////
inline bool
DefinitionMgrClass::Contains_Data () const
{
	return true;  // TODO: check if we have any definitions...
}

/////////////////////////////////////////////////////////////////////
//	Get_First_Definition
/////////////////////////////////////////////////////////////////////
inline DefinitionClass *
DefinitionMgrClass::Get_First ()
{
	DefinitionClass *definition = nullptr;
	if (_DefinitionCount > 0) {
		definition = _SortedDefinitionArray[0];
	}

	return definition;
}
#if RTS_ENGINE_CONTEXT
#undef _SortedDefinitionArray
#undef _MaxDefinitionCount
#undef _DefinitionCount
#undef DefinitionHash
#endif
