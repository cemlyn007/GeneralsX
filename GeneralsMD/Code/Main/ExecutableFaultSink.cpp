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
// FILE: ExecutableFaultSink.cpp
// GeneralsX @feature cemlyn007 15/10/2026 The game executable's FaultSink (see ExecutableEmbedding.h): the process is
// the engine's, so a fault ends it as upstream's ReleaseCrash does once its crash report is written (GeneralsX
// writes the report before it calls the sink, Common/EngineFault.h).

#if RTS_ENGINE_CONTEXT

#include "ExecutableEmbedding.h"

#include <cstdio>
#include <string>
#include <unistd.h>

#ifdef _WIN32
#include "Common/GlobalData.h"
#include "Common/ApplicationWindow.h"
#include "WW3D2/dx8wrapper.h"
#endif

namespace rts
{

namespace
{

class ExecutableFaultSink final : public FaultSink
{
public:
	[[noreturn]] void fault(std::string reason) override
	{
		fprintf(stderr, "FATAL ERROR: %s\n", reason.c_str());
		fflush(stderr);
#ifdef _WIN32
		if (!DX8Wrapper::Is_Windowed() && ApplicationHWnd)
			ShowWindow(ApplicationHWnd, SW_HIDE);
		if (!(TheGlobalData && TheGlobalData->m_headless))
		{
			::MessageBox(nullptr, "You have encountered a serious error.  Serious errors can be caused by many things including viruses, overheated hardware and hardware that does not meet the minimum specifications for the game. Please visit the forums at www.generals.ea.com for suggested courses of action or consult your manual for Technical Support contact information.",
				"Technical Difficulties...",
				MB_OK|MB_SYSTEMMODAL|MB_ICONERROR);
		}
#endif
		_exit(1);
	}
};

constexpr ExecutableFaultSink theExecutableFaultSink{};

} // namespace

FaultSink* executableFaultSink()
{
	return const_cast<ExecutableFaultSink*>(&theExecutableFaultSink);
}

} // namespace rts

#endif
