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
// FILE: w3drenderstate.h
// GeneralsX @feature cemlyn007 30/09/2026 One render engine's DX8Wrapper state (PLAN-023 Phase 8, stage RR2a-1)
//
// DX8Wrapper's mutable statics (its Direct3D interface and device, the caps, the presentation parameters, the
// render targets, the cached render and texture-stage states, transforms and lights, the pillarbox, the frame
// statistics), Debug_Statistics' counters and ShaderClass's device state (RR2a-2), as one struct. With
// RTS_ENGINE_CONTEXT each rendering engine owns one, rts::EngineContext::w3dRender: WW3D::Init (or
// Set_Display_Size_Provider, which W3DDisplay::init calls just before it, or DX8Wrapper::Init) allocates it, the end
// of WW3D::Shutdown frees it (RR2a-2, with the engine's WW3DState: ww3d.h), and it stays null for an engine that
// never renders. Several engines can then each drive their own device, and a headless engine never reaches a
// renderer's device or caps.
//
// The upstream names are unchanged: inside DX8Wrapper (dx8wrapper.h and dx8wrapper.cpp) each is a macro for the
// current engine's field (w3drenderstate_names.h), or, where the name is also a member of another class used
// there (BitDepth, Textures), an rts::IndirectContextField stand-in. An engine with no render state reads
// W3DRenderState::Defaults, every field at its upstream initial value (IsInitted false, no device, no caps),
// which is what a headless engine read from the statics before. Nothing may write the defaults: the struct is page
// aligned, so they fill whole pages of their own, which W3D_Protect_Render_Defaults makes read-only (RR2a-2).
//
// Part of dx8wrapper.h, which includes it once the types below are declared: include dx8wrapper.h, not this.
#pragma once

#include "WWLib/simplevec.h"
#include "WWLib/Vector.h"
#include "WWLib/wwstring.h"
#include "rddesc.h"

class DX8_CleanupHook;
class DX8Caps;
class LightEnvironmentClass;
class TextureClass;
class ZTextureClass;

// Debug_Statistics' counters (statistics.cpp): what the latest frame drew. Debug-only by purpose, but every
// render engine's draw writes them (WW3D::Begin_Render/End_Render, every Record_*), so each renderer counts its
// own. Without RTS_ENGINE_CONTEXT statistics.cpp keeps one for the process.
struct DebugStatisticsState
{
	struct TextureStatisticsStruct
	{
		TextureClass* tex;
		int usage_count;
		int change_count;
	};

	int texture_memory;
	int texture_count;
	int lightmap_texture_memory;
	int lightmap_texture_count;
	int procedural_texture_memory;
	int procedural_texture_count;
	int record_count;
	int texture_change_count;
	int last_frame_texture_memory;
	int last_frame_texture_count;
	int last_frame_lightmap_texture_memory;
	int last_frame_lightmap_texture_count;
	int last_frame_procedural_texture_memory;
	int last_frame_procedural_texture_count;
	int last_frame_record_count;
	int last_frame_texture_change_count;
	TextureClass* latest_texture;
	Debug_Statistics::RecordTextureMode record_texture_mode;
	StringClass texture_statistics_string;
	SimpleDynVecClass<TextureStatisticsStruct> texture_statistics;

	int dx8_skin_renders;
	int last_frame_dx8_skin_renders;
	int dx8_skin_polygons;
	int last_frame_dx8_skin_polygons;
	int dx8_skin_vertices;
	int last_frame_dx8_skin_vertices;
	int dx8_polygons;
	int last_frame_dx8_polygons;
	int dx8_vertices;
	int last_frame_dx8_vertices;
	int sorting_polygons;
	int last_frame_sorting_polygons;
	int sorting_vertices;
	int last_frame_sorting_vertices;
	int draw_calls;
	int last_frame_draw_calls;
};

#if RTS_ENGINE_CONTEXT

// The fields keep the statics' names and upstream initial values. It has no user-provided constructor, so
// `new W3DRenderState()` zero-initialises it before the member initialisers and constructors run, exactly as a
// static's storage was: a fresh engine starts where a fresh process started.
struct RTS_ENGINE_CONTEXT_API alignas(::rts::renderStateAlignment) W3DRenderState
{
	// dx8wrapper.cpp's DEFAULT_RESOLUTION_WIDTH, ... (checked there).
	static constexpr int DefaultResolutionWidth = 640;
	static constexpr int DefaultResolutionHeight = 480;
	static constexpr int DefaultBitDepth = 32;
	static constexpr int DefaultTextureBitDepth = 16;

	// Every field at its upstream initial value, for an engine with no render state (see above). Nothing may
	// write it: only reads reach it.
	static W3DRenderState Defaults;

	// DX8Wrapper's class statics (dx8wrapper.h).
	DX8_CleanupHook* m_pCleanupHook;

	RenderStateStruct render_state;
	unsigned render_state_changed;
	Matrix4x4 DX8Transforms[D3DTS_WORLD + 1];

	bool IsInitted;
	bool IsDeviceLost;
	unsigned _MainThreadID;

	bool _EnableTriangleDraw = true;

	int CurRenderDevice = -1;
	int ResolutionWidth = DefaultResolutionWidth;
	int ResolutionHeight = DefaultResolutionHeight;
	int BitDepth = DefaultBitDepth;
	int TextureBitDepth = DefaultTextureBitDepth;
	bool IsWindowed;
	D3DFORMAT DisplayFormat = D3DFMT_UNKNOWN;
	D3DMULTISAMPLE_TYPE MultiSampleAntiAliasing = D3DMULTISAMPLE_NONE;

	DWORD Vertex_Shader;
	DWORD Pixel_Shader;

	Vector4 Vertex_Shader_Constants[MAX_VERTEX_SHADER_CONSTANTS];
	Vector4 Pixel_Shader_Constants[MAX_PIXEL_SHADER_CONSTANTS];

	LightEnvironmentClass* Light_Environment;

	DWORD Vertex_Processing_Behavior;

	ZTextureClass* Shadow_Map[MAX_SHADOW_MAPS];

	Vector3 Ambient_Color;

	bool world_identity;
	unsigned RenderStates[256];
	unsigned TextureStageStates[MAX_TEXTURE_STAGES][32];
	IDirect3DBaseTexture8* Textures[MAX_TEXTURE_STAGES];

	bool FogEnable;
	D3DCOLOR FogColor;

	DX8FrameStatistics FrameStatistics;
	bool CurrentDX8LightEnables[4];

	unsigned long FrameCount;

	DX8Caps* CurrentCaps;

	D3DADAPTER_IDENTIFIER8 CurrentAdapterIdentifier;

	IDirect3D8* D3DInterface;
	IDirect3DDevice8* D3DDevice;

	IDirect3DSurface8* CurrentRenderTarget;
	IDirect3DSurface8* CurrentDepthBuffer;
	IDirect3DSurface8* DefaultRenderTarget;
	IDirect3DSurface8* DefaultDepthBuffer;

	unsigned DrawPolygonLowBoundLimit;

	bool IsRenderToTexture;

	// DX8Wrapper::DisplaySizeFunc.
	bool (*s_getNativeDisplaySize)(int& outW, int& outH, float& outDensity);
	bool (*s_getWindowSize)(int& outW, int& outH, float& outDensity);

	bool s_pillarboxEnabled;
	bool s_pillarboxActive;
	int s_bbW, s_bbH;
	int s_dstX, s_dstY, s_dstW, s_dstH;
	float s_pixelDensity = 1.0f;
	IDirect3DTexture8* s_offscreenTex;
	IDirect3DSurface8* s_offscreenSurf;
	IDirect3DSurface8* s_depthSurf;
	IDirect3DSurface8* s_savedBackbuffer;
	IDirect3DSurface8* s_savedDepth;

	int ZBias;
	float ZNear;
	float ZFar;
	Matrix4x4 ProjectionMatrix;

	// dx8wrapper.cpp's file statics and globals.
	HWND _Hwnd;
	D3DPRESENT_PARAMETERS _PresentParameters;
	DX8FrameStatistics LastFrameStatistics;
	bool _DX8SingleThreaded;
	DynamicVectorClass<StringClass> _RenderDeviceNameTable;
	DynamicVectorClass<StringClass> _RenderDeviceShortNameTable;
	DynamicVectorClass<RenderDeviceDescClass> _RenderDeviceDescriptionTable;

	// statistics.cpp's.
	DebugStatisticsState DebugStatistics;

	// GeneralsX @feature cemlyn007 30/09/2026 ShaderClass's device state (shader.cpp; PLAN-023 Phase 8, stage
	// RR2a-2): the shader last applied to this engine's device, whether it must be applied in full, and the cull
	// mode (a D3DCULL).
	bool ShaderDirty = true;
	unsigned long CurrentShader;
	unsigned long _PolygonCullMode = D3DCULL_CW;

	// GeneralsX @bugfix cemlyn007 30/09/2026 Whether WW3D::Init took this state (and WW3D's), so that WW3D::Shutdown
	// frees both; otherwise DX8Wrapper::Shutdown does (PLAN-023 Phase 8, stage RR2b).
	bool OwnedByWW3D;
};

// GeneralsX @feature cemlyn007 30/09/2026 Makes the pages of W3DRenderState::Defaults and WW3DState::Defaults
// read-only (or writable again), so that a write to them, which would leak one engine's render state into every
// engine without one, faults where it happens instead of passing unseen (PLAN-023 Phase 8, stage RR2a-2). An
// embedding host calls it once, before any engine boots; the first call also registers an exit handler that makes
// them writable again for their static destructors. Returns false where it cannot (a page size that does not divide
// rts::renderStateAlignment, or no mprotect: Windows), and then changes neither (RR2b).
RTS_ENGINE_CONTEXT_API bool W3D_Protect_Render_Defaults(bool readOnly);

// The current engine's render state, or W3DRenderState::Defaults for an engine that has none.
inline W3DRenderState& W3D_Render_State() noexcept
{
	return ::rts::indirectContext<W3DRenderState, &::rts::EngineContext::w3dRender, W3DRenderState::Defaults>();
}

// dx8wrapper.cpp's global, read by DX8_THREAD_ASSERT everywhere.
#define _DX8SingleThreaded (W3D_Render_State()._DX8SingleThreaded)

#endif // RTS_ENGINE_CONTEXT
