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

// FILE: W3DParticleSys.h /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/ParticleSys.h"
#include "WW3D2/pointgr.h"
#include "WW3D2/streak.h"
#include "WW3D2/rinfo.h"
#include "WWLib/bittype.h"

//=============================================================================
/** W3D implementation of the game display which is responsible for creating
  * all interaction with the screen and updating the display
	*/
class W3DParticleSystemManager : public ParticleSystemManager
{

public:
	W3DParticleSystemManager();
	virtual ~W3DParticleSystemManager() override;

	virtual void doParticles(RenderInfoClass &rinfo) override;
	virtual void queueParticleRender() override;
	///< returns the number of particles shown on screen per frame
	virtual Int getOnScreenParticleCount() override { return m_onScreenParticleCount; }
#if RTS_ENGINE_CONTEXT
	// GeneralsX @feature cemlyn007 21/10/2026 The client tick's count of the field particles in a camera's view
	// (Common/EngineDrawing.h)
	void countFieldParticlesInView(CameraClass &camera);
	// GeneralsX @feature cemlyn007 21/10/2026 A player's particle simulation (ParticleSystemManager::beginSimulations)
	virtual ParticleSystemManager *newSimulation() const override { return NEW W3DParticleSystemManager; }
	// GeneralsX @bugfix cemlyn007 21/10/2026 A reset lets go of the textures the last draw left on the point group and
	// the streaks, in every simulation, so that none outlives the render device: the engine's teardown resets every
	// subsystem before it releases the device, but deletes this manager only after
	virtual void reset() override;
#endif

private:
	Bool finishedBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture);
	void initializeBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture);
	void flushParticleBatch(RenderInfoClass& rinfo, UnsignedInt& pointCount);
#if RTS_ENGINE_CONTEXT
	void releaseTextures();
#endif

	enum { MAX_POINTS_PER_GROUP = 512 };

	RefCountPtr<TextureClass> m_batchTexture;		///< the texture used as the drawing surface for batched particle draws
	PointGroupClass *m_pointGroup;							///< the point group that contains all of the particles
	StreakLineClass *m_streakLine;							///< the streak class that contains all of the streaks
	ShareBufferClass<Vector3> *m_posBuffer;			///< array of particle positions
	ShareBufferClass<Vector4> *m_RGBABuffer;		///< array of particle color and alpha
	ShareBufferClass<float> *m_sizeBuffer;			///< array of particle sizes
	ShareBufferClass<uint8> *m_angleBuffer;			///< array of particle orientations

	ParticleSystemInfo::ParticleShaderType m_batchShaderType;
	Bool m_readyToRender;											///< if true, it is OK to render
	ParticleSystemInfo::ParticleAlignmentType m_batchParticleAlignment;
};
