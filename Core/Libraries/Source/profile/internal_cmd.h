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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/profile/internal_cmd.h $
// $Author: mhoffe $
// $Revision: #1 $
// $DateTime: 2003/07/09 10:57:23 $
//
// (c) 2003 Electronic Arts
//
// Profile module command interface
//////////////////////////////////////////////////////////////////////////////

#pragma once

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 14/10/2026 This file's statics are per engine (Common/EngineStatic.h, docs/EMBEDDING.md).
#define numResIf RTS_ENGINE_GLOBAL_REF(numResIf)
#endif
#if RTS_ENGINE_CONTEXT
#define resIf RTS_ENGINE_GLOBAL_REF(resIf)
#endif
class ProfileCmdInterface: public DebugCmdInterface
{
  struct Factory
  {
    ProfileResultInterface* (*func)(int, const char * const *);
    const char *name,*arg;
  };

#if RTS_ENGINE_CONTEXT
  RTS_ENGINE_CLASS_STATIC(unsigned, numResIf);
#else
  static unsigned numResIf;
#endif
#if RTS_ENGINE_CONTEXT
  RTS_ENGINE_CLASS_STATIC(Factory*, resIf);
#else
  static Factory *resIf;
#endif

  unsigned numResFunc; // optimizer bug: must be declared volatile!
  ProfileResultInterface **resFunc;

public:
  ProfileCmdInterface(): numResFunc(0), resFunc(0) {}

  static void AddResultFunction(ProfileResultInterface* (*func)(int, const char * const *),
                                const char *name, const char *arg);
  void RunResultFunctions();

  virtual bool Execute(class Debug& dbg, const char *cmd, CommandMode cmdmode,
                       unsigned argn, const char * const * argv) override;
  virtual void Delete() override {}
};
#if RTS_ENGINE_CONTEXT
#undef resIf
#undef numResIf
#endif
