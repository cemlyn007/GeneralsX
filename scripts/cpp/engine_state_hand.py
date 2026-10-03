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
# window callbacks, no layout init), the radar is RadarDummy and the tactical view is ViewDummy. At most one
# engine per process renders (PLAN-023 consumer answer 2), so what only that engine reaches is `render-only`.
# GameWindowManagerDummy never hands out null: winCreateFromScript makes a real (blank) window per call and
# winGetWindowFromId returns some real window, so a window pointer a headless engine stores is a real object
# of that engine's window manager, and a static holding one is per-engine. `render-only` therefore means
# code a headless engine never runs, not state that stays null there: every entry below was checked against
# the functions a headless engine calls (from GameLogic, the script engine, GameClient/InGameUI updates and
# every engine's boot and teardown) and against a runtime probe of a headless engine (every .data/.bss
# symbol read after boot, Hard AI skirmishes on Tournament City and Winding River, a save and load, the
# replay's playback, a game ended by script victory, and an Env game to the local player's defeat): no
# symbol classified render-only, constant or debug-only changed but the const objects built on first use.
#
# There are no per-engine entries left: PLAN-023 Phases 2-4 moved every one into the engine, and Phase 5b
# (engines on separate threads) the last 18, the "threads only" scratch that was correct for several engines
# stepped on ONE thread, into thread_locals, PER_ENGINE_STATICs, locals or (AssetStatusClass::Instance) a
# lock. A symbol classified per-engine is therefore a regression, and `check` fails on it in every mode. A thread_local is process-global here (per
# thread by design), and must stay small: the library's TLS is initial-exec, so it comes out of glibc's
# static TLS surplus for a dlopened library (a few KB at most; Phase 2's 5 KB XferLoad buffers failed to load).
#
# Process-wide state must be written once (at static initialisation or the first boot) and only read after
# that, or be atomic or locked: engines boot while others step on their threads (PLAN-023 Phase 5b). A note
# that says "boot/shutdown only, serialised by the host" names state that only boots and teardowns touch,
# which the host serialises (one process mutex), and that no engine reads while it steps.

PER = "per-engine"
GLOBAL = "process-global"
CONST = "constant"
DEBUG = "debug-only"
RENDER = "render-only"
UNREVIEWED = "unreviewed"

UI = (
    "UI only: menus, window callbacks and gadgets run only with a real window manager, and a headless "
    "engine has GameWindowManagerDummy (no .wnd parse, no callbacks, no layout init; its windows are real "
    "but blank) and no player input; one UI engine per process"
)
NET = (
    "multiplayer network/online state: reached only from the LAN/online menus and networked games; a "
    "headless skirmish opens no network (PLAN-023 'Things checked'), and one networked engine per process"
)
# The GUI files a headless engine reaches too, so the `file:/GUI/` render-only blanket must not cover them:
# InGameUI::init creates the ControlBar and runs ControlBar::init; GameLogic::startNewGame makes and inits a
# load screen; GameLogic::clearGameData (GameLogicDispatch) calls HideDiplomacy, ResetDiplomacy and
# ResetInGameChat; ScriptActions reach InGameUI::popupMessage; VictoryConditions calls
# PopulateInGameDiplomacyPopup; and GameWindowManagerDummy is a GameWindowManager whose winGetWindowFromId hands
# out a dummy window rather than null, so window pointers are real (per-engine) objects headless too. The GUI
# functions in other files that a headless engine calls directly touch per-engine state only (PLAN-023 Phase 4
# fixes): Show/HideControlBar (ControlBarCallback.cpp; no static of its own), HideQuitMenu (QuitMenu.cpp, from
# clearGameData: the state it touches is a PER_ENGINE_STATIC; the rest only the menu writes), Shell::update
# (Shell.cpp, from GameClient::update: its throttle is a PER_ENGINE_STATIC) and ~GameWindowManager's
# freeStaticStrings (GameWindowManagerScript.cpp: the .wnd parse scratch, entry below).
HEADLESS_GUI = (
    "ControlBar/|LoadScreen\\.cpp|GameWindowManager\\.cpp|"
    "GUICallbacks/(Diplomacy|InGameChat|InGamePopupMessage|ControlBarPopupDescription)\\.cpp"
)
W3D_RENDER = "W3D render path (device, shaders, draw lists): only the one rendering engine per process reaches it (PLAN-023 Phase 8)"
LOADER = "stateless W3D prototype loader/persist factory, registered once at static initialisation"

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
    # thread_local (per thread by design, so process-global here): both checkfortransitionsnum, inCRCGen,
    # dx8renderer.cpp's DX8MeshRendererState_destroying and
    # PathNode::computeDirectionVector()'s dir. The two XferLoad buffers are locals now (so gone).
    (GLOBAL, "", "re:State::friend_check(For|ForSleep)Transitions\\(StateReturnType\\)::checkfortransitionsnum", "thread_local call-depth counter (PLAN-023 Phase 2): per thread by design"),
    (GLOBAL, "", "TerrainLogic::m_gridWaterHandle", "an address-only sentinel (the grid water's WaterHandle is compared by address and never written), the same for every engine"),
    (GLOBAL, "", "re:\\(anonymous namespace\\)::DX8MeshRendererState_destroying", "thread_local, set only while this thread runs an engine's DX8MeshRendererState destructor (PLAN-023 Phase 3): per thread by design"),
    (GLOBAL, "", "inCRCGen", "thread_local, set only while this thread runs GameLogic::getCRC (PLAN-023 Phase 2): per thread by design"),
    (GLOBAL, "", "re:PathNode::computeDirectionVector\\(\\)::dir", "thread_local returned-by-pointer scratch, consumed at once (PLAN-023 Phase 2): per thread by design"),
    (GLOBAL, "", "re:BuildAssistant::buildTiledLocations\\(.*\\)::tileInfo", "thread_local returned-by-pointer scratch, consumed at once (PLAN-023 Phase 5b): per thread by design"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 3: device-layer state a headless engine uses.
    # Done (PLAN-023 Phase 3 PR): W3DDisplay's scenes and asset manager, WW3DAssetManager::TheInstance and
    # WW3D's timing statics are EngineContext fields behind rts::ContextField stand-ins; TheDX8MeshRenderer
    # and its three lists, the particle buffers' rand_gen and the mesh/material/texture/decal ID counters are
    # PER_ENGINE_STATICs; WorldHeightMap::m_alphaTiles is a member (so gone from the library, the slot
    # indexes rule:per-engine-static). What is left here is process-wide by design.
    (GLOBAL, "", "_TheFileFactory", "with RTS_ENGINE_CONTEXT, always the one permanent W3D file factory (W3DFileSystem.cpp, immortal) that forwards to the current engine's W3DFileSystem (PLAN-023 Phase 3): written once, by the first W3DFileSystem (a later one finds it set and does not write it, PLAN-023 Phase 5b), and never nulled"),
    (GLOBAL, "", "re:(guard variable for )?\\(anonymous namespace\\)::engineW3DFileFactory\\(\\)::factory", "the one permanent W3D file factory _TheFileFactory points at (PLAN-023 Phase 3): made on first use under the static-init guard and never destroyed, the same object for every engine"),
    (GLOBAL, "", "_TheSimpleFileFactory", "never reassigned by the engine (only the tools do): always the process-global default factory"),
    (GLOBAL, "", "re:table_once|Ring_Array_Once|Sphere_Array_Once", "std::once_flag of a table built once per process from constants (motchan's filter table, the ring/sphere LOD meshes; PLAN-023 Phase 3)"),
    (GLOBAL, "", "re:WWMathInitMutex|WWMathInitCount", "the process-wide WWMath::Init/Shutdown count and its mutex (PLAN-023 Phase 3): the tables are built by the first Init and freed by the last Shutdown of any engine; each engine holds at most one count (EngineContext::wwMathInitialized)"),
    (GLOBAL, "", "filtertable", "motchan's filter table: built once per process under std::call_once from constants (PLAN-023 Phase 3)"),
    (GLOBAL, "", "re:(Sphere|Ring)MeshArray|(Sphere|Ring)LODCosts", "sphere/ring LOD meshes: built once per process under std::call_once from constants (PLAN-023 Phase 3); only the one rendering engine's draw re-sets their alpha/scale, right before it draws them"),
    (GLOBAL, "", "re:_Fast(Acos|Asin|Sin|InvSin)Table", "WWMath::Init tables: built by the first of the counted WWMath::Init calls (PLAN-023 Phase 3), the same values every time"),
    (GLOBAL, "", "TheW3DFrameLengthInMsec", "the W3D frame length: every engine's GameClient::init sets it to MSEC_PER_LOGICFRAME_REAL, its initial value, and W3DGameClient::setFrameRate writes only a change (PLAN-023 Phase 5b), so no engine writes it after static initialization"),
    (GLOBAL, "", "re:W3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count|WW3DAssetManager::Create_Render_Obj\\(.*\\)::warning_count", "missing-asset log limiter: bounds a log message count, and no engine state depends on it; a std::atomic, since any engine's thread may count (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:AssetStatusClass::Instance|AssetStatusReportMutex", "the one missing-asset report (the WWDEBUG destructor writes it out at exit) and its lock: every engine adds to it, under the lock (PLAN-023 Phase 5b), and none reads it"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 4: caches and remaining statics.
    # Done (PLAN-023 Phase 4 PR): every function-local cache of a per-engine pointer (the seven ActiveBody
    # particle templates, the upgrade/thing template caches, muzzle, debrisTemplate, genericBridgeTemplate,
    # the seven WaveGuideUpdate particle templates), the thirteen static AudioEventRTS (PER_ENGINE_STATICs,
    # not locals: each keeps its event's sound rotation index, which a local would reset), Drawable's static
    # images, ControlBar's rank icons, the observer, diplomacy, briefing, chat and build-tooltip GUI state a
    # headless engine writes, debrisModelNamesGlobalHack, TerrainRoadCollection::m_idCounter, View::m_idNext,
    # InGameUI's lastMoney/lastIncome/lastLogicFrameUpdate and ShellGameLoadScreen's firstLoad are
    # PER_ENGINE_STATICs (so gone from the library; their slot indexes are rule:per-engine-static). So are,
    # since the headless-GUI audit (they were render-only on the false premise that GameWindowManagerDummy's
    # windows are null): InGameUI's m_replayWindow and ScriptActions::m_messageWindow (real windows headless:
    # the replay control InGameUI::init makes, the victory/defeat window the map scripts open), the chat type
    # (VictoryConditions sets it on the local player's defeat), ControlBar::m_containData (every ControlBar
    # constructor clears it), the quit-menu state HideQuitMenu touches (clearGameData), Shell::update's
    # throttle (GameClient::update) and W3DStatusCircle's colour (every engine's setTeamColor). The
    # other TUs' statics of the same names (the WOL and download menus' staticTextPlayer/staticTextStatus)
    # fall to the GUI blanket below.
    (RENDER, "", "re:staticTextPlayer|staticTextStatus", "with RTS_ENGINE_CONTEXT only the online menus' statics of these names are left (WOLGameSetupMenu.cpp's staticTextPlayer, DownloadMenu.cpp's staticTextStatus); the source column shows the first definition the search finds, the OFF-build one in ControlBarObserver.cpp or Diplomacy.cpp (those are PER_ENGINE_STATIC fields with the context on): " + UI),
    (GLOBAL, "", "re:ParticleSystem::(computePointOnUnitSphere|computeParticleVelocity|computeParticlePosition)\\(.*\\)::\\w+", "thread_local returned-by-pointer scratch, consumed at once (PLAN-023 Phase 5b): per thread by design"),
    # ---------------------------------------------------------------------------------------------------
    # Process-global on purpose.
    (GLOBAL, "", "re:rts::t_engine|rts::g_noEngine|rts::\\(anonymous namespace\\)::(theNextSlotIndex|theEngineLocaleOnce|theEngineLocale)", "the EngineContext mechanism itself (PLAN-023 Decision 1; the engines' locale (LC_NUMERIC set to C), made once under std::call_once, Phase 5b)"),
    (GLOBAL, "", "re:TheNameKeyGenerator|\\(anonymous namespace\\)::thePriming(Mutex|State|Failure)|NameKeyGenerator::perturbForTesting\\(.*\\)::calls", "shared immortal NameKey generator and its priming latch (PLAN-023 Decision 2)"),
    (GLOBAL, "", "re:.*::(key_\\w+|jetKey)", "cached NameKeyType (a NAMEKEY(...) in an inline function): process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:ControlBar::update(OCLTimer|Construction)TextDisplay\\(.*\\)::(descID|barID)", "cached window NameKey held as UnsignedInt: process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:Object::Object\\(.*\\)::\\w+ModuleData|.*taggedHelperModuleData<\\w+>\\(.*\\)::data", "shared helper ModuleData and Object::Object's references to it: made and tagged with its (Decision 2) NameKey once, under the static-init guard, and only read afterwards (PLAN-023 Phase 5b; every Object used to write the tag again)"),
    (GLOBAL, "", "re:gameWin(System|Input|Tooltip)Table|winLayout(Init|Update|Shutdown)Table|gameWinDrawTable|layoutInitTable", "FunctionLexicon table: names and function pointers; every engine's init interns the same names (Decision 2), and FunctionLexicon::loadTable writes a key only when it changes, so only the first boot writes (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:The(AsciiString|UnicodeString|Dma|MemoryPool|DebugLog)CriticalSection", "critical section, process-wide lock (PLAN-023 Phase 1)"),
    (GLOBAL, "", "re:TheMemoryPoolFactory|TheDynamicMemoryAllocator|theMainInitFlag|theMemoryManagerUsers(Mutex)?|\\(anonymous namespace\\)::TheProcessOperators(State)?", "refcounted process-wide memory manager (PLAN-023 Phase 1b, Decision 3)"),
    (GLOBAL, "", "TheVersion", "build version, identical for every engine"),
    (GLOBAL, "", "GlobalData::m_theOriginal", "empty stand-in: the value lives in EngineContext::originalGlobalData (Phase 1b)"),
    (GLOBAL, "", "re:theEngineEmbeddedMode|theReleaseCrashLogFile", "embedding host's fatal-error mode / release-crash log (PLAN-023 Phase 5 moves the log to per-engine output paths)"),
    (GLOBAL, "", "theInReleaseCrashNoReturn", "thread_local re-entry guard of ReleaseCrashNoReturn: per thread by design"),
    (GLOBAL, "", "re:TheSDL3Window|ApplicationHWnd", "the process's window (at most one rendering engine per process)"),
    (GLOBAL, "", "re:__argc|__argv", "the process's argv (the executable's, set by rlgenerals' launcher; PLAN-023 Phase 5 BootConfig replaces it): boot/shutdown only, serialized by the host (only CommandLine's boot-time parse reads it)"),
    (GLOBAL, "", "re:critSec[1-5]|FilterSoftwareVulkanICDs\\(\\)::hw_icds", "the game executable's own (SDL3Main), unused by an embedding host"),
    (GLOBAL, "", "re:rts::WorkingDirectory::\\w+", "the startup working directory (one per process): boot/shutdown only, serialized by the host (CommandLine's boot-time parse writes and reads it)"),
    (GLOBAL, "", "re:rts::ClientInstance::\\w+", "the process's single-instance mutex"),
    (GLOBAL, "", "re:ReplaySimulation::s_\\w+", "the game executable's multi-replay simulation driver (-simReplay); an embedded engine never runs it"),
    (GLOBAL, "", "OurLanguage", "the installation's language, the same for every engine"),
    (GLOBAL, "", "re:GetRegistryLanguage\\(\\)::(once|val)", "the installation's language string: with RTS_ENGINE_CONTEXT written exactly once, under std::call_once, and never mutated afterwards (PLAN-023 Phase 4's process-global string rule); every engine reads the same install"),
    (GLOBAL, "", "UnicodeString::format_va(wchar_t const*, __va_list_tag*)::s_utf8_locale", "a UTF-8 locale handle, created once"),
    (GLOBAL, "", "s_assetFallbackPath", "the install's asset fallback root: every engine's boot (StdBIGFileSystem::init) sets it, but setAssetRootPath writes only a change and the engines of a process share one install, so only the first boot writes while later engines' file lookups read it (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:s_thread|s_done|s_hasUpdate|s_latestTag", "update checker (menus), one per process"),
    (GLOBAL, "", "re:thread_id_map(_mutex)?|next_thread_id", "pthread-to-Win32 thread id map (CompatLib), process-wide by nature"),
    (CONST, "", "GameSpyColorDefaults", "the online chat colors' defaults (Chat.cpp), which each engine's GameSpyColor (a PER_ENGINE_STATIC, PLAN-023 Phase 5b) starts from; never written"),
    (GLOBAL, "", "NGMP_OnlineServicesManager::getInstance()::instance", "the process's GeneralsOnline session, logged into from the online menus of the one UI engine: with RTS_ENGINE_CONTEXT only a non-headless engine calls getInstance() (GameEngine::init, every update, its menus and ~GameEngine; headless engines skip it), and there is at most one such engine per process, so no other engine touches the instance (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:theLobbyFilter|isThreadHosting|NET_CRC_INTERVAL|MIN_LOGIC_FRAMES|MAX_FRAMES_AHEAD|MIN_RUNAHEAD|FRAME_DATA_LENGTH|FRAMES_TO_KEEP|commandsReadyDebugSpewage", NET),
    # Not the whole of GameNetwork/: GameInfo.cpp (skirmish setup), LANGameInfo.cpp, GameMessageParser.cpp and
    # NetworkUtil.cpp are reached without a network, so their statics are classified by name.
    (GLOBAL, "", "file:/GameNetwork/(GameSpy/|GeneralsOnline/|WOLBrowser/|GameSpy\\w*\\.cpp|LANAPI\\w*\\.cpp|NAT\\.cpp|FirewallHelper\\.cpp|Connection(Manager)?\\.cpp|DisconnectManager\\.cpp|Network\\.cpp|GUIUtil\\.cpp|Transport\\.cpp|udp\\.cpp|FileTransfer\\.cpp|DownloadManager\\.cpp|IPEnumeration\\.cpp|NetPacket\\w*\\.cpp|NetCommand\\w*\\.cpp|NetMessageStream\\.cpp|FrameData\\w*\\.cpp|FrameMetrics\\.cpp|User\\.cpp)", NET),
    (GLOBAL, "", "s_commandID", "network command ID counter (NetworkUtil GenerateNextCommandID): only networked games' NetCommandMsgs take IDs; " + NET),
    (GLOBAL, "", "file:/WWDownload/", "patch/map downloader (menus only), one per process"),
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
    (GLOBAL, "", "re:_DefaultFileFactory|_DefaultWritingFileFactory|_TheWritingFileFactory", "WWLib default (raw) file factories"),
    (GLOBAL, "", "re:SaveLoadSystemClass::\\w+|DefinitionFactoryMgrClass::_FactoryListHead|_TheDefinitionMgr|DefinitionMgrClass::\\w+|text_mutex|status_text|status_count|_(alloc|load|reg)_time", "WWSaveLoad registries (persist factories registered at static init; the definition manager is used only by W3DView/tools)"),
    (GLOBAL, "", "LookupTableMgrClass::Tables", "WWMath's lookup-table list: the first of the counted WWMath::Init calls (PLAN-023 Phase 3) adds its default table and the last Shutdown frees it; no game code adds or reads a table (only W3DView does)"),
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
    (GLOBAL, "", "re:ControlBar::(updateBuildQueueDisabledImages|populateBuildQueue)\\(.*\\)::idsInitialized", "guards the fill of that function's cached NameKey array (process-wide by PLAN-023 Decision 2)"),
    (CONST, "", "commandWindowsInitialized", "never written (nothing but its definition names it)"),
    (CONST, "", "WindowLayoutCurrentVersion", "never written"),
    (CONST, "", "re:(guard variable for )?GameWindowManager::assignDefaultGadgetLook\\(.*\\)::\\w+", "set on the first call to a fixed colour (winMakeColor of constants), the same in every engine"),
    (RENDER, "", "re:ControlBar::populateBuildQueue\\(.*\\)::cancel(Unit|Upgrade)Command", "cached CommandButton*, but populateBuildQueue runs only from evaluateContextUI (ControlBar::update returns at once headless): " + UI),
    (RENDER, "", "re:ControlBar::(showBuildTooltipLayout|populateBuildTooltipLayout)\\(.*\\)::\\w+", "tooltip on mouse hover over a command button (commandButtonTooltip): " + UI),
    (RENDER, "", "re:radioButton(InGame|Buddies)|win(InGame|Buddies|Solo)", "written only by ShowDiplomacy (player input) and the online buddy overlay: " + UI),
    (RENDER, "", "ToggleInGameChat(bool)::justHid", "chat toggle, set only on player input (ToggleInGameChat, from CommandXlat's chat keys): " + UI),
    (RENDER, "", "re:staticTextMessage|buttonOk|shouldPause", "InGamePopupMessageInit only: headless, InGameUI::popupMessage's layout has no init (GameWindowManagerDummy's winCreateFromScript returns no init name): " + UI),
    (RENDER, "", "re:ChallengeLoadScreen::activatePieces\\(.*\\)::textPos\\w+", "Generals' Challenge load screen teletype positions: GameLogic::getLoadScreen makes a ChallengeLoadScreen only for a challenge campaign, which only the shell's Generals' Challenge menu starts (" + UI + "); they are also reset (FRAME_TELETYPE_START) and consumed within one ChallengeLoadScreen::init call"),
    (RENDER, "", "re:the(System|Input|Tooltip|Draw)String", "the .wnd parser's scratch names, filled and consumed within one parse (only a real GameWindowManager parses); only the ~GameWindowManager of a manager that parsed a .wnd file clears them (freeStaticStrings, PLAN-023 Phase 5b), which only drops a finished parse's leftovers, so a headless engine never writes them: " + UI),
    (RENDER, "", "file:/GUI/(?!" + HEADLESS_GUI + ")", UI),
    (RENDER, "", "re:scrollDir|prevCursor|Mouse::updateMouseData\\(\\)::busy", "mouse/scroll input: headless has MouseDummy and no input"),
    (RENDER, "", "re:W3DRadar::.*", "W3DRadar: headless has RadarDummy"),
    (RENDER, "", "re:W3DDisplay::draw\\(\\)::\\w+|s_filtered(Resolutions|Dirty)", "W3DDisplay::draw returns at once headless; resolution list for the options menu"),
    (RENDER, "", "re:W3DView::update\\(\\)::followFactor", "W3DView: headless has ViewDummy"),
    (RENDER, "", "re:SmudgeSet::m_freeSmudgeList", "heat-haze smudges, drawn only"),
    (GLOBAL, "", "DX8Wrapper_IsWindowed", "the process's assert switch (Debug.cpp's ignoringAsserts): every headless engine's command line (parseHeadless) sets it false, and a rendering engine's DX8Wrapper::Init (false) and Set_Render_Device (the device's windowed flag) write it too, so it holds the last writer's value, not a fixed one; a std::atomic, since boots write it while other engines' assert paths read it (PLAN-023 Phase 5b)"),
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
