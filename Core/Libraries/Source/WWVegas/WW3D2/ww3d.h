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
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/ww3d.h                                 $*
 *                                                                                             *
 *                      $Author:: Steve_t                                                     $*
 *                                                                                             *
 *                     $Modtime:: 1/02/02 4:17p                                               $*
 *                                                                                             *
 *                    $Revision:: 42                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "WWLib/always.h"
#include "WWMath/vector3.h"
#include "WW3D2/layer.h"
#include "WW3D2/w3derr.h"
#include "WW3D2/robjlist.h"
#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 30/09/2026 WW3DState holds two ShaderClass values (PLAN-023 Phase 8, stage RR2a-2).
#include "WW3D2/shader.h"
#endif

class		SceneClass;
class		CameraClass;
class		ShaderClass;
class		DX8Wrapper;
class		IRenderBackend;

struct	RenderStatistics;
class		FrameGrabClass;
class		VertexMaterialClass;
class		ExtraMaterialPassClass;
class		RenderInfoClass;
class		RenderDeviceDescClass;
class		StringClass;
class		LightEnvironmentClass;
class		MaterialPassClass;
class 	StaticSortListClass;

#define MESH_RENDER_SNAPSHOT_ENABLED
#define SNAPSHOT_SAY(x) if (WW3D::Is_Snapshot_Activated()) { WWDEBUG_SAY(x); }
//#define SNAPSHOT_SAY(x)

/**
** WW3D
**
** This is the collection of static functions and data which initialize and
** control the behavior of the WW3D library.
*/
class WW3D
{
public:

	enum MultiSampleModeEnum {
		MULTISAMPLE_MODE_NONE = 0,
		MULTISAMPLE_MODE_2X = 2,
		MULTISAMPLE_MODE_4X = 4,
		MULTISAMPLE_MODE_8X = 8
	};

	enum PrelitModeEnum {
		PRELIT_MODE_VERTEX,
		PRELIT_MODE_LIGHTMAP_MULTI_PASS,
		PRELIT_MODE_LIGHTMAP_MULTI_TEXTURE
	};

	enum MeshDrawModeEnum {
		MESH_DRAW_MODE_OLD,
		MESH_DRAW_MODE_NEW,
		MESH_DRAW_MODE_DEBUG_DRAW,
		MESH_DRAW_MODE_DEBUG_CLIP,
		MESH_DRAW_MODE_DEBUG_BOX,
		MESH_DRAW_MODE_NONE,
		MESH_DRAW_MODE_DX8_ONLY
	};

	enum NPatchesGapFillingModeEnum {
		NPATCHES_GAP_FILLING_DISABLED,
		NPATCHES_GAP_FILLING_ENABLED,
		NPATCHES_GAP_FILLING_FORCE
	};

	enum ScreenShotFormatEnum {
		TGA,
		BMP
	};


	static WW3DErrorType		Init(void * hwnd, char *defaultpal = nullptr, bool lite = false);
	static WW3DErrorType		Shutdown();
	static bool					Is_Initted()								{ return IsInitted; }

	// The active rendering backend.
	// Created in Init and destroyed in Shutdown, so it is never null in between.
	static IRenderBackend *	Get_Render_Backend()						{ return RenderBackend; }

	static int					Get_Render_Device_Count();
	static const char *		Get_Render_Device_Name(int device_index);
	static const RenderDeviceDescClass &								Get_Render_Device_Desc(int device = -1);

	static int					Get_Render_Device();
	static WW3DErrorType		Set_Render_Device( int dev=-1, int resx=-1, int resy=-1, int bits=-1, int windowed=-1, bool resize_window = false, bool reset_device=false, bool restore_assets=true);
	static WW3DErrorType		Set_Render_Device( const char *dev_name, int resx=-1, int resy=-1, int bits=-1, int windowed=-1, bool resize_window = false  );
	static WW3DErrorType		Set_Next_Render_Device();
	static WW3DErrorType		Set_Any_Render_Device();

	static void					Get_Pixel_Center(float &x, float &y);
	static void					Get_Render_Target_Resolution(int & set_w,int & set_h,int & get_bits,bool & get_windowed);
	static void					Get_Device_Resolution(int & set_w,int & set_h,int & get_bits,bool & get_windowed);
	static WW3DErrorType		Set_Device_Resolution(int w=-1,int h=-1,int bits=-1,int windowed=-1, bool resize_window=false );

	static bool					Is_Windowed();
	static WW3DErrorType		Toggle_Windowed ();
	static void					Set_Window( void *hwnd );
	static void *				Get_Window();

	static WW3DErrorType		On_Activate_App();
	static WW3DErrorType		On_Deactivate_App();

	static WW3DErrorType		Registry_Save_Render_Device( const char * sub_key );
	static WW3DErrorType		Registry_Save_Render_Device( const char * sub_key, int device, int width, int height, int depth, bool windowed, int texture_depth );
	static WW3DErrorType		Registry_Load_Render_Device( const char * sub_key, bool resize_window = false );
	static bool					Registry_Load_Render_Device( const char * sub_key, char *device, int device_len, int &width, int &height, int &depth, int &windowed, int& texture_depth);

	// 0 = bilinear, 1 = trilinear, 2 = anisotropic
	static void					Set_Texture_Filter(int filter);
	static int					Get_Texture_Filter() { return TextureFilter; }

	static void					Set_Anisotropy_Level(int level);
	static int					Get_Anisotropy_Level() { return AnisotropyLevel; }

	/*
	** Rendering functions
	** Each frame should be bracketed by a Begin_Render and End_Render call.  Between these two calls you will
	** normally render scenes.  The render function which accepts a single render object is implemented for
	** special cases like generating a shadow texture for an object.  Basically this function will have the
	** entire scene rendering overhead.
	*/
	static WW3DErrorType		Begin_Render(bool clear = false,bool clearz = true,const Vector3 & color = Vector3(0,0,0), float dest_alpha=0.0f, void(*network_callback)() = nullptr);
	static WW3DErrorType		Render(const LayerListClass & layerlist);
	static WW3DErrorType		Render(const LayerClass & layer);
	static WW3DErrorType		Render(SceneClass * scene,CameraClass * cam,bool clear = false,bool clearz = false,const Vector3 & color = Vector3(0,0,0));
	static WW3DErrorType		Render(RenderObjClass & obj,RenderInfoClass & rinfo);
	static void					Flush(RenderInfoClass & rinfo);	// NOTE: "normal" usage should *NEVER* require the user to call this function

	static WW3DErrorType		End_Render(bool flip_frame = true);

	static bool					Is_Rendering() { return( IsRendering ); }

	static void Flip_To_Primary();

	// TheSuperHackers @info Add amount of milliseconds that the simulation has advanced in this render frame.
	// This can be a fraction of a logic step.
	static void Update_Logic_Frame_Time(float milliseconds);
	/*
	** Timing
	** By calling the Sync function, the application can move the ww3d library time forward.  This
	** will control things like animated uv-offset mappers and render object animations.
	*/
	static void						Sync(bool step);

	// Total sync time in milliseconds. Advances in full logic time steps only.
	static unsigned int		Get_Sync_Time() { return SyncTime; }

	// Current sync frame time in milliseconds. Can be zero when the logic has not stepped forward in the current render update.
	static unsigned int		Get_Sync_Frame_Time() { return SyncTime - PreviousSyncTime; }

	// Fractional sync frame time. Accumulates for as long as the sync frame is not stepped forward.
	static unsigned int		Get_Fractional_Sync_Milliseconds() { return (unsigned int)FractionalSyncMs; }

	// Total logic time in milliseconds. Can include fractions of a logic step. Is rounded to integer.
	static unsigned int		Get_Logic_Time_Milliseconds() { return SyncTime + (unsigned int)FractionalSyncMs; }

	// Logic time step in milliseconds. Can be a fraction of a logic step.
	static float					Get_Logic_Frame_Time_Milliseconds() { return LogicFrameTimeMs; }

	// Logic time step in seconds. Can be a fraction of a logic step.
	static float					Get_Logic_Frame_Time_Seconds() { return LogicFrameTimeMs * 0.001f; }

	// Returns the render frame count.
	static unsigned int		Get_Frame_Count() { return FrameCount; }

	static unsigned int		Get_Last_Frame_Poly_Count();
	static unsigned int		Get_Last_Frame_Vertex_Count();

	/*
	** Screen/Movie capturing
	** These functions allow you to create screenshots and movies.
	*/
	static void					Make_Screen_Shot( const char * filename = "ScreenShot", const float gamma = 1.3f, const ScreenShotFormatEnum format = TGA);
	static void					Start_Movie_Capture( const char * filename_base = "Movie", float frame_rate = 15);
	static void					Stop_Movie_Capture();
	static void					Toggle_Movie_Capture( const char * filename_base = "Movie", float frame_rate = 15);
	static void					Start_Single_Frame_Movie_Capture(const char *filename_base = "Frames");
	static void					Capture_Next_Movie_Frame();
	static void					Update_Movie_Capture();
	static float				Get_Movie_Capture_Frame_Rate();
	static void					Pause_Movie(bool mode);
	static bool					Is_Movie_Paused();
	static bool					Is_Recording_Next_Frame();
	static bool					Is_Movie_Ready();

   /*
	** Set_Ext_Swap_Interval - how many vertical retraces to wait before flipping frames
	** Get_Ext_Swap_Interval - what is our current setting for the swap interval?
	*/
	static void             Set_Ext_Swap_Interval(long swap);
   static long             Get_Ext_Swap_Interval();

	/*
	** Texture Reduction - all currently loaded textures can be de-resed on the fly
	** by passing in a non-unit value to Set_Texture_Reduction.  Passing in 2 causes
	** all textures to be half their normal resolution.  Passing in 3 causes them to
	** be cut in half twice, etc
	*/
	static void					Set_Texture_Reduction( int value, int min_dim=1 );
	static int					Get_Texture_Reduction();
	static int					Get_Texture_Min_Dimension();
	static void					Enable_Large_Texture_Extra_Reduction(bool onoff);
	static bool					Is_Large_Texture_Extra_Reduction_Enabled();
	static void					_Invalidate_Mesh_Cache();
	static void					_Invalidate_Textures();

	static void					Set_Thumbnail_Enabled(bool b) { ThumbnailEnabled=b; }
	static bool					Get_Thumbnail_Enabled() { return ThumbnailEnabled; }

	static void					Enable_Sorting(bool onoff);
	static bool					Is_Sorting_Enabled()					{ return IsSortingEnabled; }

	static void					Set_Screen_UV_Bias( bool onoff )			{ IsScreenUVBiased = onoff; }
	static bool					Is_Screen_UV_Biased()				{ return IsScreenUVBiased; }

	static void					Set_Collision_Box_Display_Mask(int mask);
	static int					Get_Collision_Box_Display_Mask();

	static void					Set_Default_Native_Screen_Size(float dnss)	{ DefaultNativeScreenSize = dnss; }
	static float				Get_Default_Native_Screen_Size()			{ return DefaultNativeScreenSize; }

	static void					Normalize_Coordinates(int x, int y, float &fx, float &fy); // convert pixel coordinates to 0..1 screen coordinates

	static VertexMaterialClass *	Peek_Default_Debug_Material();
	static ShaderClass		Peek_Default_Debug_Shader();
	static ShaderClass		Peek_Backface_Debug_Shader();
	static ShaderClass		Peek_Lightmap_Debug_Shader();

	static void					Set_Prelit_Mode (PrelitModeEnum mode)			{ PrelitMode = mode; }
	static PrelitModeEnum 	Get_Prelit_Mode ()									{ return (PrelitMode); }
	static bool					Supports_Prelit_Mode (PrelitModeEnum mode)	{ return (true); }
	static void					Expose_Prelit (bool onoff)							{ ExposePrelit = onoff; }
	static bool					Expose_Prelit ()										{ return (ExposePrelit); }

	static void					Set_Texture_Bitdepth(int bitdepth);
	static int					Get_Texture_Bitdepth();

	static void					Set_MSAA_Mode(MultiSampleModeEnum mode);
	static MultiSampleModeEnum Get_MSAA_Mode();

	static void					Set_Mesh_Draw_Mode (MeshDrawModeEnum mode)	{ MeshDrawMode = mode; }
	static MeshDrawModeEnum Get_Mesh_Draw_Mode ()								{ return (MeshDrawMode); }

	static void					Set_NPatches_Gap_Filling_Mode (NPatchesGapFillingModeEnum mode);
	static NPatchesGapFillingModeEnum 	Get_NPatches_Gap_Filling_Mode () { return (NPatchesGapFillingMode); }

	static void					Set_NPatches_Level(unsigned level);
	static unsigned			Get_NPatches_Level() { return NPatchesLevel; }

	static void					Enable_Texturing(bool b);
	static bool					Is_Texturing_Enabled() { return IsTexturingEnabled; }
	static bool					Is_Coloring_Enabled() { return (IsColoringEnabled == 0) ? false : true; }
	static void					Enable_Coloring(unsigned int color);	///<when non-zero color is passed, it will override vertex colors

	static int					Get_Last_Frame_Memory_Allocation_Count() { return LastFrameMemoryAllocations; }
	static int					Get_Last_Frame_Memory_Free_Count() { return LastFrameMemoryFrees; }

	/*
	** Decal control
	** These global settings can control whether decals are rendered at all and
	** at what distance to stop rendering/creating decals
	*/
	static void					Enable_Decals(bool onoff)					{ AreDecalsEnabled = onoff; }
	static bool					Are_Decals_Enabled()					{ return AreDecalsEnabled; }
	static void					Set_Decal_Rejection_Distance(float d)	{ DecalRejectionDistance = d; }
	static float				Get_Decal_Rejection_Distance()		{ return DecalRejectionDistance; }

	/*
	** Static sort lists. The ability to temporarily set a different static
	** sort list from the default one and a min/max sort list range is for
	** specialised uses (such as pipctuire-in-picture windows which need to
	** sort at a certain sort level). After this override is called, the
	** default sort list must be restored.
	*/
	static void					Enable_Static_Sort_Lists(bool onoff)	{ AreStaticSortListsEnabled = onoff; }
	static bool					Are_Static_Sort_Lists_Enabled()		{ return AreStaticSortListsEnabled; }
	static void					Enable_Munge_Sort_On_Load(bool onoff)	{ MungeSortOnLoad=onoff; }
	static bool					Is_Munge_Sort_On_Load_Enabled()		{ return MungeSortOnLoad; }
	static void					Add_To_Static_Sort_List(RenderObjClass *robj, unsigned int sort_level);
	static void					Render_And_Clear_Static_Sort_Lists(RenderInfoClass & rinfo);
	static void					Override_Current_Static_Sort_Lists(StaticSortListClass * sort_list);
	static void					Reset_Current_Static_Sort_Lists_To_Default();

	/*
	** Overbright modify on load - when this mode is set meshes will be
	** modified at load time. All shaders which originally had the primary
	** gradient set to MODULATE will be changed to MODULATE2X instead.
	*/
	static void					Enable_Overbright_Modify_On_Load(bool onoff)	{ OverbrightModifyOnLoad = onoff; }
	static bool					Is_Overbright_Modify_On_Load_Enabled()	{ return OverbrightModifyOnLoad; }

	static bool					Is_Snapshot_Activated()						{ return SnapshotActivated; }
	static void					Activate_Snapshot(bool b)					{ SnapshotActivated=b; }

	// These clock all the time under user control, and are used to update
   // Stats.UserStat* when performance sampling is enabled.
#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 30/09/2026 The engine's (WW3DState, below; PLAN-023 Phase 8, stage RR2a-2).
	static inline long& UserStat0_State() noexcept;
	static inline long& UserStat1_State() noexcept;
	static inline long& UserStat2_State() noexcept;
	static constexpr rts::AccessorContextField<long, &WW3D::UserStat0_State> UserStat0{};
	static constexpr rts::AccessorContextField<long, &WW3D::UserStat1_State> UserStat1{};
	static constexpr rts::AccessorContextField<long, &WW3D::UserStat2_State> UserStat2{};
#else
   static long             UserStat0;
   static long             UserStat1;
   static long             UserStat2;
#endif

	// Gamma control
	static void					Set_Gamma(float gamma,float bright,float contrast,bool calibrate=true);

private:

	enum
	{
		DEFAULT_RESOLUTION_WIDTH =			640,
		DEFAULT_RESOLUTION_HEIGHT =		480,
		DEFAULT_BIT_DEPTH =					16
	};

	static void					Read_Gerd_Render_Device_Description(RenderDeviceDescClass &desc);
	static void					Update_Pixel_Center();
	static void					Allocate_Debug_Resources();
	static void					Release_Debug_Resources();

#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 28/09/2026 The timing statics are per engine (EngineContext fields, with
	// the same initial values): each engine's animation clock advances with its own frames only (PLAN-023
	// Phase 3). The uses are unchanged.
	static constexpr rts::ContextField<float, &rts::EngineContext::ww3dLogicFrameTimeMs> LogicFrameTimeMs{};
	static constexpr rts::ContextField<float, &rts::EngineContext::ww3dFractionalSyncMs> FractionalSyncMs{};
	static constexpr rts::ContextField<unsigned int, &rts::EngineContext::ww3dSyncTime> SyncTime{};
	static constexpr rts::ContextField<unsigned int, &rts::EngineContext::ww3dPreviousSyncTime> PreviousSyncTime{};
#else
	// Logic frame time, in milliseconds
	static float LogicFrameTimeMs;

	// Accumulated synchronized frame time in milliseconds
	static float FractionalSyncMs;

	// Timing info:
	// The absolute synchronized frame time (in milliseconds) supplied by the
	// application at the start of every frame. Note that wraparound cases
	// etc. need to be considered.
	static unsigned int SyncTime;

	// The previously set absolute sync time - this is used to get the interval between
	// the most recently set sync time and the previous one. Assuming the
	// application sets sync time at the start of every frame, this represents
	// the frame interval.
	static unsigned int PreviousSyncTime;
#endif

#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 30/09/2026 The current engine's WW3DState fields (below; PLAN-023 Phase 8, stage
	// RR2a-2): each name is a stand-in for the field its accessor returns, so the uses are unchanged.
	static inline float& PixelCenterX_State() noexcept;
	static inline float& PixelCenterY_State() noexcept;
	static inline IRenderBackend*& RenderBackend_State() noexcept;
	static inline bool& IsInitted_State() noexcept;
	static inline bool& IsRendering_State() noexcept;
	static inline bool& IsCapturing_State() noexcept;
	static inline bool& IsSortingEnabled_State() noexcept;
	static inline bool& IsScreenUVBiased_State() noexcept;
	static inline bool& AreDecalsEnabled_State() noexcept;
	static inline float& DecalRejectionDistance_State() noexcept;
	static inline bool& AreStaticSortListsEnabled_State() noexcept;
	static inline bool& MungeSortOnLoad_State() noexcept;
	static inline bool& OverbrightModifyOnLoad_State() noexcept;
	static inline FrameGrabClass*& Movie_State() noexcept;
	static inline bool& PauseRecord_State() noexcept;
	static inline bool& RecordNextFrame_State() noexcept;
	static inline VertexMaterialClass*& DefaultDebugMaterial_State() noexcept;
	static inline ShaderClass& DefaultDebugShader_State() noexcept;
	static inline ShaderClass& LightmapDebugShader_State() noexcept;
	static inline PrelitModeEnum& PrelitMode_State() noexcept;
	static inline bool& ExposePrelit_State() noexcept;
	static inline int& TextureFilter_State() noexcept;
	static inline int& AnisotropyLevel_State() noexcept;
	static inline bool& SnapshotActivated_State() noexcept;
	static inline bool& ThumbnailEnabled_State() noexcept;
	static inline MeshDrawModeEnum& MeshDrawMode_State() noexcept;
	static inline NPatchesGapFillingModeEnum& NPatchesGapFillingMode_State() noexcept;
	static inline unsigned& NPatchesLevel_State() noexcept;
	static inline bool& IsTexturingEnabled_State() noexcept;
	static inline bool& IsColoringEnabled_State() noexcept;
	static inline bool& Lite_State() noexcept;
	static inline float& DefaultNativeScreenSize_State() noexcept;
	static inline StaticSortListClass*& DefaultStaticSortLists_State() noexcept;
	static inline StaticSortListClass*& CurrentStaticSortLists_State() noexcept;
	static inline int& LastFrameMemoryAllocations_State() noexcept;
	static inline int& LastFrameMemoryFrees_State() noexcept;

	static constexpr rts::AccessorContextField<float, &WW3D::PixelCenterX_State> PixelCenterX{};
	static constexpr rts::AccessorContextField<float, &WW3D::PixelCenterY_State> PixelCenterY{};
	static constexpr rts::AccessorContextField<IRenderBackend*, &WW3D::RenderBackend_State> RenderBackend{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsInitted_State> IsInitted{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsRendering_State> IsRendering{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsCapturing_State> IsCapturing{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsSortingEnabled_State> IsSortingEnabled{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsScreenUVBiased_State> IsScreenUVBiased{};
	static constexpr rts::AccessorContextField<bool, &WW3D::AreDecalsEnabled_State> AreDecalsEnabled{};
	static constexpr rts::AccessorContextField<float, &WW3D::DecalRejectionDistance_State> DecalRejectionDistance{};
	static constexpr rts::AccessorContextField<bool, &WW3D::AreStaticSortListsEnabled_State> AreStaticSortListsEnabled{};
	static constexpr rts::AccessorContextField<bool, &WW3D::MungeSortOnLoad_State> MungeSortOnLoad{};
	static constexpr rts::AccessorContextField<bool, &WW3D::OverbrightModifyOnLoad_State> OverbrightModifyOnLoad{};
	static constexpr rts::AccessorContextField<FrameGrabClass*, &WW3D::Movie_State> Movie{};
	static constexpr rts::AccessorContextField<bool, &WW3D::PauseRecord_State> PauseRecord{};
	static constexpr rts::AccessorContextField<bool, &WW3D::RecordNextFrame_State> RecordNextFrame{};
	static constexpr rts::AccessorContextField<VertexMaterialClass*, &WW3D::DefaultDebugMaterial_State> DefaultDebugMaterial{};
	static constexpr rts::AccessorContextField<ShaderClass, &WW3D::DefaultDebugShader_State> DefaultDebugShader{};
	static constexpr rts::AccessorContextField<ShaderClass, &WW3D::LightmapDebugShader_State> LightmapDebugShader{};
	static constexpr rts::AccessorContextField<PrelitModeEnum, &WW3D::PrelitMode_State> PrelitMode{};
	static constexpr rts::AccessorContextField<bool, &WW3D::ExposePrelit_State> ExposePrelit{};
	static constexpr rts::AccessorContextField<int, &WW3D::TextureFilter_State> TextureFilter{};
	static constexpr rts::AccessorContextField<int, &WW3D::AnisotropyLevel_State> AnisotropyLevel{};
	static constexpr rts::AccessorContextField<bool, &WW3D::SnapshotActivated_State> SnapshotActivated{};
	static constexpr rts::AccessorContextField<bool, &WW3D::ThumbnailEnabled_State> ThumbnailEnabled{};
	static constexpr rts::AccessorContextField<MeshDrawModeEnum, &WW3D::MeshDrawMode_State> MeshDrawMode{};
	static constexpr rts::AccessorContextField<NPatchesGapFillingModeEnum, &WW3D::NPatchesGapFillingMode_State> NPatchesGapFillingMode{};
	static constexpr rts::AccessorContextField<unsigned, &WW3D::NPatchesLevel_State> NPatchesLevel{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsTexturingEnabled_State> IsTexturingEnabled{};
	static constexpr rts::AccessorContextField<bool, &WW3D::IsColoringEnabled_State> IsColoringEnabled{};
	static constexpr rts::AccessorContextField<bool, &WW3D::Lite_State> Lite{};
	static constexpr rts::AccessorContextField<float, &WW3D::DefaultNativeScreenSize_State> DefaultNativeScreenSize{};
	static constexpr rts::AccessorContextField<StaticSortListClass*, &WW3D::DefaultStaticSortLists_State> DefaultStaticSortLists{};
	static constexpr rts::AccessorContextField<StaticSortListClass*, &WW3D::CurrentStaticSortLists_State> CurrentStaticSortLists{};
	static constexpr rts::AccessorContextField<int, &WW3D::LastFrameMemoryAllocations_State> LastFrameMemoryAllocations{};
	static constexpr rts::AccessorContextField<int, &WW3D::LastFrameMemoryFrees_State> LastFrameMemoryFrees{};

	static constexpr rts::ContextField<int, &rts::EngineContext::ww3dFrameCount> FrameCount{};

	// Declared upstream, never defined or used.
	static bool							IsBackfaceDebugEnabled;
	static VertexMaterialClass *	BackfaceDebugMaterial;
#else
	static float						PixelCenterX;
	static float						PixelCenterY;

	static IRenderBackend *		RenderBackend;

	static bool							IsInitted;
	static bool							IsRendering;
	static bool							IsCapturing;
	static bool							IsSortingEnabled;
	static bool							IsScreenUVBiased;
	static bool							IsBackfaceDebugEnabled;

	static bool							AreDecalsEnabled;
	static float						DecalRejectionDistance;

	static bool							AreStaticSortListsEnabled;
	static bool							MungeSortOnLoad;

	static bool							OverbrightModifyOnLoad;

	static FrameGrabClass *			Movie;
	static bool							PauseRecord;
	static bool							RecordNextFrame;
#if RTS_ENGINE_CONTEXT
	static constexpr rts::ContextField<int, &rts::EngineContext::ww3dFrameCount> FrameCount{};
#else
	static int							FrameCount;
#endif

	static VertexMaterialClass *	DefaultDebugMaterial;
	static VertexMaterialClass *	BackfaceDebugMaterial;
	static ShaderClass				DefaultDebugShader;
	static ShaderClass				LightmapDebugShader;

	static PrelitModeEnum			PrelitMode;
	static bool							ExposePrelit;

	static int							TextureFilter;
	static int							AnisotropyLevel;

	static bool							SnapshotActivated;
	static bool							ThumbnailEnabled;

	static MeshDrawModeEnum			MeshDrawMode;
	static NPatchesGapFillingModeEnum NPatchesGapFillingMode;
	static unsigned NPatchesLevel;
	static bool							IsTexturingEnabled;
	static bool							IsColoringEnabled;

	static bool							Lite;

	// This is the default native screen size which will be set for each
	// RenderObject on construction. The native screen size is the screen size
	// at which the object was designed to be viewed, and it is used in the
	// texture resizing algorithm (may be used in future for other things).
	// If the default is overridden, it will usually be in the asset manager
	// post-load callback.
	static float						DefaultNativeScreenSize;

	// For meshes which have a static sorting order. These will get drawn
	// after opaque meshes and before normally sorted meshes. The 'current'
	// pointer is so the application can temporarily set a different set of
	// static sort lists to be used temporarily. This is for specialised uses.
	static StaticSortListClass * DefaultStaticSortLists;
	static StaticSortListClass * CurrentStaticSortLists;

	// Memory allocation statistics
	static int							LastFrameMemoryAllocations;
	static int							LastFrameMemoryFrees;
#endif
};

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 30/09/2026 One render engine's WW3D state (PLAN-023 Phase 8, stage RR2a-2)
//
// WW3D's mutable statics (the render settings and render-loop state its Init, Shutdown, render loop, static sort
// lists and options write) and ww3d.cpp's file statics, as one struct with the statics' names and upstream initial
// values. Each render engine owns one, rts::EngineContext::ww3dState, allocated beside its W3DRenderState
// (DX8Wrapper::Create_Render_State) and freed with it at the end of WW3D::Shutdown; an engine that does not render
// reads WW3DState::Defaults, which nothing may write (W3D_Protect_Render_Defaults makes its pages read-only). Page
// aligned, so the defaults fill whole pages of their own.
struct RTS_ENGINE_CONTEXT_API alignas(::rts::renderStateAlignment) WW3DState
{
	// ww3d.cpp: the two debug shaders' bits and the texture filter defaults (TextureFilterClass) are its own.
	WW3DState();

	static WW3DState Defaults;

	bool IsSortingEnabled = true;

	float PixelCenterX = 0.0f;
	float PixelCenterY = 0.0f;

	IRenderBackend* RenderBackend = nullptr;

	bool IsInitted = false;
	bool IsRendering = false;
	bool IsCapturing = false;
	bool IsScreenUVBiased = false;

	bool AreDecalsEnabled = true;
	float DecalRejectionDistance = 1000000.0f;

	bool AreStaticSortListsEnabled = false;
	bool MungeSortOnLoad = false;

	bool OverbrightModifyOnLoad = false;

	FrameGrabClass* Movie = nullptr;
	bool PauseRecord = false;
	bool RecordNextFrame = false;

	long UserStat0 = 0;
	long UserStat1 = 0;
	long UserStat2 = 0;

	float DefaultNativeScreenSize = 1.0f;

	StaticSortListClass* DefaultStaticSortLists = nullptr;
	StaticSortListClass* CurrentStaticSortLists = nullptr;

	VertexMaterialClass* DefaultDebugMaterial = nullptr;
	ShaderClass DefaultDebugShader;
	ShaderClass LightmapDebugShader;

	WW3D::PrelitModeEnum PrelitMode = WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS;
	bool ExposePrelit = false;

	bool SnapshotActivated = false;
	bool ThumbnailEnabled = true;

	WW3D::MeshDrawModeEnum MeshDrawMode = WW3D::MESH_DRAW_MODE_OLD;
	WW3D::NPatchesGapFillingModeEnum NPatchesGapFillingMode = WW3D::NPATCHES_GAP_FILLING_ENABLED;
	unsigned NPatchesLevel = 1;
	bool IsTexturingEnabled = true;
	bool IsColoringEnabled = false;

	int LastFrameMemoryAllocations = 0;
	int LastFrameMemoryFrees = 0;

	int TextureFilter;
	int AnisotropyLevel;

	bool Lite = false;

	// ww3d.cpp's file statics (an HWND), and Make_Screen_Shot's file number.
	void* _Hwnd = nullptr;
	int _TextureReduction = 0;
	int _TextureMinDim = 1;
	bool _LargeTextureExtraReductionEnabled = false;
	int ScreenShotFrameNumber = 1;
};

// The current engine's WW3D state, or WW3DState::Defaults for an engine that has none.
inline WW3DState& WW3D_State() noexcept
{
	return ::rts::indirectContext<WW3DState, &::rts::EngineContext::ww3dState, WW3DState::Defaults>();
}

#define WW3D_STATE_ACCESSOR(name) \
	inline decltype(WW3DState::name)& WW3D::name##_State() noexcept { return WW3D_State().name; }
WW3D_STATE_ACCESSOR(UserStat0)
WW3D_STATE_ACCESSOR(UserStat1)
WW3D_STATE_ACCESSOR(UserStat2)
WW3D_STATE_ACCESSOR(PixelCenterX)
WW3D_STATE_ACCESSOR(PixelCenterY)
WW3D_STATE_ACCESSOR(RenderBackend)
WW3D_STATE_ACCESSOR(IsInitted)
WW3D_STATE_ACCESSOR(IsRendering)
WW3D_STATE_ACCESSOR(IsCapturing)
WW3D_STATE_ACCESSOR(IsSortingEnabled)
WW3D_STATE_ACCESSOR(IsScreenUVBiased)
WW3D_STATE_ACCESSOR(AreDecalsEnabled)
WW3D_STATE_ACCESSOR(DecalRejectionDistance)
WW3D_STATE_ACCESSOR(AreStaticSortListsEnabled)
WW3D_STATE_ACCESSOR(MungeSortOnLoad)
WW3D_STATE_ACCESSOR(OverbrightModifyOnLoad)
WW3D_STATE_ACCESSOR(Movie)
WW3D_STATE_ACCESSOR(PauseRecord)
WW3D_STATE_ACCESSOR(RecordNextFrame)
WW3D_STATE_ACCESSOR(DefaultDebugMaterial)
WW3D_STATE_ACCESSOR(DefaultDebugShader)
WW3D_STATE_ACCESSOR(LightmapDebugShader)
WW3D_STATE_ACCESSOR(PrelitMode)
WW3D_STATE_ACCESSOR(ExposePrelit)
WW3D_STATE_ACCESSOR(TextureFilter)
WW3D_STATE_ACCESSOR(AnisotropyLevel)
WW3D_STATE_ACCESSOR(SnapshotActivated)
WW3D_STATE_ACCESSOR(ThumbnailEnabled)
WW3D_STATE_ACCESSOR(MeshDrawMode)
WW3D_STATE_ACCESSOR(NPatchesGapFillingMode)
WW3D_STATE_ACCESSOR(NPatchesLevel)
WW3D_STATE_ACCESSOR(IsTexturingEnabled)
WW3D_STATE_ACCESSOR(IsColoringEnabled)
WW3D_STATE_ACCESSOR(Lite)
WW3D_STATE_ACCESSOR(DefaultNativeScreenSize)
WW3D_STATE_ACCESSOR(DefaultStaticSortLists)
WW3D_STATE_ACCESSOR(CurrentStaticSortLists)
WW3D_STATE_ACCESSOR(LastFrameMemoryAllocations)
WW3D_STATE_ACCESSOR(LastFrameMemoryFrees)
#undef WW3D_STATE_ACCESSOR
#endif // RTS_ENGINE_CONTEXT


/*
** RenderStatistics
** This struct holds the results of a performance sampling.  The WW3D object returns
** its statistics packaged up in one of these structures.
*/
struct RenderStatistics
{
		// General statistics
		double	ElapsedSeconds;
      int      FramesRendered;

		// Geometry engine statistics
		double	TrianglesReceived;
		double	TrianglesSubmitted;
		double	TrianglesSorted;
		double	VerticesReceived;
		double	VerticesSubmitted;

		// State change statistics
		double	ViewStateChanges;
		double	DrawStateChanges;
		double	TextureChanges;
		double	TextureParameterChanges;
		double	TexturesCreated;
		double	PaletteChanges;
		double	ShaderChanges;
		double	DrawCommands;
		double	TrianglesClipRemoved;
		double	TrianglesClipCreated;
		double	DeviceDriverCalls;

		// Rendering device statistics
		double	TextureTransfers;
		double	PixelsDrawn;
		double	PixelsRejected;

		// Surface cache statistics
		long		Hits;
		long		Misses;
		long		Insertions;
		long		Removals;
		long		MemUsed;
		long		MaxMemory;

      // User stats (can be used to see how often a function is called, etc.)
      long     UserStat0;
      long     UserStat1;
      long     UserStat2;
};
