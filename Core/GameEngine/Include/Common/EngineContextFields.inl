// FILE: EngineContextFields.inl
// GeneralsX @feature cemlyn007 03/10/2026 The direct EngineContext fields (PLAN-023 Phases 2-3)
//
// X-macro list of the EngineContext fields other than the singletons (Common/EngineSingletons.inl), the
// RNG seeds (the three arrays and gameLogicBaseSeed) and originalGlobalData. Common/EngineContext.h
// declares them from this list, Common/EngineContext.cpp enumerates the pointer fields for the lifecycle
// checks and compares the value fields with their initial values for noEngineIsPristine(), so a field
// added here is covered by both. The seeds and originalGlobalData are checked by hand in
// noEngineIsPristine() and ~EngineContext(); any other field added outside this list is not checked.
//
//   RTS_ENGINE_CONTEXT_POINTER(Class, name)      -- `Class* name = nullptr`, Class declared with `class`,
//                                                   set by the engine and freed and nulled at its teardown
//   RTS_ENGINE_CONTEXT_VALUE(Type, name, init)   -- `Type name = init`

// Set while this engine is being torn down (GameEngine's destructor onwards), cleared when a GameEngine is
// constructed in it. Fatal errors raised in that window do not throw (see FatalEngineError.h).
RTS_ENGINE_CONTEXT_VALUE(bool, engineTearingDown, false)

// Set while this engine boots from names another engine primed, until its upgrades are loaded: it must
// intern no new NameKey then (NameKeyGenerator::PrimingLatch, PLAN-023 Decision 2).
RTS_ENGINE_CONTEXT_VALUE(bool, nameKeysFrozen, false)

// PathfindCellInfo::s_infoArray/s_firstFree: the pathfinder's cell-info pool and its free list
// (AIPathfind.cpp), made and freed by this engine's Pathfinder.
RTS_ENGINE_CONTEXT_POINTER(PathfindCellInfo, pathfindCellInfoArray)
RTS_ENGINE_CONTEXT_POINTER(PathfindCellInfo, pathfindCellInfoFirstFree)

// AIPathfind.cpp's RETAIL_COMPATIBLE_PATHFINDING s_useFixedPathfinding/s_forceCleanCells: set when this
// engine's own cell-info pool (above) runs out, so a RETAIL_COMPATIBLE_PATHFINDING build's pool-exhaustion
// fallback stays this engine's own and never force-cleans another engine's healthy pool. Unused outside that
// build option, but kept unconditional (two bools) rather than guarded on it, since this header is not where
// RETAIL_COMPATIBLE_PATHFINDING is defined.
RTS_ENGINE_CONTEXT_VALUE(bool, pathfindUseFixedPathfinding, false)
RTS_ENGINE_CONTEXT_VALUE(bool, pathfindForceCleanCells, false)

// PolygonTrigger::ThePolygonTriggerListPtr/s_currentID: the map's trigger areas, walked on every object's
// cell change, and the next trigger ID.
RTS_ENGINE_CONTEXT_POINTER(PolygonTrigger, polygonTriggerList)
RTS_ENGINE_CONTEXT_VALUE(std::int32_t, polygonTriggerCurrentID, 1)

// MapObject::TheMapObjectListPtr: the map objects of the last map this engine read.
RTS_ENGINE_CONTEXT_POINTER(MapObject, mapObjectList)

// PartitionManager.cpp's TheContactList (the contact list of the partition update in progress) and
// getClosestObjects()'s iteration stamp (nonzero).
RTS_ENGINE_CONTEXT_POINTER(PartitionContactList, partitionContactList)
RTS_ENGINE_CONTEXT_VALUE(std::int32_t, partitionIterFlag, 1)

// ScriptList::m_curId: the last script ID handed out.
RTS_ENGINE_CONTEXT_VALUE(std::int32_t, scriptListCurId, 0)

// REPLAY_CRC_INTERVAL (Recorder.cpp): the logic CRC interval of a solo game or replay.
RTS_ENGINE_CONTEXT_VALUE(std::int32_t, replayCrcInterval, 100)

// W3DDisplay's scenes and asset manager (W3DDisplay::m_3DScene, ...; PLAN-023 Phase 3). Headless builds
// only the asset manager, and each engine's GameClient::reset resets its own.
RTS_ENGINE_CONTEXT_POINTER(RTS3DScene, w3dDisplay3DScene)
RTS_ENGINE_CONTEXT_POINTER(RTS2DScene, w3dDisplay2DScene)
RTS_ENGINE_CONTEXT_POINTER(RTS3DInterfaceScene, w3dDisplay3DInterfaceScene)
RTS_ENGINE_CONTEXT_POINTER(W3DAssetManager, w3dDisplayAssetManager)

// WW3DAssetManager::TheInstance: the asset manager the W3D loaders use (bones and meshes, headless too);
// the same object as w3dDisplayAssetManager once W3DDisplay has made it.
RTS_ENGINE_CONTEXT_POINTER(WW3DAssetManager, ww3dAssetManager)

// WW3D's timing statics (WW3D::SyncTime, ...), with their upstream initial values: the animation clock
// (bones are posed headless too), advanced by this engine's frames only. ww3d.cpp asserts that the initial
// 1000.0f / 30 is 1000.0f / WWSyncPerSecond.
RTS_ENGINE_CONTEXT_VALUE(float, ww3dLogicFrameTimeMs, 1000.0f / 30)
RTS_ENGINE_CONTEXT_VALUE(float, ww3dFractionalSyncMs, 0.0f)
RTS_ENGINE_CONTEXT_VALUE(unsigned int, ww3dSyncTime, 0)
RTS_ENGINE_CONTEXT_VALUE(unsigned int, ww3dPreviousSyncTime, 0)
RTS_ENGINE_CONTEXT_VALUE(int, ww3dFrameCount, 0)

// Whether this engine holds one of WWMath's Init counts (wwmath.cpp): set by its first WWMath::Init and
// cleared by its first WWMath::Shutdown after that, so an engine that calls Shutdown twice (W3DDisplay's
// failed init, then its destructor) releases only its own count, never another live engine's.
RTS_ENGINE_CONTEXT_VALUE(bool, wwMathInitialized, false)
