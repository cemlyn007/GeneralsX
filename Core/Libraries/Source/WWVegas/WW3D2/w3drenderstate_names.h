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
// FILE: w3drenderstate_names.h (no include guard: included more than once)
// GeneralsX @feature cemlyn007 30/09/2026 DX8Wrapper's state names as the current engine's W3DRenderState fields
// (PLAN-023 Phase 8, stage RR2a-1; see w3drenderstate.h)
//
// Reference-returning macros, so each name keeps its real type: `sizeof`, `&`, `[]` and `.` behave as they did on
// the static. Their names are common words, so they are defined only where DX8Wrapper's code is: dx8wrapper.h
// includes this before the class and w3drenderstate_names_end.h at its end, and dx8wrapper.cpp includes this after
// its last #include. BitDepth and Textures, also members of other classes used there, are
// rts::IndirectContextField stand-ins in the class instead. With RTS_ENGINE_CONTEXT off it defines nothing.
#if RTS_ENGINE_CONTEXT
#define m_pCleanupHook (W3D_Render_State().m_pCleanupHook)
#define render_state (W3D_Render_State().render_state)
#define render_state_changed (W3D_Render_State().render_state_changed)
#define DX8Transforms (W3D_Render_State().DX8Transforms)
#define IsInitted (W3D_Render_State().IsInitted)
#define IsDeviceLost (W3D_Render_State().IsDeviceLost)
#define _MainThreadID (W3D_Render_State()._MainThreadID)
#define _EnableTriangleDraw (W3D_Render_State()._EnableTriangleDraw)
#define CurRenderDevice (W3D_Render_State().CurRenderDevice)
#define ResolutionWidth (W3D_Render_State().ResolutionWidth)
#define ResolutionHeight (W3D_Render_State().ResolutionHeight)
#define TextureBitDepth (W3D_Render_State().TextureBitDepth)
#define IsWindowed (W3D_Render_State().IsWindowed)
#define DisplayFormat (W3D_Render_State().DisplayFormat)
#define MultiSampleAntiAliasing (W3D_Render_State().MultiSampleAntiAliasing)
#define Vertex_Shader (W3D_Render_State().Vertex_Shader)
#define Pixel_Shader (W3D_Render_State().Pixel_Shader)
#define Vertex_Shader_Constants (W3D_Render_State().Vertex_Shader_Constants)
#define Pixel_Shader_Constants (W3D_Render_State().Pixel_Shader_Constants)
#define Light_Environment (W3D_Render_State().Light_Environment)
#define Vertex_Processing_Behavior (W3D_Render_State().Vertex_Processing_Behavior)
#define Shadow_Map (W3D_Render_State().Shadow_Map)
#define Ambient_Color (W3D_Render_State().Ambient_Color)
#define world_identity (W3D_Render_State().world_identity)
#define RenderStates (W3D_Render_State().RenderStates)
#define TextureStageStates (W3D_Render_State().TextureStageStates)
#define FogEnable (W3D_Render_State().FogEnable)
#define FogColor (W3D_Render_State().FogColor)
#define FrameStatistics (W3D_Render_State().FrameStatistics)
#define CurrentDX8LightEnables (W3D_Render_State().CurrentDX8LightEnables)
#define FrameCount (W3D_Render_State().FrameCount)
#define CurrentCaps (W3D_Render_State().CurrentCaps)
#define CurrentAdapterIdentifier (W3D_Render_State().CurrentAdapterIdentifier)
#define D3DInterface (W3D_Render_State().D3DInterface)
#define D3DDevice (W3D_Render_State().D3DDevice)
#define CurrentRenderTarget (W3D_Render_State().CurrentRenderTarget)
#define CurrentDepthBuffer (W3D_Render_State().CurrentDepthBuffer)
#define DefaultRenderTarget (W3D_Render_State().DefaultRenderTarget)
#define DefaultDepthBuffer (W3D_Render_State().DefaultDepthBuffer)
#define DrawPolygonLowBoundLimit (W3D_Render_State().DrawPolygonLowBoundLimit)
#define IsRenderToTexture (W3D_Render_State().IsRenderToTexture)
#define s_getNativeDisplaySize (W3D_Render_State().s_getNativeDisplaySize)
#define s_getWindowSize (W3D_Render_State().s_getWindowSize)
#define s_pillarboxEnabled (W3D_Render_State().s_pillarboxEnabled)
#define s_pillarboxActive (W3D_Render_State().s_pillarboxActive)
#define s_bbW (W3D_Render_State().s_bbW)
#define s_bbH (W3D_Render_State().s_bbH)
#define s_dstX (W3D_Render_State().s_dstX)
#define s_dstY (W3D_Render_State().s_dstY)
#define s_dstW (W3D_Render_State().s_dstW)
#define s_dstH (W3D_Render_State().s_dstH)
#define s_pixelDensity (W3D_Render_State().s_pixelDensity)
#define s_offscreenTex (W3D_Render_State().s_offscreenTex)
#define s_offscreenSurf (W3D_Render_State().s_offscreenSurf)
#define s_depthSurf (W3D_Render_State().s_depthSurf)
#define s_savedBackbuffer (W3D_Render_State().s_savedBackbuffer)
#define s_savedDepth (W3D_Render_State().s_savedDepth)
#define ZBias (W3D_Render_State().ZBias)
#define ZNear (W3D_Render_State().ZNear)
#define ZFar (W3D_Render_State().ZFar)
#define ProjectionMatrix (W3D_Render_State().ProjectionMatrix)
// dx8wrapper.cpp's file statics (w3drenderstate_names_end.h leaves these defined only there, as they were).
#define _Hwnd (W3D_Render_State()._Hwnd)
#define _PresentParameters (W3D_Render_State()._PresentParameters)
#define LastFrameStatistics (W3D_Render_State().LastFrameStatistics)
#define _RenderDeviceNameTable (W3D_Render_State()._RenderDeviceNameTable)
#define _RenderDeviceShortNameTable (W3D_Render_State()._RenderDeviceShortNameTable)
#define _RenderDeviceDescriptionTable (W3D_Render_State()._RenderDeviceDescriptionTable)
#endif // RTS_ENGINE_CONTEXT
