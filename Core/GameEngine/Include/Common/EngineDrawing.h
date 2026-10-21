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
// FILE: EngineDrawing.h
// GeneralsX @feature cemlyn007 21/10/2026 Drawing on demand (docs/EMBEDDING.md)
//
// A render-headless engine (GlobalData's m_headless and m_headlessRender: an embedding host's image observations)
// whose host sets the context's onDemandDrawing draws nothing as its frames run. After each logic frame, its client
// tick (Display::tickClient) advances, once, everything a draw used to advance: WW3D's clock, the cameras, every
// drawable's draw modules and model animations, the particle simulations, the scene's per-frame updates, the trees and
// the cinematic text's countdown. A draw (Display::draw) then only renders the state the tick left, with no time
// passing, so two draws at one frame are identical and an image does not depend on which frames were drawn.
#pragma once

#if RTS_ENGINE_CONTEXT

#include "Common/EngineContext.h"
#include "Common/GlobalData.h"

namespace rts
{

inline Bool drawsOnDemand()
{
	return ctx()->onDemandDrawing && TheGlobalData != nullptr && TheGlobalData->m_headless && TheGlobalData->m_headlessRender;
}

}

#endif
