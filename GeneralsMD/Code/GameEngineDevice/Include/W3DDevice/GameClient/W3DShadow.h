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

#pragma once

#include "WWMath/matrix4.h"
#include "GameClient/Shadow.h"

class Drawable;	//forward reference
class FrustumClass;	//forward reference

// ShadowManager -------------------------------------------------------------
class W3DShadowManager
{

public:

	W3DShadowManager();
	~W3DShadowManager();
	Bool init();	///<initialize resources used by manager, must have valid D3D device.
	void queueShadows(Bool state) {m_isShadowScene=state;}	///<flags system to process shadows on next render call.

	// shadow list management
	void Reset();
	Shadow* addShadow( RenderObjClass *robj,Shadow::ShadowTypeInfo *shadowInfo=nullptr, Drawable *draw=nullptr);	///< adds shadow caster to rendering system.
	void removeShadow(Shadow *shadow);	///< removed shadow from rendering system and frees its resources.
	void removeAllShadows(); ///< Remove all shadows.
	void setShadowColor(UnsignedInt color) { m_shadowColor=color;}	///<sets the shadow color and alpha, value in ARGB format.
	UnsignedInt getShadowColor() { return m_shadowColor;}	///<gets the shadow color and alpha, value in ARGB format.
	void setLightPosition(Int lightIndex, Real x, Real y, Real z);	///<sets the position of a specific light source.
	void setTimeOfDay(TimeOfDay tod);
	void invalidateCachedLightPositions();	///<forces shadow volumes to update regardless of last lightposition
	Vector3 &getLightPosWorld(Int lightIndex);	///<returns the position of specified light source.
	// GeneralsX @refactor cemlyn007 01/10/2026 The frustum of the camera the shadows are drawn for, set by DoShadows at each
	// draw (PLAN-023 Phase 8, stage RR3).
	const FrustumClass *getShadowCameraFrustum() { return m_shadowCameraFrustum; }
	void setShadowCameraFrustum(const FrustumClass *frustum) { m_shadowCameraFrustum = frustum; }
	Bool	isShadowScene()	{return m_isShadowScene;}
	void setStencilShadowMask(int mask) {m_stencilShadowMask=mask;}	///<mask used to mask out stencil bits used for storing occlusion/playerColor
	Int getStencilShadowMask()	{return m_stencilShadowMask;}

	// rendering
	void RenderShadows();
	void ReleaseResources();
	Bool ReAcquireResources();

protected:

		Bool	m_isShadowScene;	///<flag if current scene needs shadows.  No shadows on pre-pass and 2D.
		UnsignedInt m_shadowColor;	///<color and alpha for all shadows in scene.
		Int m_stencilShadowMask;
		// GeneralsX @refactor cemlyn007 01/10/2026 W3DShadow.cpp's shadowCameraFrustum and LightPosWorld, globals before: each
		// render engine's shadows use their own camera and light (PLAN-023 Phase 8, stage RR3).
		const FrustumClass *m_shadowCameraFrustum = nullptr;
		Vector3 m_lightPosWorld[ MAX_SHADOW_LIGHTS ] =
		{
			Vector3( 94.0161f, 50.499f, 200.0f)
		};
};

#if !RTS_ENGINE_CONTEXT
extern W3DShadowManager *TheW3DShadowManager;
#endif
