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

#include <atomic>

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

// GeneralsX @feature cemlyn007 01/10/2026 The embedding host's lock around the window's SDL calls (PLAN-023 Phase 8,
// stage RR4).
//
// A host that boots or shuts down other engines on other threads while its window's engine updates (rlgenerals:
// offscreen renderers beside its viewer) serialises SDL's video calls with a lock of its own, which these hooks try.
// Every SDL call the window's engine makes between its boot and shutdown runs under it (EventPumpLock below):
// - SDL3GameEngine::pollSDL3Events (every update() and serviceWindowsOS()) and reset()'s text-input stop;
// - W3DDisplay::draw's DX8Wrapper::Pillarbox_Process_Resize (every frame), whose window-size query
//   (SDL_GetWindowSize, SDL_GetWindowSizeInPixels) and, on a resize, Reset_Device (which rebuilds DXVK's SDL
//   surface) reach SDL. An engine without a window makes no SDL call there and does not take the lock.
// The rest are unreachable for a headless window engine (GlobalData::m_headless, which rlgenerals requires of every
// boot): GameClient::init then makes no keyboard and a MouseDummy, so SDL3Mouse's per-frame calls (draw()'s
// setCursor -> SDL_SetCursor, the event translation's SDL_GetWindowFromID, SDL_GetWindowSize and
// SDL_GetMouseState, capture() and releaseCapture()'s SDL_CaptureMouse and SDL_SetWindowMouseGrab) and
// SDL3Keyboard's never run, and W3DDisplay's window-mode calls (SDL3_ApplyWindowModeForRenderConfig) run only at
// its init (the boot) and from the options menus' setDisplayMode, which need the real window manager. A host with a
// non-headless window engine would have to take the lock around those too. ~SDL3GameEngine's SDL_StopTextInput
// does not take it: an engine is destroyed by its shutdown, which such a host runs under the same lock (rlgenerals:
// its process mutex).
//
// TryLock returns whether the SDL calls may run now; false skips them (the pump's events stay queued for the next
// pump, the resize check waits for the next frame). Unlock follows every TryLock that returned true. Null (the
// game's own main(): one engine on one thread) runs them unlocked. The host stores Unlock, then TryLock (release),
// once, before the window's engine boots; EventPumpLock loads TryLock once (acquire) and Unlock after it.
extern std::atomic<bool (*)()> ApplicationWindow_TryLockEventPump;
extern std::atomic<void (*)()> ApplicationWindow_UnlockEventPump;

// GeneralsX @feature cemlyn007 01/10/2026 Holds the host's lock (above), if it has one, for its scope (PLAN-023
// Phase 8, stage RR4). acquired() is false when the host's lock is busy: the caller then skips its SDL calls.
class EventPumpLock
{
public:
	EventPumpLock()
		: m_unlock(nullptr), m_acquired(true)
	{
		bool (*const tryLock)() = ApplicationWindow_TryLockEventPump.load(std::memory_order_acquire);
		if (tryLock != nullptr) {
			m_acquired = tryLock();
			if (m_acquired) {
				m_unlock = ApplicationWindow_UnlockEventPump.load(std::memory_order_relaxed);
			}
		}
	}
	~EventPumpLock()
	{
		if (m_unlock != nullptr) {
			m_unlock();
		}
	}
	EventPumpLock(const EventPumpLock&) = delete;
	EventPumpLock& operator=(const EventPumpLock&) = delete;

	bool acquired() const { return m_acquired; }

private:
	void (*m_unlock)();
	bool m_acquired;
};
