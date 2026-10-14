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
 *                 Project Name : WWMath                                                       *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwmath/wwmath.cpp                            $*
 *                                                                                             *
 *                       Author:: Eric_c                                                       *
 *                                                                                             *
 *                     $Modtime:: 5/10/01 10:52p                                              $*
 *                                                                                             *
 *                    $Revision:: 11                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "wwmath.h"
#include "WWDebug/wwhack.h"
#include "lookuptable.h"
#include <stdlib.h>
#include "WWDebug/wwdebug.h"
#include "WWDebug/wwprofile.h"

// TODO: convert to use loouptablemanager...
#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
RTS_ENGINE_EXTERN_GLOBAL_DEFINE(float[ARC_TABLE_SIZE], _FastAcosTable);
#define _FastAcosTable RTS_ENGINE_GLOBAL_REF(_FastAcosTable)
#else
float _FastAcosTable[ARC_TABLE_SIZE];
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL_DEFINE(float[ARC_TABLE_SIZE], _FastAsinTable);
#define _FastAsinTable RTS_ENGINE_GLOBAL_REF(_FastAsinTable)
#else
float _FastAsinTable[ARC_TABLE_SIZE];
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL_DEFINE(float[SIN_TABLE_SIZE], _FastSinTable);
#define _FastSinTable RTS_ENGINE_GLOBAL_REF(_FastSinTable)
#else
float _FastSinTable[SIN_TABLE_SIZE];
#endif
#if RTS_ENGINE_CONTEXT
RTS_ENGINE_EXTERN_GLOBAL_DEFINE(float[SIN_TABLE_SIZE], _FastInvSinTable);
#define _FastInvSinTable RTS_ENGINE_GLOBAL_REF(_FastInvSinTable)
#else
float _FastInvSinTable[SIN_TABLE_SIZE];
#endif

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 28/09/2026 Init and Shutdown pair per engine (EngineContext::wwMathInitialized), so an
// unpaired Shutdown (W3DDisplay's failed init, then its destructor) does nothing.
// GeneralsX @refactor cemlyn007 14/10/2026 Each engine builds its own _Fast* tables and lookup tables, and registers its
// own persist factories: they are per-engine statics (docs/EMBEDDING.md), so no count across engines is kept.
#include "WWLib/RANDOM.h"
#include "WWSaveLoad/wwsaveload.h"
void WWMath_Register_Hermitespline_Factories();
void WWMath_Register_Curve_Factories();
void WWMath_Register_Catmullromspline_Factories();
void WWMath_Register_Tcbspline_Factories();
void WWMath_Register_Vehiclecurve_Factories();
void WWMath_Register_Cardinalspline_Factories();
#endif

void		WWMath::Init()
{
#if RTS_ENGINE_CONTEXT
	rts::EngineContext* context = rts::ctx();
	if (context->wwMathInitialized)
		return;
	context->wwMathInitialized = true;
	WWSaveLoad::Init();
	WWMath_Register_Hermitespline_Factories();
	WWMath_Register_Curve_Factories();
	WWMath_Register_Catmullromspline_Factories();
	WWMath_Register_Tcbspline_Factories();
	WWMath_Register_Vehiclecurve_Factories();
	WWMath_Register_Cardinalspline_Factories();
#endif
	LookupTableMgrClass::Init();

	int a=0;
	for (;a<ARC_TABLE_SIZE;++a) {
		float cv=float(a-ARC_TABLE_SIZE/2)*(1.0f/(ARC_TABLE_SIZE/2));
		_FastAcosTable[a]=acos(cv);
		_FastAsinTable[a]=asin(cv);
	}

	for (a=0;a<SIN_TABLE_SIZE;++a) {
		float cv= (float)a * 2.0f * WWMATH_PI / SIN_TABLE_SIZE; //float(a-SIN_TABLE_SIZE/2)*(1.0f/(SIN_TABLE_SIZE/2));
		_FastSinTable[a]=sin(cv);

		if (a>0) {
			_FastInvSinTable[a]=1.0f/_FastSinTable[a];
		} else {
			_FastInvSinTable[a]=WWMATH_FLOAT_MAX;
		}
	}
}

void		WWMath::Shutdown()
{
#if RTS_ENGINE_CONTEXT
	rts::EngineContext* context = rts::ctx();
	if (!context->wwMathInitialized)
		return;
	context->wwMathInitialized = false;
#endif
	LookupTableMgrClass::Shutdown();
}

#if RTS_ENGINE_CONTEXT
// GeneralsX @bugfix cemlyn007 01/10/2026 From the engine's own generator, seeded the same for every engine, not the C
// library's process-wide rand(): the camera shake (CameraShakeSystem) draws its frequencies and jitter from it, so an
// engine's shake, and its image, followed whatever other engines had drawn (PLAN-023 Phase 8, stage RR3).
static constexpr rts::PerEngineStatic<Random3Class> RandomFloat_perEngine{};

float		WWMath::Random_Float()
{
	return ((float)(RandomFloat_perEngine.get()() & 0xFFF)) / (float)(0xFFF);
}
#else
float		WWMath::Random_Float()
{
	return ((float)(rand() & 0xFFF)) / (float)(0xFFF);
}
#endif


/*
** Force link some modules from this library.
*/
void Do_Force_Links()
{
	FORCE_LINK(curve);
	FORCE_LINK(hermitespline);
	FORCE_LINK(catmullromspline);
	FORCE_LINK(cardinalspline);
	FORCE_LINK(tcbspline);
}

