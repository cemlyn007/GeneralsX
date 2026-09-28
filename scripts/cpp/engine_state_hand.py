# GeneralsX @feature cemlyn007 28/09/2026 The hand classifications for engine_state_symbols.py (PLAN-023 Phase 1)
#
# Written by reading the code. Each entry is (class, phase, matcher, note); first match wins, before
# engine_state_symbols.py's rules. A per-engine entry carries the PLAN-023 phase that fixes it (2, 3 or 4)
# and a one-line fix note; every other entry says why it is what it is. The matcher is the exact demangled
# key (compiler `.N` suffixes stripped), `re:` + a regular expression that must match the whole key, or
# `file:` + a regular expression searched for in the source column (path:line).
#
# "Headless" below means an rlgenerals engine: GlobalData::m_headless without m_headlessRender. Such an
# engine still runs GameClient::update (so the message translators, InGameUI::update and the drawables),
# but W3DDisplay::draw returns at once, the window manager is GameWindowManagerDummy (no .wnd parse, no
# window callbacks), the radar is RadarDummy and the tactical view is ViewDummy. At most one engine per
# process renders (PLAN-023 consumer answer 2), so what only that engine reaches is `render-only`.
#
# A per-engine note that starts "threads only" names state that is correct for several engines stepped on
# ONE thread (a scratch buffer filled and consumed inside one call, a call-depth counter) and only breaks
# once engines step concurrently: PLAN-023's delivery scope defers those, so they are listed for the
# threaded follow-up rather than for Phases 2-4 of this delivery.

PER = "per-engine"
GLOBAL = "process-global"
CONST = "constant"
DEBUG = "debug-only"
RENDER = "render-only"
UNREVIEWED = "unreviewed"

UI = (
    "UI only: menus, window callbacks and gadgets run only with a real window manager, and a headless "
    "engine has GameWindowManagerDummy (no .wnd parse, no callbacks); one UI engine per process"
)
NET = (
    "multiplayer network/online state: reached only from the LAN/online menus and networked games; a "
    "headless skirmish opens no network (PLAN-023 'Things checked'), and one networked engine per process"
)
W3D_RENDER = "W3D render path (device, shaders, draw lists): only the one rendering engine per process reaches it (PLAN-023 Phase 8)"
LOADER = "stateless W3D prototype loader/persist factory, registered once at static initialisation"
PER_ENGINE_STATIC = "cached per-engine pointer (template/image/button looked up once): PER_ENGINE_STATIC slot"
AUDIO_SCRATCH = "static AudioEventRTS: make it a local (PLAN-023 Phase 4; also removes a lock-taking exit destructor)"

HAND = [
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 2: simulation statics.
    (PER, 2, "PathfindCellInfo::s_infoArray", "blocker at creation: a second Pathfinder frees the first's pool; make it a Pathfinder member (TheAI->pathfinder())"),
    (PER, 2, "PathfindCellInfo::s_firstFree", "the pool's free list, with s_infoArray: Pathfinder member"),
    (PER, 2, "PolygonTrigger::ThePolygonTriggerListPtr", "blocker: cleared on every map load, walked on every cell change; per-engine list owned by TerrainLogic"),
    (PER, 2, "PolygonTrigger::s_currentID", "trigger ID counter, with the list: TerrainLogic member"),
    (PER, 2, "MapObject::TheMapObjectListPtr", "blocker for resets: rebuilt on every map load; EngineContext field"),
    (PER, 2, "MapObject::TheWorldDict", "the map's world dict (weather, ...), with the map-object list: EngineContext field"),
    (PER, 2, "re:theGame(Logic|Client|Audio)Seed|theGameLogicBaseSeed", "blocker: one engine's InitRandom reseeds every engine; EngineContext fields (only RandomValue.cpp changes)"),
    (PER, 2, "TheContactList", "partition-manager scratch list, set and consumed per update: PartitionManager member"),
    (PER, 2, "re:PartitionManager::getClosestObjects\\(.*\\)::theIterFlag", "iteration stamp; safe on one thread (every call takes a fresh value), a PartitionManager member before threads (not thread_local)"),
    (PER, 2, "re:State::friend_check(For|ForSleep)Transitions\\(StateReturnType\\)::checkfortransitionsnum", "threads only: a call-depth counter, correct on one thread; thread_local later (PLAN-023 delivery scope)"),
    (PER, 2, "ScriptList::s_readLists", "map-parse scratch: concurrent map loads swap scripts; per-parse context or SidesList member"),
    (PER, 2, "ScriptList::s_numInReadList", "map-parse scratch, with s_readLists"),
    (PER, 2, "ScriptList::m_curId", "script ID counter carried from one engine's map load into the next: reset per load or ScriptEngine member"),
    (PER, 2, "static_readPlayerNames", "SidesList parse scratch (player names read from the map): SidesList member"),
    (PER, 2, "re:s_mt(Script|Group)", "xfer recovery scratch shared by concurrent loads: make locals"),
    (PER, 2, "re:m_width|m_height|m_borderSize|m_boundaries|m_data|worldDict|m_waypoints|m_supplyPositions|m_techPositions|m_mapDX|m_mapDY", "MapUtil map-parse scratch filled by loadMap() on every MapCache miss: a stack-local parser object"),
    (PER, 2, "s_failedMapLookups", "MapUtil negative-lookup cache: MapCache member (or one read-only MapCache per process)"),
    (PER, 2, "re:XferLoad::xfer(Ascii|Unicode)String\\(.*\\)::buffer", "threads only (a load runs to completion on one thread): local or member buffer; matters once save/load is used"),
    (PER, 2, "s_transportStatuses", "ScriptConditions transport-status cache: ScriptConditions member"),
    (PER, 2, "re:theBuildPlan|thePlanSubjectCount|thePlanSubject", "GameLogicDispatch build-plan state: GameLogic members, reset in clearGameData"),
    (PER, 2, "re:\\(anonymous namespace\\)::s_(observer|userData|scopeActive|origin|script|depth|muted|seq)", "AIDecisionObserver hook state (rlgenerals' decisions recorder): per GameLogic / EngineContext field"),
    (PER, 2, "startTime", "Recorder start time: Recorder member"),
    (PER, 2, "REPLAY_CRC_INTERVAL", "written by -ReplayCRCInterval and by replay playback (Recorder.cpp): Recorder/GameLogic member"),
    (PER, 2, "TerrainLogic::m_gridWaterHandle", "the grid-water handle every TerrainLogic hands out: TerrainLogic member"),
    (PER, 2, "inCRCGen", "threads only: set only while GameLogic::getCRC runs; GameLogic member before threads"),
    (PER, 2, "re:PathNode::computeDirectionVector\\(\\)::dir", "threads only: returned-by-pointer scratch, consumed at once; local/member before threads"),
    (PER, 4, "re:BuildAssistant::buildTiledLocations\\(.*\\)::tileInfo", "threads only: returned-by-pointer scratch, consumed at once"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 3: device-layer state a headless engine uses.
    (PER, 3, "re:W3DDisplay::m_(3DScene|2DScene|3DInterfaceScene|assetManager)", "W3DDisplay class static (94 references): PerEnginePtr<T> proxy"),
    (PER, 3, "WW3DAssetManager::TheInstance", "the asset manager (bones and meshes headless): change only Get_Instance/Delete_This"),
    (PER, 3, "TheDX8MeshRenderer", "mutated on every map load (Free_Assets_With_Exclusion_List) and mesh destruction: per engine (T3: not a T* field)"),
    (PER, 3, "re:_RegisteredMeshList|texture_category_delete_list|fvf_category_container_delete_list|_TempVertexBuffer|_TempNormalBuffer", "TheDX8MeshRenderer's file-static lists: per engine with it"),
    (PER, 3, "WorldHeightMap::m_alphaTiles", "built lazily from the first map's tiles: build once and make immutable, or a member"),
    (PER, 3, "re:s_buffer|s_blendBuffer", "threads only: WorldHeightMap tile scratch, filled and consumed in one call"),
    (PER, 3, "re:filtertable|table_valid", "motchan's lazily built filter table (animation decompression, headless too): std::call_once"),
    (PER, 3, "re:(Sphere|Ring)_Array_Valid|(Sphere|Ring)MeshArray|(Sphere|Ring)LODCosts", "lazily built sphere/ring LOD meshes: std::call_once (or per engine)"),
    (PER, 3, "AssetStatusClass::Instance", "asset-status report object: std::call_once / per engine"),
    (PER, 3, "re:_Fast(Acos|Asin|Sin|InvSin)Table", "WWMath::Init tables: refcount WWMath::Init/Shutdown (same values every time)"),
    (PER, 3, "re:MeshDebugIdCount|unique|unused_texture_id|DecalSystemClass::DecalIDGenerator", "W3D mesh/material/texture/decal ID counter: per engine (IDs continue from the previous engine)"),
    (PER, 3, "rand_gen", "ParticleBufferClass's Random4Class: per engine"),
    (PER, 3, "rand4", "the texture mapper's Random4Class (random UV mappers): per engine"),
    (PER, 3, "re:WW3D::(SyncTime|PreviousSyncTime|FractionalSyncMs|LogicFrameTimeMs|FrameCount)", "WW3D timing statics that drive animation time (bones headless too): per engine"),
    (PER, 3, "re:TheWaterTransparency|TheWeatherSetting", "OVERRIDE<> INI objects (Water.ini/Weather.ini): per engine, freed outside W3DWater (T3: freed by ~GameEngine for now)"),
    (PER, 3, "WaterSettings", "Water.ini time-of-day settings, rewritten by every engine's parse: per engine with TheWaterTransparency"),
    (PER, 3, "TheW3DFrameLengthInMsec", "W3D frame timing value (T3: not a T* field): per engine"),
    (PER, 3, "_TheFileFactory", "WW file factory the W3D loaders read through: one permanent process-wide factory dispatching to the current engine's TheFileSystem"),
    (PER, 3, "_TheSimpleFileFactory", "with _TheFileFactory"),
    (PER, 3, "re:W3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count|WW3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count", "missing-asset warning limiter: harmless shared, per engine with the asset manager"),
    (PER, 3, "_PlaneEQArray", "threads only: MeshGeometryClass plane-equation scratch (ray casts), filled and consumed in one call"),
    (PER, 3, "re:CollisionContext|IntersectContext", "threads only: WWMath AAB-tree collision scratch, filled and consumed in one call"),
    (PER, 3, "CameraShakerSystem", "W3D camera shake system; W3DView only, but the object is created at static init: per engine with the view"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 4: caches and remaining statics.
    (PER, 4, "re:ActiveBody::updateBodyParticleSystems\\(\\)::\\w+Template", PER_ENGINE_STATIC),
    (PER, 4, "re:.*::(upgradeTemplate|supplyLinesTemplate|workerShoeTemplate|nationalismTemplate|fanaticismTemplate|muzzle|debrisTemplate|genericBridgeTemplate)", PER_ENGINE_STATIC),
    (PER, 4, "re:WaveGuideUpdate::.*::(wave[123]|left|right|splash|waveSplash)", PER_ENGINE_STATIC),
    (PER, 4, "re:.*::(structureAttackSound|underAttackSound|infiltrationWarningSound|rallyNotSet|rallyPointSet|placeBuilding|aSound|discoveredSound|neutralizedSound|leftGameSound|noCanDoSound|click)", AUDIO_SCRATCH),
    (PER, 4, "re:Drawable::s_(staticImagesInited|veterancyImage|fullAmmo|emptyAmmo|fullContainer|emptyContainer|animationTemplates)", "written by every Drawable constructor (headless too), read only by drawIconUI: stale Image*/Anim2DTemplate* after the first engine dies; PER_ENGINE_STATIC"),
    (PER, 4, "debrisModelNamesGlobalHack", "INI-parse output (ObjectCreationList) consumed by the client's preload: another engine's boot feeds this engine's preload; ObjectCreationListStore member"),
    (PER, 4, "TerrainRoadCollection::m_idCounter", "road type ID counter continued by every engine's Roads.ini parse: TheTerrainRoads member"),
    (PER, 4, "View::m_idNext", "view ID counter: per engine"),
    (PER, 4, "re:InGameUI::update\\(\\)::(lastMoney|lastIncome)|InGameUI::updateFloatingText\\(\\)::lastLogicFrameUpdate", "InGameUI::update runs headless too (GameClient::update): UI state, no simulation effect; InGameUI members"),
    (PER, 4, "re:ParticleSystem::(computePointOnUnitSphere|computeParticleVelocity|computeParticlePosition|generateParticleInfo)\\(.*\\)::\\w+", "threads only: particle scratch returned by pointer, consumed at once"),
    # ---------------------------------------------------------------------------------------------------
    # Process-global on purpose.
    (GLOBAL, "", "re:rts::t_engine|rts::g_noEngine|rts::\\(anonymous namespace\\)::theNextSlotIndex", "the EngineContext mechanism itself (PLAN-023 Decision 1)"),
    (GLOBAL, "", "re:TheNameKeyGenerator|\\(anonymous namespace\\)::thePriming(Mutex|State|Failure)|NameKeyGenerator::perturbForTesting\\(.*\\)::calls", "shared immortal NameKey generator and its priming latch (PLAN-023 Decision 2)"),
    (GLOBAL, "", "re:.*::(key_\\w+|jetKey)", "cached NameKeyType (a NAMEKEY(...) in an inline function): process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:ControlBar::update(OCLTimer|Construction)TextDisplay\\(.*\\)::(descID|barID)", "cached window NameKey held as UnsignedInt: process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:Object::Object\\(.*\\)::\\w+ModuleData", "shared helper ModuleData: every Object writes the same (Decision 2) NameKey tag into it"),
    (GLOBAL, "", "re:scoringBuilding(Create|Destroy)?Mask", "rewritten with the same constant KindOf bits by every ScoreKeeper"),
    (GLOBAL, "", "re:ModuleInfo::clearCopiedFromDefaultEntries\\(.*\\)::\\w+Mask", "rewritten with the same constant KindOf bits on every call"),
    (GLOBAL, "", "re:gameWin(System|Input|Tooltip)Table|winLayout(Init|Update|Shutdown)Table|gameWinDrawTable|layoutInitTable", "FunctionLexicon table: names and function pointers; every engine's init writes the same NameKeys into it (Decision 2)"),
    (GLOBAL, "", "re:The(AsciiString|UnicodeString|Dma|MemoryPool|DebugLog)CriticalSection", "critical section, process-wide lock (PLAN-023 Phase 1)"),
    (GLOBAL, "", "re:TheMemoryPoolFactory|TheDynamicMemoryAllocator|theMainInitFlag|theMemoryManagerUsers(Mutex)?|\\(anonymous namespace\\)::TheProcessOperators(State)?", "refcounted process-wide memory manager (PLAN-023 Phase 1b, Decision 3)"),
    (GLOBAL, "", "TheVersion", "build version, identical for every engine"),
    (GLOBAL, "", "GlobalData::m_theOriginal", "empty stand-in: the value lives in EngineContext::originalGlobalData (Phase 1b)"),
    (GLOBAL, "", "re:theEngineEmbeddedMode|theReleaseCrashLogFile", "embedding host's fatal-error mode / release-crash log (PLAN-023 Phase 5 moves the log to per-engine output paths)"),
    (GLOBAL, "", "theInReleaseCrashNoReturn", "thread_local re-entry guard of ReleaseCrashNoReturn: per thread by design"),
    (GLOBAL, "", "re:TheSDL3Window|ApplicationHWnd", "the process's window (at most one rendering engine per process)"),
    (GLOBAL, "", "re:__argc|__argv", "the process's argv (the executable's, set by rlgenerals' launcher; PLAN-023 Phase 5 BootConfig replaces it)"),
    (GLOBAL, "", "re:critSec[1-5]|FilterSoftwareVulkanICDs\\(\\)::hw_icds", "the game executable's own (SDL3Main), unused by an embedding host"),
    (GLOBAL, "", "re:rts::WorkingDirectory::\\w+", "the startup working directory (one per process)"),
    (GLOBAL, "", "re:rts::ClientInstance::\\w+", "the process's single-instance mutex"),
    (GLOBAL, "", "re:ReplaySimulation::s_\\w+", "the game executable's multi-replay simulation driver (-simReplay); an embedded engine never runs it"),
    (GLOBAL, "", "OurLanguage", "the installation's language, the same for every engine"),
    (GLOBAL, "", "re:GetRegistryLanguage\\(\\)::(cached|val)", "the installation's language string, written once (Phase 4 string rule: call_once before threads)"),
    (GLOBAL, "", "UnicodeString::format_va(wchar_t const*, __va_list_tag*)::s_utf8_locale", "a UTF-8 locale handle, created once"),
    (GLOBAL, "", "s_assetFallbackPath", "the install's asset fallback root, set once from the environment (PLAN-023 Phase 5 BootConfig)"),
    (GLOBAL, "", "re:s_thread|s_done|s_hasUpdate|s_latestTag", "update checker (menus), one per process"),
    (GLOBAL, "", "re:thread_id_map(_mutex)?|next_thread_id", "pthread-to-Win32 thread id map (CompatLib), process-wide by nature"),
    (GLOBAL, "", "re:GameSpyColor|theLobbyFilter|isThreadHosting|NET_CRC_INTERVAL|MIN_LOGIC_FRAMES|MAX_FRAMES_AHEAD|MIN_RUNAHEAD|FRAME_DATA_LENGTH|FRAMES_TO_KEEP|commandsReadyDebugSpewage", NET),
    (GLOBAL, "", "file:/GameNetwork/", NET),
    (GLOBAL, "", "file:/WWDownload/", "patch/map downloader (menus only), one per process"),
    (GLOBAL, "", "re:(Unicode|Ascii)StringToQuotedPrintable\\(.*\\)::dest|QuotedPrintableTo(Unicode|Ascii)String\\(.*\\)::dest", "only the LAN/online lobby code calls these: " + NET),
    (GLOBAL, "", "re:Return_Buffer|Temp_Buffer", "password encryption for the online login menu: " + NET),
    (GLOBAL, "", "re:TheLobbyQueuedUTMs.*", "GameSpy lobby menu queue (menu state; one UI engine per process)"),
    (GLOBAL, "", "re:CPUDetectClass::\\w+|Windows9xVersionTable", "CPU/OS detection, done once per process at static initialisation"),
    (GLOBAL, "", "re:WideStringClass::m_\\w+|StringClass::(m_Mutex|m_NullChar|m_EmptyString|m_TempStrings|ReservedMask)", "WWLib string temp-buffer pool, guarded by its own mutex: process-wide by design"),
    (GLOBAL, "", "re:generalAllocator|FastAllocatorGeneral::Alloc\\(unsigned int\\)::re_entrancy", "WWLib fast allocator, process-wide like malloc"),
    (GLOBAL, "", "re:AutoPoolClass<.*>::Allocator\\(\\)::allocator", "WWLib object pool per type, process-wide like malloc"),
    (GLOBAL, "", "re:RegistryClass::IsLocked|\\(anonymous namespace\\)::GetRegistryPaths\\(\\)::paths", "registry emulation (the process's settings files)"),
    (GLOBAL, "", "re:IndexClass<int, INI(Entry|Section)\\*>::operator\\[\\]\\(int const&\\) const::x", "default value returned for a missing index, never written"),
    (CONST, "", "BufferedFileClass::_DesiredBufferSize", "buffer-size setting, never changed"),
    (GLOBAL, "", "INIClass::KeepBlankEntries", "WWLib INI parser option, never changed"),
    (GLOBAL, "", "re:_DefaultFileFactory|_DefaultWritingFileFactory|_TheWritingFileFactory", "WWLib default (raw) file factories; the per-engine one is _TheFileFactory"),
    (GLOBAL, "", "re:SaveLoadSystemClass::\\w+|DefinitionFactoryMgrClass::_FactoryListHead|_TheDefinitionMgr|DefinitionMgrClass::\\w+|text_mutex|status_text|status_count|_(alloc|load|reg)_time", "WWSaveLoad registries (persist factories registered at static init; the definition manager is used only by W3DView/tools)"),
    (GLOBAL, "", "LookupTableMgrClass::Tables", "WWMath lookup-table manager (tools/W3DView); not used by the game"),
    (GLOBAL, "", "re:CollisionMath::Stats", "collision-math statistics counters (debug display)"),
    (GLOBAL, "", "OpenALAudioManager::isOnScreen(Coord3D const*) const::dummy", "write-only out-parameter, never read"),
    (GLOBAL, "", "s_screenshotWrittenQueue", "screenshot writer queue (render mode; its thread reads no engine state)"),
    (GLOBAL, "", "OPENSSL_ia32cap_P", "OpenSSL CPU capability word (third-party)"),
    (GLOBAL, "", "re:stbi__flip_vertically_on_write|stbi_write_(force_png_filter|png_compression_level|tga_with_rle)", "stb_image_write configuration, never changed"),
    # ---------------------------------------------------------------------------------------------------
    # Constant.
    (CONST, "", "re:Git(CommitTimeStamp|HaveInfo|Revision|UncommittedChanges)", "build information"),
    (CONST, "", "re:LZHL(De|En)coderStat::\\w+Table0", "LZHL static coding tables"),
    (CONST, "", "re:gli::.*::Table|stbiw__encode_png_line\\(.*\\)::(firstmap|mapping)|pi_lo|tiny", "table/constant in header-only third-party code"),
    (CONST, "", "re:paramsFor(Startup|EngineInit)", "command-line parameter table (names and handlers)"),
    (CONST, "", "re:OBJECT_STATUS_MASK_NONE|DAMAGE_TYPE_FLAGS_(NONE|ALL)|DISABLEDMASK_(NONE|ALL)|KINDOFMASK_(NONE|FS)", "constant bit mask, set at static initialisation"),
    (CONST, "", "re:replayExtention|lastReplayFileName|PORTABLE_(SAVE|MAPS|USER_MAPS)|g_csfFile|g_strFile|statsDir|ignoredChars", "constant string, never reassigned"),
    (CONST, "", "re:Surfaces|ShakeIntensities|EMITTER_TYPE_NAMES|ReportCategoryNames", "name table, never written"),
    (CONST, "", "re:s_noSoundMarker|ThingTemplate::s_audioEventNoSound", "default-constructed 'no sound' marker, returned by const pointer"),
    (CONST, "", "SimpleObjectIterator::theClumpCompareProcs", "function-pointer table"),
    (CONST, "", "re:fast_float_(floor|ceil)\\(float\\)::almost1", "constant, never written"),
    (CONST, "", "re:CRC::_Table|CRC32_Table|Random3Class::Mix[12]|_box_normal|TwiddlerClassName", "constant table"),
    (CONST, "", "re:gameWindowFieldList|layoutScriptTable", ".wnd parse table (names and parsers)"),
    (CONST, "", "re:_\\w+(Loader|Factory)|_NullPrototype|_TwiddlerPersistFactory", LOADER),
    # ---------------------------------------------------------------------------------------------------
    # Debug only.
    (DEBUG, "", "re:TheDebugIgnoreSyncErrors|TheCurrentIgnoreCrashPtr|g_LastErrorDump", "debug"),
    (DEBUG, "", "re:st_(LastCurrentFrame|CurrentFrame|CanAppCont|AppIsFast|DebugDLL|ParticleDLL|particleSystem|particleSystemNeedsStopping)", "ScriptEngine VTune/debug-DLL/particle-editor hooks (PLAN-023 Phase 4 names them)"),
    (DEBUG, "", "re:_writeSingleParticleSystem\\(.*\\)::buff[1-4]|_reloadParticleSystemFromINI\\(.*\\)::linebuff|_getParticleSystemName\\(\\)::buff", "particle-editor writer/reader buffers (debug DLL)"),
    (DEBUG, "", "ScriptEngine::getTeamNamed(AsciiString const&)::warnCount", "debug-message limiter"),
    (DEBUG, "", "re:Object::setTriggerAreaFlagsForChangeInPosition\\(\\)::didWarn|PathfindCell::~PathfindCell\\(\\)::warn", "warn-once flag"),
    (DEBUG, "", "re:s_totalOpen", "open-file counter for a debug assert"),
    (DEBUG, "", "re:DebugDisplay::printf\\(.*\\)::text", "debug display text buffer"),
    (DEBUG, "", "re:CommandTranslator::translateGameMessage\\(.*\\)::old\\w+", "debug-command toggles (MSG_META_DEMO_*)"),
    (DEBUG, "", "file:/WWDebug/", "WW memory log"),
    (DEBUG, "", "re:W3DDisplay::gatherDebugStats\\(\\)::\\w+|W3DDisplay::updateAverageFPS\\(\\)::\\w+", "debug statistics (render only)"),
    (DEBUG, "", "file:WW3D2/(statistics|dx8rendererdebugger)\\.cpp", "WW3D render statistics / renderer debugger"),
    # ---------------------------------------------------------------------------------------------------
    # Render only (and UI only).
    (RENDER, "", "Drawable::s_modelLockCount", "model lock count: reaches the simulation only in render mode (getBarrelCount), PLAN-023 Phase 8"),
    (RENDER, "", "re:WW3D::(IsSortingEnabled|PixelCenter[XY]|RenderBackend|IsInitted|IsRendering|IsCapturing|IsScreenUVBiased|AreDecalsEnabled|DecalRejectionDistance|AreStaticSortListsEnabled|MungeSortOnLoad|OverbrightModifyOnLoad|Movie|PauseRecord|RecordNextFrame|UserStat[0-2]|DefaultNativeScreenSize|DefaultStaticSortLists|CurrentStaticSortLists|DefaultDebugMaterial|DefaultDebugShader|LightmapDebugShader|PrelitMode|ExposePrelit|SnapshotActivated|ThumbnailEnabled|MeshDrawMode|NPatchesGapFillingMode|NPatchesLevel|IsTexturingEnabled|IsColoringEnabled|LastFrameMemoryAllocations|LastFrameMemoryFrees|TextureFilter|AnisotropyLevel|Lite)|_TextureReduction|_TextureMinDim|_LargeTextureExtraReductionEnabled|DAZZLE_INI_FILENAME", "WW3D render setting or render-loop state: only WW3D::Init, the render loop and the options code (render mode) write it; a headless engine only reads the defaults"),
    (RENDER, "", "re:LocationHash|DuplicateLocationHash|SideHash", "MeshModelClass::Init_For_NPatch_Rendering scratch (needs the render device's caps)"),
    (RENDER, "", "re:TheSupplyAndTechImageLocations", "skirmish menu map-preview markers (T3 listed it for Phase 4; only the menu reads it): " + UI),
    (RENDER, "", "file:/GUI/", UI),
    (RENDER, "", "re:m_replayWindow|ScriptActions::m_messageWindow", "GameWindow pointer, null with GameWindowManagerDummy: " + UI),
    (RENDER, "", "re:scrollDir|prevCursor|Mouse::updateMouseData\\(\\)::busy", "mouse/scroll input: headless has MouseDummy and no input"),
    (RENDER, "", "re:W3DRadar::.*", "W3DRadar: headless has RadarDummy"),
    (RENDER, "", "re:W3DDisplay::draw\\(\\)::\\w+|s_filtered(Resolutions|Dirty)", "W3DDisplay::draw returns at once headless; resolution list for the options menu"),
    (RENDER, "", "re:W3DView::update\\(\\)::followFactor", "W3DView: headless has ViewDummy"),
    (RENDER, "", "re:SmudgeSet::m_freeSmudgeList", "heat-haze smudges, drawn only"),
    (RENDER, "", "file:/W3DDevice/GameClient/(Shadow|Water)/", W3D_RENDER),
    (RENDER, "", "file:W3DShaderManager\\.cpp|W3DMouse\\.cpp|W3DScene\\.cpp|W3DShroud\\.cpp|W3DStatusCircle\\.cpp|W3DTreeBuffer\\.cpp|FlatHeightMap\\.cpp|HeightMap\\.cpp|BaseHeightMap\\.cpp|W3DGhostObject\\.cpp|Win32Mouse\\.cpp", W3D_RENDER),
    (RENDER, "", "file:WW3D2/(dx8wrapper|dx8indexbuffer|dx8vertexbuffer|dx8caps|dx8texman|dx8webbrowser|sortingrenderer|shader|render2d|font3d|missingtexture|dazzle|pointgr|segline|linegrp|texture|textureloader|texturefilter|texturethumbnail|texproject|boxrobj|scene|formconv|decalmsh|visrasterizer|metalmap|lightenvironment|vertmaterial|part_buf|meshmatdesc|mesh|meshmdl|meshgeometry|shattersystem|predlod|matpass|dynamesh|rendobj|part_emt|ww3d|dx8renderer)\\.cpp", W3D_RENDER),
    (RENDER, "", "re:Quads|SortingQuads", W3D_RENDER),
    (RENDER, "", "Vector3Randomizer::Randomizer", "RNG of the W3D particle emitters' randomizers (render objects)"),
    (RENDER, "", "listboxLobbyGamesLarge", UI),
    (RENDER, "", "re:AnimatedSoundMgrClass::\\w+", "W3D animated sounds (WWAudio), not used by the game's audio"),
]
