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

// CommandLine.h
// The command-line interface
// Author: Matthew D. Campbell, September 2001

#pragma once

class CommandLine
{
public:

	// Parses startup flags and applies the process working directory.
#if RTS_ENGINE_CONTEXT
	// GeneralsX @refactor cemlyn007 16/10/2026 With RTS_ENGINE_CONTEXT the engine's command line is its host's to give,
	// never the process's: `argv[0]` is the program name, as in main(), and argc may be 0 for none. The engine keeps the
	// pointers for parseCommandLineForEngineInit, so the arguments must outlive GameEngine::init.
	static void parseCommandLineForStartup(int argc, char** argv);
#else
	static void parseCommandLineForStartup();
#endif
	static void parseCommandLineForEngineInit();

	// Returns true if command-line parsing consumed the zero-based argument index.
	// The index excludes the executable name.
	static bool wasCommandLineArgumentParsed(int argIndex);
};
