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
// FILE: ApplicationWindow.h
// GeneralsX @feature cemlyn007 30/09/2026 The application window's handles, in one place (PLAN-023 Phase 8, stage
// RR2a-2)
//
// TheSDL3Window (the SDL3 window, SDL3Main.cpp's) and ApplicationHWnd (the same window as an HWND, or the Win32
// window, WinMain.cpp's). Upstream declared them `extern` in each file that used them; include this instead.
//
// With RTS_ENGINE_CONTEXT they are the current engine's (rts::EngineContext::sdl3Window and applicationHWnd): the
// game's main() sets its engine's, an embedding host its viewer engine's, and every other engine has none (null),
// so an engine never reaches another engine's window. Without it they are the process's globals, defined in the
// game's SDL3Main.cpp or WinMain.cpp.
// GeneralsX @bugfix cemlyn007 30/09/2026 It includes what it uses: the engine context and HWND, from the same
// headers PreRTS.h takes them from (PLAN-023 Phase 8, stage RR2b).
#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include "windows_compat.h"
#endif

#if RTS_ENGINE_CONTEXT
#include "Common/EngineContext.h"
#endif

struct SDL_Window;

#if RTS_ENGINE_CONTEXT
static_assert(sizeof(HWND) == sizeof(void*), "EngineContext::applicationHWnd holds an HWND");
#define TheSDL3Window (::rts::ctx()->sdl3Window)
#define ApplicationHWnd (reinterpret_cast<HWND&>(::rts::ctx()->applicationHWnd))
#else
extern SDL_Window* TheSDL3Window;  ///< the SDL3 window (Linux, macOS)
extern HWND ApplicationHWnd;  ///< our application window handle
#endif

// GeneralsX @feature cemlyn007 01/10/2026 The embedding host's lock around the SDL3 event pump (PLAN-023 Phase 8,
// stage RR4).
//
// SDL3GameEngine::pollSDL3Events (every update() and serviceWindowsOS()) and reset()'s text-input stop are the only
// SDL calls an engine with a window makes outside its boot and shutdown. A host that boots or shuts down other
// engines on other threads while its window's engine updates (rlgenerals: offscreen renderers beside its viewer)
// serialises SDL's video calls with a lock of its own, which these take around them. TryLock returns whether the
// pump may run now; false skips this pump, and the events stay queued for the next one. Unlock follows every TryLock
// that returned true. Null (the game's own main(): one engine on one thread) runs the pump unlocked. Set once,
// before the window's engine boots, and only read after that.
extern bool (*ApplicationWindow_TryLockEventPump)();
extern void (*ApplicationWindow_UnlockEventPump)();
