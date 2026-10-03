# GeneralsX @feature cemlyn007 28/09/2026 The hand classifications for engine_state_symbols.py (PLAN-023 Phase 1)
#
# Written by reading the code. Each entry is (class, phase, matcher, note); first match wins, before
# engine_state_symbols.py's rules. A per-engine entry carries the PLAN-023 phase that fixes it (2, 3 or 4)
# and a one-line fix note; every other entry says why it is what it is. The matcher is the exact demangled
# key (compiler `.N` suffixes stripped), `re:` + a regular expression that must match the whole key, or
# `file:` + a regular expression searched for in every definition site of the source column (path:line;
# all sites must match). A `file:` pattern is a blanket: keep it to files that only a rendering, UI or
# networked engine reaches. No entry here classifies a symbol the checked-in TSV lacks without review:
# `check` (non-strict included) fails on every new symbol a hand entry matches, exact, `re:` or `file:`,
# until `snapshot` records it (engine_state_symbols.py's SAFE_FOR_NEW).
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
# threaded follow-up rather than for Phases 2-4 of this delivery. A process-global note can start "threads
# only" too: every engine writes the SAME value into a shared process-global (so it is correct, and stays
# process-global, on one thread), but the write itself is unsynchronised, so it is a data race once engines
# run on separate threads (Object's helper ModuleData, the ScoreKeeper/ModuleInfo KindOf masks, the
# FunctionLexicon tables, generalAllocator's lazy, lock-free creation); the threaded follow-up gives each of
# these a `std::call_once` or an equivalent one-time/locked write.

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
# The GUI files a headless engine reaches too, so the `file:/GUI/` render-only blanket must not cover them:
# InGameUI::init creates the ControlBar and runs ControlBar::init; GameLogic::startNewGame makes and inits a
# load screen; GameLogic::clearGameData (GameLogicDispatch) calls HideDiplomacy, ResetDiplomacy and
# ResetInGameChat; ScriptActions reach InGameUI::popupMessage; VictoryConditions calls
# PopulateInGameDiplomacyPopup; GameClient::update calls TheShell->UPDATE() every frame (Shell::update, which
# also runs the scheme manager's update), headless included; and GameWindowManagerDummy is a GameWindowManager
# whose winGetWindowFromId hands out a dummy window rather than null, so window pointers are real (per-engine)
# objects headless too.
HEADLESS_GUI = (
    "ControlBar/|LoadScreen\\.cpp|GameWindowManager\\.cpp|Shell/(Shell|ShellMenuScheme)\\.cpp|"
    "GUICallbacks/(Diplomacy|InGameChat|InGamePopupMessage|ControlBarPopupDescription)\\.cpp"
)
W3D_RENDER = "W3D render path (device, shaders, draw lists): only the one rendering engine per process reaches it (PLAN-023 Phase 8)"
LOADER = "stateless W3D prototype loader/persist factory, registered once at static initialization"
PER_ENGINE_STATIC = "cached per-engine pointer (template/image/button looked up once): PER_ENGINE_STATIC slot"
AUDIO_SCRATCH = "static AudioEventRTS: make it a local (PLAN-023 Phase 4; also removes a lock-taking exit destructor)"

HAND = [
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 2: simulation statics.
    # Done (PLAN-023 Phase 2 PR): the pathfinder pool, the polygon triggers, the map objects and world dict,
    # the four RNG seeds, TheContactList and getClosestObjects' theIterFlag, the script and sides parse
    # scratch, ScriptList::m_curId, s_failedMapLookups, MapUtil's map-parse scratch, s_transportStatuses,
    # the build plan, the eight AIDecisionObserver statics, the recorder's startTime and REPLAY_CRC_INTERVAL
    # are EngineContext fields or PER_ENGINE_STATICs now (so gone from the library; their slot indexes are
    # rule:per-engine-static), as are Phase 3's TheWaterTransparency, TheWeatherSetting, WaterSettings[], rand4
    # and CameraShakerSystem.
    # thread_local (per thread by design, so process-global here): both checkfortransitionsnum, inCRCGen and
    # PathNode::computeDirectionVector()'s dir. The two XferLoad buffers are locals now (so gone).
    (GLOBAL, "", "re:State::friend_check(For|ForSleep)Transitions\\(StateReturnType\\)::checkfortransitionsnum", "thread_local call-depth counter (PLAN-023 Phase 2): per thread by design"),
    (GLOBAL, "", "TerrainLogic::m_gridWaterHandle", "an address-only sentinel (the grid water's WaterHandle is compared by address and never written), the same for every engine"),
    (GLOBAL, "", "inCRCGen", "thread_local, set only while this thread runs GameLogic::getCRC (PLAN-023 Phase 2): per thread by design"),
    (GLOBAL, "", "re:PathNode::computeDirectionVector\\(\\)::dir", "thread_local returned-by-pointer scratch, consumed at once (PLAN-023 Phase 2): per thread by design"),
    (PER, 4, "re:BuildAssistant::buildTiledLocations\\(.*\\)::tileInfo", "threads only: returned-by-pointer scratch, consumed at once"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 3: device-layer state a headless engine uses.
    (PER, 3, "re:W3DDisplay::m_(3DScene|2DScene|3DInterfaceScene|assetManager)", "W3DDisplay class static (94 references): PerEnginePtr<T> proxy"),
    (PER, 3, "WW3DAssetManager::TheInstance", "the asset manager (bones and meshes headless): change only Get_Instance/Delete_This"),
    (PER, 3, "TheDX8MeshRenderer", "mutated on every map load (Free_Assets_With_Exclusion_List) and mesh destruction: per engine (T3: not a T* field)"),
    (PER, 3, "re:_RegisteredMeshList|texture_category_delete_list|fvf_category_container_delete_list", "TheDX8MeshRenderer's file-static lists: per engine with it"),
    (PER, 3, "re:_TempVertexBuffer|_TempNormalBuffer", "threads only: one per TU (dx8renderer.cpp's skinned-mesh deform scratch, cleared by TheDX8MeshRenderer's teardown; decalmsh.cpp's decal scratch; meshmdl.cpp's unused vertex/normal pair; and mesh.cpp's lone _TempVertexBuffer, no _TempNormalBuffer there, its decal/special-render skin scratch), each resized, filled and consumed within one call"),
    (PER, 3, "WorldHeightMap::m_alphaTiles", "built lazily from the first map's tiles: build once and make immutable, or a member"),
    (PER, 3, "re:s_buffer|s_blendBuffer", "threads only: WorldHeightMap tile scratch, filled and consumed in one call"),
    (PER, 3, "re:filtertable|table_valid", "motchan's lazily built filter table (animation decompression, headless too): std::call_once"),
    (PER, 3, "re:(Sphere|Ring)_Array_Valid|(Sphere|Ring)MeshArray|(Sphere|Ring)LODCosts", "lazily built sphere/ring LOD meshes: std::call_once (or per engine)"),
    (PER, 3, "AssetStatusClass::Instance", "asset-status report object: std::call_once / per engine"),
    (PER, 3, "re:_Fast(Acos|Asin|Sin|InvSin)Table", "WWMath::Init tables: refcount WWMath::Init/Shutdown (same values every time)"),
    (PER, 3, "re:MeshDebugIdCount|unique|unused_texture_id|DecalSystemClass::DecalIDGenerator", "W3D mesh/material/texture/decal ID counter: per engine (IDs continue from the previous engine)"),
    (PER, 3, "rand_gen", "ParticleBufferClass's Random4Class: per engine"),
    (PER, 3, "re:WW3D::(SyncTime|PreviousSyncTime|FractionalSyncMs|LogicFrameTimeMs|FrameCount)", "WW3D timing statics that drive animation time (bones headless too): per engine"),
    (PER, 3, "TheW3DFrameLengthInMsec", "W3D frame timing value (T3: not a T* field): per engine"),
    (PER, 3, "_TheFileFactory", "WW file factory the W3D loaders read through: one permanent process-wide factory dispatching to the current engine's TheFileSystem"),
    (PER, 3, "_TheSimpleFileFactory", "with _TheFileFactory"),
    (PER, 3, "re:W3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count|WW3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count", "missing-asset warning limiter: harmless shared, per engine with the asset manager"),
    (PER, 3, "_PlaneEQArray", "threads only: MeshGeometryClass plane-equation scratch (ray casts), filled and consumed in one call"),
    (PER, 3, "re:CollisionContext|IntersectContext", "threads only: WWMath AAB-tree collision scratch, filled and consumed in one call"),
    (PER, 3, "InheritedWorldSpaceEmitterVel", "threads only: set by ParticleEmitterClass::Emit and read by Initialize_Particle within the same call"),
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
    (PER, 4, "Shell::update()::lastUpdate", "GameClient::update calls TheShell->UPDATE() headless too: a wall-clock throttle shared by every engine's shell screens, so concurrent engines skip each other's scheme-manager updates; Shell member"),
    # GUI statics that a headless engine writes too (see HEADLESS_GUI): the control bar, diplomacy and chat
    # code runs headless against GameWindowManagerDummy's dummy windows, so these point at one engine's
    # objects or are cleared/deleted by another engine's teardown.
    (PER, 4, "re:ControlBar::m_rank(Veteran|Elite|Heroic)Icon", "written by ControlBar::init (InGameUI::init, headless too) from this engine's TheMappedImageCollection: stale Image* after that engine dies; ControlBar members (" + PER_ENGINE_STATIC + ")"),
    (PER, 4, "re:ObserverPlayer(Info|List)Window|buttonPlayer|winFlag|winGeneralPortrait|buttonIdleWorker|staticTextNumberOf(Units|Buildings|UnitsKilled|UnitsLost)|staticTextPlayerName", "ControlBar::initObserverControls (from ControlBar::init, headless too) stores this engine's (dummy) windows: ControlBar members"),
    (PER, 4, "re:staticTextPlayer|staticTextSide|staticTextTeam|staticTextStatus|buttonMute|buttonUnMute|slotNumInRow", "Diplomacy/observer window pointers: HideDiplomacy (GameLogic::clearGameData, headless too) clears them and PopulateInGameDiplomacyPopup (VictoryConditions) writes through them, so one engine touches another's windows; a per-engine Diplomacy state (the ControlBarObserver/menu TUs of the same name follow)"),
    (PER, 4, "re:theWindow|theLayout", "Diplomacy's layout and window (the only instance linked in; ControlBarPopupDescription's are unused): ResetDiplomacy (clearGameData, headless too) destroys the layout and clears the window, another engine's included; per-engine Diplomacy state"),
    (PER, 4, "theAnimateWindowManager", "ResetDiplomacy (clearGameData) and ControlBar::deleteBuildTooltipLayout (ScriptActions' disable-input, headless too) delete it: one engine deletes another's; per-engine Diplomacy state / ControlBar member"),
    (PER, 4, "prevWindow", "tooltip button pointer, cleared by ControlBar::deleteBuildTooltipLayout (ScriptActions, headless too): ControlBar member"),
    (PER, 4, "theBriefingList", "script briefing texts (UpdateDiplomacyBriefingText from InGameUI's popup/military captions, headless too; saved with the GameClient): InGameUI member"),
    (PER, 4, "re:chatWindow|chatTextEntry|chatTypeStaticText|s_savedChat", "ResetInGameChat (GameLogic::clearGameData, headless too) destroys the chat window and clears these, another engine's included: per-engine chat state"),
    (PER, 4, "re:ParticleSystem::(computePointOnUnitSphere|computeParticleVelocity|computeParticlePosition|generateParticleInfo)\\(.*\\)::\\w+", "threads only: particle scratch returned by pointer, consumed at once"),
    # ---------------------------------------------------------------------------------------------------
    # Process-global on purpose.
    (GLOBAL, "", "re:rts::t_engine|rts::g_noEngine|rts::\\(anonymous namespace\\)::theNextSlotIndex", "the EngineContext mechanism itself (PLAN-023 Decision 1)"),
    (GLOBAL, "", "re:TheNameKeyGenerator|\\(anonymous namespace\\)::thePriming(Mutex|State|Failure)|NameKeyGenerator::perturbForTesting\\(.*\\)::calls", "shared immortal NameKey generator and its priming latch (PLAN-023 Decision 2)"),
    (GLOBAL, "", "re:.*::(key_\\w+|jetKey)", "cached NameKeyType (a NAMEKEY(...) in an inline function): process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:ControlBar::update(OCLTimer|Construction)TextDisplay\\(.*\\)::(descID|barID)", "cached window NameKey held as UnsignedInt: process-wide by PLAN-023 Decision 2"),
    # rule:namekey now requires the declaration itself to initialise from NAMEKEY(...) or
    # TheNameKeyGenerator->nameToKey(...): sound for most caches, but these are declared `= NAMEKEY_INVALID`
    # (or with no initialiser, for an array) and filled from the generator later, in a separate init function
    # (ControlBar::initObserverControls, ShowDiplomacy, InGamePopupMessageInit, LobbyUtils, ...), so the rule
    # no longer sees them. Still process-wide NameKey caches by PLAN-023 Decision 2.
    (GLOBAL, "", "re:buttonOkID|buttonCancelID|buttonMuteID|buttonUnMuteID|buttonPlayerID|parentID|staticTextPlayerID|staticTextSideID|staticTextTeamID|staticTextStatusID|radioButtonInGameID|radioButtonBuddiesID|winInGameID|winBuddiesID|winSoloID|s_replayObserverNameKey", "threads only: cached NameKeyType (a window/button ID key), declared `= NAMEKEY_INVALID` and filled from TheNameKeyGenerator by a later init call (ControlBar::initObserverControls, ShowDiplomacy, ...) that every engine runs, unsynchronized: process-wide by PLAN-023 Decision 2; std::call_once or an engine-independent static initializer fixes it"),
    (GLOBAL, "", "re:ControlBar::(updateBuildQueueDisabledImages|populateBuildQueue)\\(.*\\)::buildQueueIDs", "threads only: cached NameKeyType array, declared empty and filled from TheNameKeyGenerator in the same function under an unsynchronized check-then-set idsInitialized latch: process-wide by PLAN-023 Decision 2, with the ID statics above; std::call_once fixes it"),
    (GLOBAL, "", "re:Object::Object\\(.*\\)::\\w+ModuleData", "threads only: shared helper ModuleData; every Object writes the same (Decision 2) NameKey tag into it, but the write is unsynchronized"),
    (GLOBAL, "", "re:scoringBuilding(Create|Destroy)?Mask", "threads only: rewritten (a KindOfMaskType::set(), a non-atomic read-modify-write) with the same constant KindOf bits by every ScoreKeeper"),
    (GLOBAL, "", "re:ModuleInfo::clearCopiedFromDefaultEntries\\(.*\\)::\\w+Mask", "threads only: rewritten with the same constant KindOf bits on every call, unsynchronized"),
    (GLOBAL, "", "re:gameWin(System|Input|Tooltip)Table|winLayout(Init|Update|Shutdown)Table|gameWinDrawTable|layoutInitTable", "threads only: FunctionLexicon table (names and function pointers); every engine's init writes the same NameKeys into it (Decision 2), unsynchronized"),
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
    # Not the whole of GameNetwork/: GameInfo.cpp (skirmish setup), LANGameInfo.cpp, GameMessageParser.cpp and
    # NetworkUtil.cpp are reached without a network, so their statics are classified by name.
    (GLOBAL, "", "file:/GameNetwork/(GameSpy/|GeneralsOnline/|WOLBrowser/|GameSpy\\w*\\.cpp|LANAPI\\w*\\.cpp|NAT\\.cpp|FirewallHelper\\.cpp|Connection(Manager)?\\.cpp|DisconnectManager\\.cpp|Network\\.cpp|GUIUtil\\.cpp|Transport\\.cpp|udp\\.cpp|FileTransfer\\.cpp|DownloadManager\\.cpp|IPEnumeration\\.cpp|NetPacket\\w*\\.cpp|NetCommand\\w*\\.cpp|NetMessageStream\\.cpp|FrameData\\w*\\.cpp|FrameMetrics\\.cpp|User\\.cpp)", NET),
    (GLOBAL, "", "s_commandID", "network command ID counter (NetworkUtil GenerateNextCommandID): only networked games' NetCommandMsgs take IDs; " + NET),
    (GLOBAL, "", "file:/WWDownload/", "patch/map downloader (menus only), one per process"),
    (GLOBAL, "", "re:(Unicode|Ascii)StringToQuotedPrintable\\(.*\\)::dest|QuotedPrintableTo(Unicode|Ascii)String\\(.*\\)::dest", "only the LAN/online lobby code calls these: " + NET),
    (GLOBAL, "", "re:Return_Buffer|Temp_Buffer", "password encryption for the online login menu: " + NET),
    (GLOBAL, "", "re:TheLobbyQueuedUTMs.*", "GameSpy lobby menu queue (menu state; one UI engine per process)"),
    (GLOBAL, "", "re:CPUDetectClass::\\w+|Windows9xVersionTable", "CPU/OS detection, done once per process at static initialization"),
    (GLOBAL, "", "re:WideStringClass::m_\\w+|StringClass::(m_Mutex|m_NullChar|m_EmptyString|m_TempStrings|ReservedMask)", "WWLib string temp-buffer pool, guarded by its own mutex: process-wide by design"),
    (GLOBAL, "", "re:generalAllocator|FastAllocatorGeneral::Alloc\\(unsigned int\\)::re_entrancy", "threads only: WWLib fast allocator, process-wide like malloc, but generalAllocator's lazy creation and re_entrancy's increment/decrement are both unsynchronized, lock-free reads/writes"),
    (GLOBAL, "", "re:AutoPoolClass<.*>::Allocator\\(\\)::allocator", "WWLib object pool per type, process-wide like malloc"),
    (GLOBAL, "", "re:RegistryClass::IsLocked|\\(anonymous namespace\\)::GetRegistryPaths\\(\\)::paths", "registry emulation (the process's settings files)"),
    (GLOBAL, "", "re:IndexClass<int, INI(Entry|Section)\\*>::operator\\[\\]\\(int const&\\) const::x", "default value returned for a missing index, never written"),
    (CONST, "", "BufferedFileClass::_DesiredBufferSize", "buffer-size setting, never changed"),
    (CONST, "", "re:rts::WorkingDirectory::saveStartupWorkingDirectory\\(\\)::len", "captures the process's own startup cwd (::GetCurrentDirectory): the same OS-level value for every engine in the process, written once"),
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
    (CONST, "", "re:OBJECT_STATUS_MASK_NONE|DAMAGE_TYPE_FLAGS_(NONE|ALL)|DISABLEDMASK_(NONE|ALL)|KINDOFMASK_(NONE|FS)", "constant bit mask, set at static initialization"),
    (CONST, "", "re:replayExtention|lastReplayFileName|PORTABLE_(SAVE|MAPS|USER_MAPS)|g_csfFile|g_strFile|statsDir|ignoredChars", "constant string, never reassigned"),
    (CONST, "", "re:Surfaces|ShakeIntensities|EMITTER_TYPE_NAMES|ReportCategoryNames", "name table, never written"),
    (CONST, "", "re:s_noSoundMarker|ThingTemplate::s_audioEventNoSound", "default-constructed 'no sound' marker, returned by const pointer"),
    (CONST, "", "SimpleObjectIterator::theClumpCompareProcs", "function-pointer table"),
    (CONST, "", "re:fast_float_(floor|ceil)\\(float\\)::almost1", "constant, never written"),
    (CONST, "", "re:CRC::_Table|CRC32_Table|Random3Class::Mix[12]|_box_normal|TwiddlerClassName", "constant table"),
    (CONST, "", "re:gameWindowFieldList|layoutScriptTable", ".wnd parse table (names and parsers)"),
    (CONST, "", "re:_\\w+(Loader|Factory)|_NullPrototype|_TwiddlerPersistFactory", LOADER),
    (
        CONST,
        "",
        "SpecialPowerTemplate::m_specialPowerFieldParse",
        "INI FieldParse table, built once, never written; rule:fieldparse's declared-type check expects an "
        "unqualified array name and this is an out-of-line, class-qualified definition",
    ),
    (
        CONST,
        "",
        "s_allWeaponFireFlags",
        "built once from MAKE_MODELCONDITION_MASK5 (Common/ModelState.h), which macro-expands to "
        "ModelConditionFlags's own constructor applied to literal flags only; a textual rule cannot see "
        "through the macro to confirm that",
    ),
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
    (GLOBAL, "", "re:ControlBar::(updateBuildQueueDisabledImages|populateBuildQueue)\\(.*\\)::idsInitialized", "threads only: unsynchronized check-then-set latch guarding the fill of that function's cached NameKey array (process-wide by PLAN-023 Decision 2); std::call_once replaces it"),
    (CONST, "", "commandWindowsInitialized", "never written (nothing but its definition names it)"),
    (CONST, "", "WindowLayoutCurrentVersion", "never written"),
    (CONST, "", "re:(guard variable for )?GameWindowManager::assignDefaultGadgetLook\\(.*\\)::\\w+", "set on the first call to a fixed color (winMakeColor of constants), the same in every engine"),
    (RENDER, "", "ControlBar::m_containData", "written only by the context UI (evaluateContextUI, from ControlBar::update, which returns at once headless) and button clicks: " + UI),
    (RENDER, "", "re:ControlBar::populateBuildQueue\\(.*\\)::cancel(Unit|Upgrade)Command", "cached CommandButton*, but populateBuildQueue runs only from evaluateContextUI (ControlBar::update returns at once headless): " + UI),
    (RENDER, "", "re:ControlBar::(showBuildTooltipLayout|populateBuildTooltipLayout)\\(.*\\)::\\w+", "tooltip on mouse hover over a command button (commandButtonTooltip): " + UI),
    (RENDER, "", "re:radioButton(InGame|Buddies)|win(InGame|Buddies|Solo)", "written only by ShowDiplomacy (player input) and the online buddy overlay: " + UI),
    (RENDER, "", "re:inGameChatType|ToggleInGameChat\\(bool\\)::justHid", "chat type and toggle, set only on player input (Show/ToggleInGameChat): " + UI),
    (RENDER, "", "re:staticTextMessage|buttonOk|shouldPause", "InGamePopupMessageInit only: headless, InGameUI::popupMessage's layout has no init (GameWindowManagerDummy's winCreateFromScript returns no init name): " + UI),
    (UNREVIEWED, "", "re:ShellGameLoadScreen::init\\(.*\\)::firstLoad|ChallengeLoadScreen::activatePieces\\(.*\\)::textPos\\w+", "load-screen state: GameLogic::startNewGame makes and inits a load screen headless too (a shell game's or a challenge campaign's); not yet reviewed whether a headless engine's use of these can change the rendering engine's"),
    (RENDER, "", "file:/GUI/(?!" + HEADLESS_GUI + ")", UI),
    (RENDER, "", "re:m_replayWindow|ScriptActions::m_messageWindow", "GameWindow pointer, null with GameWindowManagerDummy: " + UI),
    (RENDER, "", "re:scrollDir|prevCursor|Mouse::updateMouseData\\(\\)::busy", "mouse/scroll input: headless has MouseDummy and no input"),
    (RENDER, "", "re:W3DRadar::.*", "W3DRadar: headless has RadarDummy"),
    (RENDER, "", "re:W3DDisplay::draw\\(\\)::\\w+|s_filtered(Resolutions|Dirty)", "W3DDisplay::draw returns at once headless; resolution list for the options menu"),
    (RENDER, "", "re:W3DView::update\\(\\)::followFactor", "W3DView: headless has ViewDummy"),
    (RENDER, "", "re:SmudgeSet::m_freeSmudgeList", "heat-haze smudges, drawn only"),
    (RENDER, "", "file:/W3DDevice/GameClient/(Shadow|Water)/", W3D_RENDER),
    (RENDER, "", "file:W3DShaderManager\\.cpp|W3DMouse\\.cpp|W3DScene\\.cpp|W3DShroud\\.cpp|W3DStatusCircle\\.cpp|W3DTreeBuffer\\.cpp|FlatHeightMap\\.cpp|HeightMap\\.cpp|BaseHeightMap\\.cpp|W3DGhostObject\\.cpp|Win32Mouse\\.cpp", W3D_RENDER),
    # Only device-only WW3D2 files: mesh, meshmdl, meshgeometry, rendobj, texture, ww3d, dx8renderer,
    # part_emt and part_buf are reached by a headless engine too (models, bones, emitters, asset loading),
    # so their statics are classified by name below or above.
    (RENDER, "", "file:WW3D2/(dx8wrapper|dx8indexbuffer|dx8vertexbuffer|dx8caps|dx8texman|dx8webbrowser|sortingrenderer|shader|render2d|font3d|missingtexture|dazzle|pointgr|segline|linegrp|textureloader|texturefilter|texturethumbnail|texproject|boxrobj|scene|formconv|decalmsh|visrasterizer|metalmap|lightenvironment|vertmaterial|meshmatdesc|shattersystem|predlod|matpass|dynamesh)\\.cpp", W3D_RENDER),
    (RENDER, "", "_Hwnd", "the render window handle (dx8wrapper.cpp's and ww3d.cpp's, one per TU)"),
    (RENDER, "", "_LineRenderer", "segline.cpp's and streak.cpp's line renderers (draw only)"),
    (RENDER, "", "detailAlphaShader", "tree/bib/bridge/road buffer shaders, one per TU: " + W3D_RENDER),
    (RENDER, "", "parent", "a menu's parent window, one per menu TU (in-game popup, lobby, score screen, map select, ...): " + UI),
    (GLOBAL, "", "pingImages", "the online lobby's and WOL game setup's ping images: " + NET),
    (RENDER, "", "WW3D::Make_Screen_Shot(char const*, float, WW3D::ScreenShotFormatEnum)::frame_number", "screenshot file counter (render mode)"),
    (RENDER, "", "temp_apt", "MeshClass::Create_Decal scratch: no game code creates W3D decals (Create_Decal's callers are all inside WW3D2's decal system)"),
    (RENDER, "", "re:_TempTransformedVertexBuffer|_TempClipFlagBuffer", "MeshModelClass::Shadow_Render scratch (the clip-flag buffer is unused)"),
    (RENDER, "", "re:ParticleBufferClass::Render_Line\\(RenderInfoClass&\\)::tmp_(points|diffuse|id)", "ParticleBufferClass::Render_Line scratch: draw only"),
    (DEBUG, "", "statistics_requested", "DX8 mesh-renderer statistics request (debug display)"),
    (DEBUG, "", "ParticleBufferClass::TotalActiveCount", "particle-buffer statistics counter; its getter Get_Total_Active_Count has no caller"),
    (DEBUG, "", "RenderObjPersistFactoryClass::Load(ChunkLoadClass&) const::count", "warning limiters (two in the function)"),
    (CONST, "", "DX8TextureCategoryClass::m_gForceMultiply", "never written: SetForceMultiply has no caller"),
    (CONST, "", "MeshClass::Legacy_Meshes_Fogged", "never written"),
    (CONST, "", "ParticleBufferClass::LODMaxScreenSizes", "never written: Set_LOD_Max_Screen_Size has no caller"),
    (CONST, "", "re:ParticleEmitterClass::(DebugDisable|DefaultRemoveOnComplete)", "never written: Disable_All_Emitters / Set_Default_Remove_On_Complete have no caller"),
    (RENDER, "", "re:Quads|SortingQuads", W3D_RENDER),
    (RENDER, "", "Vector3Randomizer::Randomizer", "RNG of the W3D particle emitters' randomizers (render objects)"),
    (RENDER, "", "listboxLobbyGamesLarge", UI),
    (RENDER, "", "re:AnimatedSoundMgrClass::\\w+", "W3D animated sounds (WWAudio), not used by the game's audio"),
]
