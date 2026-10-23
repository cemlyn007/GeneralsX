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

// FILE: Display.h ////////////////////////////////////////////////////////////
// The graphics display singleton
// Author: Michael S. Booth, March 2001

#pragma once

#include "Common/SubsystemInterface.h"
#include "GameClient/Color.h"
#include "GameClient/GameFont.h"
#include "GameClient/View.h"
#include <vector>	// GeneralsX @feature cemlyn007 21/10/2026 The players' views

enum ScreenshotFormat
{
	SCREENSHOT_JPEG,
	SCREENSHOT_PNG,

	SCREENSHOT_FORMAT_COUNT
};

constexpr const Int DEFAULT_JPEG_QUALITY = 80;

struct ShroudLevel
{
	Short m_currentShroud;		///< A Value of 1 means shrouded.  0 is not.  Negative is the count of people looking.
	Short m_activeShroudLevel;///< A Value of 0 means passive shroud.  Positive is the count of people shrouding.
};

class VideoBuffer;
class VideoStreamInterface;
class DebugDisplayInterface;
class Radar;
class Image;
class DisplayString;
enum StaticGameLODLevel CPP_11(: Int);
/**
 * The Display class implements the Display interface
 */
class Display : public SubsystemInterface
{

public:
	enum DrawImageMode
	{
		DRAW_IMAGE_SOLID,
		DRAW_IMAGE_GRAYSCALE,		//draw image without blending and ignoring alpha
		DRAW_IMAGE_ALPHA,		//alpha blend the image into frame buffer
		DRAW_IMAGE_ADDITIVE	//additive blend the image into frame buffer
	};

	typedef void (DebugDisplayCallback)( DebugDisplayInterface *debugDisplay, void *userData, FILE *fp );

	Display();
	virtual ~Display() override;

	virtual void init() override { };																///< Initialize
	virtual void reset() override;																		///< Reset system
	virtual void update() override;																	///< Update system

	//---------------------------------------------------------------------------------------
	// Display attribute methods
	virtual void setWidth( UnsignedInt width );										///< Sets the width of the display
	virtual void setHeight( UnsignedInt height );									///< Sets the height of the display
	virtual UnsignedInt getWidth() { return m_width; }			///< Returns the width of the display
	virtual UnsignedInt getHeight() { return m_height; }		///< Returns the height of the display
	virtual void setBitDepth( UnsignedInt bitDepth ) { m_bitDepth = bitDepth; }
	virtual UnsignedInt getBitDepth() { return m_bitDepth; }
	virtual void setWindowed( Bool windowed ) { m_windowed = windowed; }  ///< set windowed/fullscreen flag
	virtual Bool getWindowed() { return m_windowed; }				///< return widowed/fullscreen flag
	virtual Bool getViewportRect( Int& x, Int& y, Int& width, Int& height ) const { return FALSE; }  ///< pillarbox/letterbox viewport in logical pixels
	virtual Bool setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed );	///<sets screen resolution/mode
	virtual Int getDisplayModeCount() {return 0;}	///<return number of display modes/resolutions supported by video card.
	virtual void getDisplayModeDescription(Int modeIndex, Int *xres, Int *yres, Int *bitDepth) {}	///<return description of mode
 	virtual void setGamma(Real gamma, Real bright, Real contrast, Bool calibrate) {};
	virtual Bool testMinSpecRequirements(Bool *videoPassed, Bool *cpuPassed, Bool *memPassed,StaticGameLODLevel *idealVideoLevel=nullptr, Real *cpuTime=nullptr) {*videoPassed=*cpuPassed=*memPassed=true; return true;}
	virtual void doSmartAssetPurgeAndPreload(const char* usageFileName) = 0;
#if defined(RTS_DEBUG)
	virtual void dumpAssetUsage(const char* mapname) = 0;
#endif

	//---------------------------------------------------------------------------------------
	// View management
	virtual void attachView( View *view );												///< Attach the given view to the world
	virtual View *getFirstView() { return m_viewList; }				///< Return the first view of the world
	virtual View *getNextView( View *view )
	{
		if( view )
			return view->getNextView();
		return nullptr;
	}

	virtual void drawViews();																///< Render all views of the world
	virtual void updateViews ();															///< Updates state of world views
	virtual void stepViews(); ///< Update views for every fixed time step

	virtual VideoBuffer*	createVideoBuffer() = 0;							///< Create a video buffer that can be used for this display

	//---------------------------------------------------------------------------------------
	// Drawing management
	virtual void setClipRegion( IRegion2D *region ) = 0;	///< Set clip rectangle for 2D draw operations.
	virtual	Bool isClippingEnabled() = 0;
	virtual	void enableClipping( Bool onoff ) = 0;

	// TheSuperHackers @performance Batching 2D draw operations to reduce state changes and draw call overhead.
	virtual void beginBatch(); 									///< start batching 2D draw operations.
	virtual void endBatch();   									///< stop batching and flush pending 2D draw operations.
	virtual void flush();      									///< flush pending 2D draw operations without ending the batch.
	virtual Bool isBatching() const { return m_isBatching; }	///< returns true if currently batching 2D draw operations.

	virtual void step() {}; ///< Do one fixed time step
#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 21/10/2026 Advance the client by one logic frame, for an engine that draws on demand
	// (Common/EngineDrawing.h)
	virtual void tickClient() {}

	// GeneralsX @feature cemlyn007 21/10/2026 Per-player views, for a host that sets the context's playerViews
	// (docs/EMBEDDING.md, "Per-player views"). Each game's start gives every slot's player a view of its own
	// (GameLogic::startNewGame), kept here beside the tactical view, which stays the view of the game's local player: the
	// watcher's, for a host that anchors its local player. Every player's view is updated and stepped with the tactical
	// view, so its camera moves by logic-frame time whether or not it is drawn, and none of them is ever drawn but by
	// drawPlayerView, which draws one view as its player sees the game (the tactical view for a player index of -1) and
	// leaves the client as it found it.
	void endPlayerViews();
	void addPlayerView( Int playerIndex, View *view );
	View *findPlayerView( Int playerIndex ) const;
	void drawPlayerView( Int playerIndex );
	// GeneralsX @feature cemlyn007 23/10/2026 GeneralsX shakes its tactical view where its observed or local player can see
	// the shake made (ViewShakeFXNugget). After the tactical view's shake, this shakes the view of each player among
	// viewers, one bit per player index (effectViewers), in the tactical view's direction (View::shakeLike)
	void shakePlayerViews( const Coord3D *epicenter, View::CameraShakeType shakeType, UnsignedInt viewers );

	// GeneralsX @feature cemlyn007 21/10/2026 Per-player terrain shrouds, beside the per-player views: a display that draws
	// gives every slot's player a terrain shroud of its own at each game's start (W3DDevice/GameClient/BaseHeightMap.h,
	// addPlayerShroud), which the partition manager keeps up to date (setPlayerShroudLevel) and drawPlayerView draws the
	// player's view with (drawWithShroudOf). A display that does not draw keeps none.
	// GeneralsX @feature cemlyn007 23/10/2026 A draw with a player's shroud also draws the player's own scorch marks and
	// only the light pulses the player could see made (effectViewers)
	virtual void addPlayerShroud( Int playerIndex ) {}
	virtual void endPlayerShrouds() {}
	virtual Bool hasPlayerShroud( Int playerIndex ) const { return FALSE; }
	virtual void setPlayerShroudLevel( Int playerIndex, Int x, Int y, CellShroudStatus setting ) {}
	virtual void drawWithShroudOf( Int playerIndex ) {}
#endif
	virtual void draw() override;																		///< Redraw the entire display
	virtual void setTimeOfDay( TimeOfDay tod ) = 0;								///< Set the time of day for this display
	virtual void createLightPulse( const Coord3D *pos, const RGBColor *color, Real innerRadius,Real attenuationWidth,
																 UnsignedInt increaseFrameTime, UnsignedInt decayFrameTime//, Bool donut = FALSE
																 ) = 0;

	/// draw a line on the display in pixel coordinates with the specified color
	virtual void drawLine( Int startX, Int startY, Int endX, Int endY,
												 Real lineWidth, UnsignedInt lineColor ) = 0;
	/// draw a line on the display in pixel coordinates with the specified 2 colors
	virtual void drawLine( Int startX, Int startY, Int endX, Int endY,
												 Real lineWidth, UnsignedInt lineColor1, UnsignedInt lineColor2 ) = 0;
	/// draw a rect border on the display in pixel coordinates with the specified color
	virtual void drawOpenRect( Int startX, Int startY, Int width, Int height,
														 Real lineWidth, UnsignedInt lineColor ) = 0;
	/// draw a filled rect on the display in pixel coords with the specified color
	virtual void drawFillRect( Int startX, Int startY, Int width, Int height,
														 UnsignedInt color ) = 0;

	/// Draw a percentage of a rectangle, much like a clock
	virtual void drawRectClock(Int startX, Int startY, Int width, Int height, Int percent, UnsignedInt color) = 0;
	virtual void drawRemainingRectClock(Int startX, Int startY, Int width, Int height, Int percent, UnsignedInt color) = 0;

	/// draw an image fit within the screen coordinates
	virtual void drawImage( const Image *image, Int startX, Int startY,
													Int endX, Int endY, Color color = 0xFFFFFFFF, DrawImageMode mode=DRAW_IMAGE_ALPHA) = 0;

	/// draw a video buffer fit within the screen coordinates
	virtual void drawScaledVideoBuffer( VideoBuffer *buffer, VideoStreamInterface *stream ) = 0;
	virtual void drawVideoBuffer( VideoBuffer *buffer, Int startX, Int startY,
													Int endX, Int endY ) = 0;

	/// FullScreen video playback
	virtual void playMovie( AsciiString movieName );
	virtual void stopMovie();
	virtual Bool isMoviePlaying();

	/// Register debug display callback
	virtual void setDebugDisplayCallback( DebugDisplayCallback *callback, void *userData = nullptr  );
	virtual DebugDisplayCallback *getDebugDisplayCallback();

	virtual void setShroudLevel(Int x, Int y, CellShroudStatus setting ) = 0;	  ///< set shroud
	virtual void clearShroud() = 0;														///< empty the entire shroud
	virtual void setBorderShroudLevel(UnsignedByte level) = 0;	///<color that will appear in unused border terrain.

#if defined(RTS_DEBUG)
	virtual void dumpModelAssets(const char *path) = 0;	///< dump all used models/textures to a file.
#endif
	virtual void preloadModelAssets( AsciiString model ) = 0;	///< preload model asset
	virtual void preloadTextureAssets( AsciiString texture ) = 0;	///< preload texture asset

	virtual void takeScreenShot(ScreenshotFormat format, Int jpegQuality = DEFAULT_JPEG_QUALITY) = 0;			///< saves screenshot in specified format
	virtual void toggleMovieCapture() = 0;							///< starts saving frames to an avi or frame sequence
	virtual void toggleLetterBox() = 0;										///< enabled letter-boxed display
	virtual void enableLetterBox(Bool enable) = 0;						///< forces letter-boxed display on/off
	virtual Bool isLetterBoxFading() { return FALSE; }	///< returns true while letterbox fades in/out
	virtual Bool isLetterBoxed() { return FALSE; }	//WST 10/2/2002. Added query interface

	virtual void setCinematicText( AsciiString string ) { m_cinematicText = string; }
	virtual void setCinematicFont( GameFont *font ) { m_cinematicFont = font; }
	virtual void setCinematicTextFrames( Int frames ) { m_cinematicTextFrames = frames; }

	virtual Real getAverageFPS() = 0;	///< returns the average FPS.
	virtual Real getCurrentFPS() = 0;	///< returns the current FPS.
	virtual Int getLastFrameDrawCalls() = 0;  ///< returns the number of draw calls issued in the previous frame

protected:
	virtual void onBeginBatch() { }
	virtual void onEndBatch() { }
	virtual void onFlush() { }

	virtual void deleteViews();   ///< delete all views
	UnsignedInt m_width, m_height;			///< Dimensions of the display
	UnsignedInt m_bitDepth;							///< bit depth of the display
	Bool m_windowed;										///< TRUE when windowed, FALSE when fullscreen
	Bool m_isBatching;
	View *m_viewList;										///< All of the views into the world
#if RTS_ENGINE_CONTEXT
	std::vector<View *> m_playerViews;	///< GeneralsX @feature cemlyn007 21/10/2026 each player's view, by player index
#endif

	// Cinematic text data
	AsciiString m_cinematicText;        ///< string of the cinematic text that should be displayed
	GameFont *m_cinematicFont;           ///< font for cinematic text
	Int m_cinematicTextFrames;          ///< count of how long the cinematic text should be displayed

	// Video playback data
	VideoBuffer						*m_videoBuffer;						///< Video playback buffer
	VideoStreamInterface	*m_videoStream;						///< Video stream;
	AsciiString						 m_currentlyPlayingMovie;	///< The currently playing video. Used to notify TheScriptEngine of completed videos.

	// Debug display data
	DebugDisplayInterface *m_debugDisplay;					///< Actual debug display
	DebugDisplayCallback	*m_debugDisplayCallback;	///< Code to update the debug display
	void									*m_debugDisplayUserData;	///< Data for debug display update handler
	Real	m_letterBoxFadeLevel;	///<tracks the current alpha level for fading letter-boxed mode in/out.
	Bool	m_letterBoxEnabled;		///<current state of letterbox
	UnsignedInt	m_letterBoxFadeStartTime;		///< time of letterbox fade start
};

// the singleton
#if !RTS_ENGINE_CONTEXT
extern Display *TheDisplay;
#endif

extern void StatDebugDisplay( DebugDisplayInterface *dd, void *, FILE *fp = nullptr );

//Necessary for display resolution confirmation dialog box
//Holds the previous and current display settings
typedef struct _DisplaySettings
{
	Int xRes;  //Resolution width
	Int yRes;  //Resolution height
	Int bitDepth; //Color Depth
	Bool windowed; //Window mode TRUE: we're windowed, FALSE: we're not windowed
} DisplaySettings;
