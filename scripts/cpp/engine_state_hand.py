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
# stepped on ONE thread, into thread_locals, PER_ENGINE_STATICs or locals. A symbol classified per-engine is
# therefore a regression, and `check` fails on it in every mode. A thread_local is process-global here (per
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
RPER = "render-per-engine"
RSCR = "render-scratch"
RCONST = "render-const"
RPROC = "render-process"
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
    (GLOBAL, "", "_TheFileFactory", "with RTS_ENGINE_CONTEXT, always the one permanent W3D file factory (W3DFileSystem.cpp, immortal) that forwards to the current engine's W3DFileSystem (PLAN-023 Phase 3): written once, by the first W3DFileSystem (a later one finds it set and does not write it, PLAN-023 Phase 5b; it replaces only the library's default, and refuses any other factory with a DEBUG_CRASH), and never nulled"),
    (GLOBAL, "", "re:(guard variable for )?\\(anonymous namespace\\)::engineW3DFileFactory\\(\\)::factory", "the one permanent W3D file factory _TheFileFactory points at (PLAN-023 Phase 3): made on first use under the static-init guard and never destroyed, the same object for every engine"),
    (GLOBAL, "", "_TheSimpleFileFactory", "never reassigned by the engine (only the tools do): always the process-global default factory"),
    (GLOBAL, "", "re:table_once|Ring_Array_Once|Sphere_Array_Once", "std::once_flag of a table built once per process from constants (motchan's filter table, the ring/sphere LOD meshes; PLAN-023 Phase 3)"),
    (GLOBAL, "", "re:WWMathInitMutex|WWMathInitCount", "the process-wide WWMath::Init/Shutdown count and its mutex (PLAN-023 Phase 3): the tables are built by the first Init and freed by the last Shutdown of any engine; each engine holds at most one count (EngineContext::wwMathInitialized)"),
    (GLOBAL, "", "filtertable", "motchan's filter table: built once per process under std::call_once from constants (PLAN-023 Phase 3)"),
    (GLOBAL, "", "re:(Sphere|Ring)MeshArray|(Sphere|Ring)LODCosts", "sphere/ring LOD meshes: built once per process under std::call_once from constants (PLAN-023 Phase 3); only the one rendering engine's draw re-sets their alpha/scale, right before it draws them"),
    (GLOBAL, "", "re:_Fast(Acos|Asin|Sin|InvSin)Table", "WWMath::Init tables: built by the first of the counted WWMath::Init calls (PLAN-023 Phase 3), the same values every time"),
    (GLOBAL, "", "TheW3DFrameLengthInMsec", "the W3D frame length: every engine's GameClient::init sets it to MSEC_PER_LOGICFRAME_REAL, its initial value, and with RTS_ENGINE_CONTEXT W3DGameClient::setFrameRate refuses a different value (DEBUG_CRASH) rather than write it (PLAN-023 Phase 5b), so no engine writes it after static initialisation"),
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
    # fall to the GUI blanket below. What follows is the one rule for Phase 5b's thread_local scratch.
    (RENDER, "", "re:staticTextPlayer|staticTextStatus", "with RTS_ENGINE_CONTEXT only the online menus' statics of these names are left (WOLGameSetupMenu.cpp's staticTextPlayer, DownloadMenu.cpp's staticTextStatus); the source column shows the first definition the search finds, the OFF-build one in ControlBarObserver.cpp or Diplomacy.cpp (those are PER_ENGINE_STATIC fields with the context on): " + UI),
    (GLOBAL, "", "re:ParticleSystem::(computePointOnUnitSphere|computeParticleVelocity|computeParticlePosition)\\(.*\\)::\\w+", "thread_local returned-by-pointer scratch, consumed at once (PLAN-023 Phase 5b): per thread by design"),
    # ---------------------------------------------------------------------------------------------------
    # Process-global on purpose.
    (GLOBAL, "", "re:rts::t_engine|rts::g_noEngine|rts::\\(anonymous namespace\\)::(theNextSlotIndex|theEngineFloatingPointMode)", "the EngineContext mechanism itself (PLAN-023 Decision 1; the engines' floating-point mode, learnt from setFPMode() and atomic, Phase 5b)"),
    (GLOBAL, "", "re:rts::\\(anonymous namespace\\)::(t_engineLocale|threadEngineLocaleKey\\(\\)::key)", "the Scope's per-thread engine locale (thread_local, per thread by design) and the pthread key that frees it at thread exit, made once (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:TheNameKeyGenerator|\\(anonymous namespace\\)::thePriming(Mutex|State|Failure)|NameKeyGenerator::perturbForTesting\\(.*\\)::calls", "shared immortal NameKey generator and its priming latch (PLAN-023 Decision 2)"),
    (GLOBAL, "", "re:.*::(key_\\w+|jetKey)", "cached NameKeyType (a NAMEKEY(...) in an inline function): process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:ControlBar::update(OCLTimer|Construction)TextDisplay\\(.*\\)::(descID|barID)", "cached window NameKey held as UnsignedInt: process-wide by PLAN-023 Decision 2"),
    (GLOBAL, "", "re:Object::Object\\(.*\\)::\\w+ModuleData|.*taggedHelperModuleData<\\w+>\\(.*\\)::data", "shared helper ModuleData and Object::Object's references to it: made and tagged with its (Decision 2) NameKey once, under the static-init guard, and only read afterwards (PLAN-023 Phase 5b; every Object used to write the tag again)"),
    (GLOBAL, "", "re:gameWin(System|Input|Tooltip)Table|winLayout(Init|Update|Shutdown)Table|gameWinDrawTable|layoutInitTable", "FunctionLexicon table: names and function pointers; every engine's init interns the same names (Decision 2), and with RTS_ENGINE_CONTEXT FunctionLexicon::loadTable writes a key only while it is unset (a different one from a later boot is refused with a DEBUG_CRASH), so only the first boot writes (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:The(AsciiString|UnicodeString|Dma|MemoryPool|DebugLog)CriticalSection", "critical section, process-wide lock (PLAN-023 Phase 1)"),
    (GLOBAL, "", "re:TheMemoryPoolFactory|TheDynamicMemoryAllocator|theMainInitFlag|theMemoryManagerUsers(Mutex)?|\\(anonymous namespace\\)::TheProcessOperators(State)?", "refcounted process-wide memory manager (PLAN-023 Phase 1b, Decision 3)"),
    (GLOBAL, "", "TheVersion", "build version, identical for every engine"),
    (GLOBAL, "", "GlobalData::m_theOriginal", "empty stand-in: the value lives in EngineContext::originalGlobalData (Phase 1b)"),
    (GLOBAL, "", "re:theEngineEmbeddedMode|theReleaseCrashLogFile", "embedding host's fatal-error mode / release-crash log (PLAN-023 Phase 5 moves the log to per-engine output paths)"),
    (GLOBAL, "", "theInReleaseCrashNoReturn", "thread_local re-entry guard of ReleaseCrashNoReturn: per thread by design"),
    (GLOBAL, "", "re:TheSDL3Window|ApplicationHWnd", "the process's window (at most one rendering engine per process)"),
    (GLOBAL, "", "re:__argc|__argv", "the process's argv (the executable's, set by rlgenerals' launcher; PLAN-023 Phase 5 BootConfig replaces it): boot/shutdown only, serialised by the host (only CommandLine's boot-time parse reads it)"),
    (GLOBAL, "", "re:critSec[1-5]|FilterSoftwareVulkanICDs\\(\\)::hw_icds", "the game executable's own (SDL3Main), unused by an embedding host"),
    (GLOBAL, "", "re:rts::WorkingDirectory::\\w+", "the startup working directory (one per process): boot/shutdown only, serialised by the host (CommandLine's boot-time parse writes and reads it)"),
    (GLOBAL, "", "re:rts::ClientInstance::\\w+", "the process's single-instance mutex"),
    (GLOBAL, "", "re:ReplaySimulation::s_\\w+", "the game executable's multi-replay simulation driver (-simReplay); an embedded engine never runs it"),
    (GLOBAL, "", "OurLanguage", "the installation's language, the same for every engine"),
    (GLOBAL, "", "re:GetRegistryLanguage\\(\\)::(once|val)", "the installation's language string: with RTS_ENGINE_CONTEXT written exactly once, under std::call_once, and never mutated afterwards (PLAN-023 Phase 4's process-global string rule); every engine reads the same install"),
    (GLOBAL, "", "UnicodeString::format_va(wchar_t const*, __va_list_tag*)::s_utf8_locale", "a UTF-8 locale handle, created once"),
    (GLOBAL, "", "s_assetFallbackPath", "the install's asset fallback root: every engine's boot (StdBIGFileSystem::init) sets it, but setAssetRootPath writes only a change, and with RTS_ENGINE_CONTEXT only while it is unset (another root from a later boot is refused with a DEBUG_CRASH): the engines of a process share one install, so only the first boot writes while later engines' file lookups read it (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:s_thread|s_done|s_hasUpdate|s_latestTag", "update checker (menus), one per process"),
    (GLOBAL, "", "re:thread_id_map(_mutex)?|next_thread_id", "pthread-to-Win32 thread id map (CompatLib), process-wide by nature"),
    (CONST, "", "GameSpyColorDefaults", "the online chat colours' defaults (Chat.cpp), which each engine's GameSpyColor (a PER_ENGINE_STATIC, PLAN-023 Phase 5b) starts from; never written"),
    (GLOBAL, "", "NGMP_OnlineServicesManager::getInstance()::instance", "the process's GeneralsOnline session, logged into from the online menus of the one UI engine: every engine's teardown makes the (idle) instance, but with RTS_ENGINE_CONTEXT only a non-headless engine initialises it (GameEngine::init) or shuts it down (~GameEngine), so a headless engine never touches the session; the instance is made under the static-init guard, at teardown only, which the host serialises (PLAN-023 Phase 5b)"),
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
    # ScriptEngine's debugger-window and particle-editor hooks (st_CurrentFrame, st_DebugDLL, st_ParticleDLL,
    # ...) were listed here as debug-only, but every ScriptEngine's constructor, init() and update() write
    # them: they are a PER_ENGINE_STATIC now (PLAN-023 Phase 5b, found by the TSan build), so gone from the
    # library; the slot index is rule:per-engine-static.
    (DEBUG, "", "re:_writeSingleParticleSystem\\(.*\\)::buff[1-4]|_reloadParticleSystemFromINI\\(.*\\)::linebuff|_getParticleSystemName\\(\\)::buff", "particle-editor writer/reader buffers (debug DLL)"),
    (DEBUG, "", "ScriptEngine::getTeamNamed(AsciiString const&)::warnCount", "debug-message limiter"),
    (DEBUG, "", "re:Object::setTriggerAreaFlagsForChangeInPosition\\(\\)::didWarn|PathfindCell::~PathfindCell\\(\\)::warn", "warn-once flag"),
    (DEBUG, "", "re:s_totalOpen", "open-file counter for a debug assert"),
    (DEBUG, "", "re:DebugDisplay::printf\\(.*\\)::text", "debug display text buffer"),
    (DEBUG, "", "re:CommandTranslator::translateGameMessage\\(.*\\)::old\\w+", "debug-command toggles (MSG_META_DEMO_*)"),
    (DEBUG, "", "file:/WWDebug/", "WW memory log"),
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 8 (stage RR1): the render state audit. Every WW3D2 and W3DDevice static that only a render
    # engine reaches, and the debug statistics its draw writes, in one of four classes (the blanket render-only
    # `file:` rules they replace said only "one renderer per process"):
    #   render-per-engine  state of one render engine's device, scene or draw that lives from one call to the
    #                      next: it moves into the engine (W3DRenderState or its owner) in the stage its phase
    #                      names (RR2a-1: DX8Wrapper; RR2a-2: WW3D, ShaderClass, the window, the FPS statistics;
    #                      RR2b: WW3D2's device-dependent subsystems, buffers, texture loader and draw scratch;
    #                      RR3: W3DDevice's shader manager, shadows, terrain, water, view and display);
    #   render-scratch     filled and consumed within one call (a draw, a load): safe for one renderer at a time,
    #                      not for renderers drawing at once; per engine, per thread or a local by its phase;
    #   render-const       the same for every engine and device: built at static initialisation or from
    #                      constants (a phase: still rewritten at every render boot, with the same value, until
    #                      that stage builds it once);
    #   render-process     one per process by design: the D3D8 library, the host's hooks, and what only a user
    #                      interface reaches (every rlgenerals engine keeps GlobalData::m_headless TRUE, the
    #                      viewer's included, so no menu, mouse cursor, control bar or radar is drawn).
    # `engine_state_symbols.py check --render` prints the render-per-engine and render-scratch rows by phase.
    # The runtime probe (engine_state_probe.py) checks this list against the writes a render engine makes.
    (RPROC, "", "re:D3D8Lib|Direct3DCreate8Ptr|DX8Wrapper::Init\\(void\\*, bool\\)::s_d3d8LibMutex", "the D3D8 library and its Direct3DCreate8, loaded per process under a mutex (a failed load is retried) and never freed (RR1): a render engine's Shutdown no longer unloads it from under the next device"),
    (RPROC, "", "DX8Wrapper_FinalReleaseHook", "the embedding host's check before a render device's or Direct3D interface's last release (RR1): set once, before any render device"),
    (RPER, "RR2a-1", "re:_PresentParameters|DX8Wrapper::(s_getNativeDisplaySize|s_getWindowSize|s_pillarboxEnabled|s_pillarboxActive|s_bbW|s_bbH|s_dstX|s_dstY|s_dstW|s_dstH|s_pixelDensity|s_offscreenTex|s_offscreenSurf|s_depthSurf|s_savedBackbuffer|s_savedDepth|FrameStatistics|IsInitted|_EnableTriangleDraw|CurRenderDevice|ResolutionWidth|ResolutionHeight|BitDepth|TextureBitDepth|IsWindowed|DisplayFormat|MultiSampleAntiAliasing|Vertex_Shader|Pixel_Shader|Vertex_Shader_Constants|Pixel_Shader_Constants|Light_Environment|Vertex_Processing_Behavior|Shadow_Map|Ambient_Color|world_identity|RenderStates|TextureStageStates|Textures|render_state|render_state_changed|FogEnable|FogColor|D3DInterface|D3DDevice|CurrentRenderTarget|CurrentDepthBuffer|DefaultRenderTarget|DefaultDepthBuffer|IsRenderToTexture|_MainThreadID|CurrentDX8LightEnables|IsDeviceLost|ZBias|ZNear|ZFar|ProjectionMatrix|DX8Transforms|CurrentCaps|DrawPolygonLowBoundLimit|CurrentAdapterIdentifier|FrameCount|m_pCleanupHook)|LastFrameStatistics|DX8Wrapper_HeadlessRender|DX8Wrapper_PreserveFPU|_DX8SingleThreaded|_RenderDevice(Name|ShortName|Description)Table", "DX8Wrapper's device state: its interface, device, caps, presentation parameters, render targets, cached render/texture-stage states and transforms, the pillarbox, the frame statistics and the host's render switches; set by each render engine's Init, Set_Render_Device and every draw, cleared by its Shutdown, so each renderer needs its own (W3DRenderState)"),
    (RPER, "RR2a-1", "_Hwnd", "the render window handle, dx8wrapper.cpp's (RR2a-1) and ww3d.cpp's (RR2a-2), one per TU: the render engine's window (null for a windowless one)"),
    (RCONST, "", "re:DX8Wrapper::Find_Color_And_Z_Mode\\(.*\\)::_formats(16|32)", "format lists that Find_Color_And_Z_Mode only reads: can be const"),
    (RPER, "RR2a-1", "file:WW3D2/statistics\\.cpp", "Debug_Statistics: the draw's polygon, vertex, draw-call and texture counters, reset by WW3D::Begin_Render and written by every Record_DX8_Polys_And_Vertices (ww3d.cpp, dx8wrapper.h): debug-only by purpose, but every render engine's draw writes them, so they become a struct in W3DRenderState"),
    (DEBUG, "", "file:WW3D2/dx8rendererdebugger\\.cpp", "WW3D renderer debugger (DX8RendererDebugger::Enabled is never set)"),
    (RPER, "RR2a-2", "re:W3DDisplay::(gatherDebugStats|updateAverageFPS)\\(\\)::\\w+", "the FPS history and debug statistics W3DDisplay::draw updates on every frame (updateAverageFPS unconditionally): per render engine, as W3DDisplay members"),
    (RPER, "RR2a-2", "re:WW3D::(IsSortingEnabled|PixelCenter[XY]|RenderBackend|IsInitted|IsRendering|IsCapturing|IsScreenUVBiased|AreDecalsEnabled|DecalRejectionDistance|AreStaticSortListsEnabled|MungeSortOnLoad|OverbrightModifyOnLoad|Movie|PauseRecord|RecordNextFrame|UserStat[0-2]|DefaultNativeScreenSize|DefaultStaticSortLists|CurrentStaticSortLists|DefaultDebugMaterial|DefaultDebugShader|LightmapDebugShader|PrelitMode|ExposePrelit|SnapshotActivated|ThumbnailEnabled|MeshDrawMode|NPatchesGapFillingMode|NPatchesLevel|IsTexturingEnabled|IsColoringEnabled|LastFrameMemoryAllocations|LastFrameMemoryFrees|TextureFilter|AnisotropyLevel|Lite)|_TextureReduction|_TextureMinDim|_LargeTextureExtraReductionEnabled|WW3D::Make_Screen_Shot\\(.*\\)::frame_number", "WW3D's render settings and render-loop state: WW3D::Init and Shutdown, the render loop, the static sort lists and the options (texture filter, texture reduction) write them, so each render engine needs its own"),
    (RCONST, "", "VertexMaterialClass::Apply_Null()::default_settings", "the null material's settings, constant data"),
    (RCONST, "", "DAZZLE_INI_FILENAME", "the dazzle INI's name, never reassigned"),
    (RPER, "RR2a-2", "re:ShaderClass::(ShaderDirty|CurrentShader)|_PolygonCullMode", "the shader last applied to the device and its dirty flag, and the cull mode: device state each render engine's draw writes"),
    (RCONST, "RR2a-2", "re:ShaderClass::_Preset\\w+Shader", "the 22 preset shaders, built at static initialisation and only copied: made const"),
    (RCONST, "", "re:(src|dst)BlendLUT", "blend-mode lookup tables, built at static initialisation from constants and only read"),
    (RPER, "RR2b", "re:VertexMaterialClass::Presets|BoxRenderObjClass::IsInitted|_BoxMaterial|PointGroupClass::PointMaterial|_MissingTexture|DX8TextureManagerClass::Managed_Textures|Render2DClass::ScreenResolution|_surface|CapsWorkString", "a device-dependent object Do_Onetime_Device_Dependent_Inits (or WW3D::Init) makes and its Shutdowns free, or state they set from the device: per render engine"),
    (RPER, "RR2b", "re:_Dynamic(DX8|Sorting)(Vertex|Index)(Buffer|Array)(Size|Offset)?|_(Vertex|Index)BufferTotal(Vertices|Indices|Size)|_(Vertex|Index)BufferCount", "the dynamic vertex/index buffers, their cursors, and the buffer statistics every buffer's creation and release counts: per render engine"),
    (RCONST, "", "_DynamicFVFInfo", "the dynamic vertex buffers' vertex format, built at static initialisation"),
    (RPER, "RR2b", "re:PointGroupClass::(_TriVertexLocationOrientationTable|_QuadVertexLocationOrientationTable|_TriVertexUVFrameTable|_QuadVertexUVFrameTable)", "rebuilt from constants by every render engine's PointGroupClass::_Init (the same values): built once for the process (std::call_once), then only read"),
    (RPER, "RR2b", "re:Tris|SortingTris|Quads|SortingQuads", "PointGroupClass's shared index buffers, made by _Init and freed by _Shutdown: per render engine"),
    (RSCR, "RR2b", "re:PointGroupClass::(compressed_(loc|diffuse|size|orient|frame)|transformed_loc)|VertexLoc|VertexDiffuse|VertexUV|remap", "PointGroupClass's render scratch, filled and consumed within one draw"),
    (RCONST, "", "re:PointGroupClass::_ScreenspaceVertexLocationSizeTable|GroundMultiplier[XY]", "constant vectors, initialised statically and only read"),
    (RCONST, "", "re:missing_image_(palette|pixels)", "the missing texture's image, constant data"),
    (RCONST, "", "re:MetalMapManagerClass::(_NormalTable|initialize_normal_table\\(\\)::_normal_table)", "the metal map normal table, built from constants on first use"),
    (RCONST, "", "MaterialPassClass::EnablePerPolygonCulling", "never written: Enable_Per_Polygon_Culling has no caller"),
    (RCONST, "", "re:WW3D(Z)?FormatToD3DFormatConversionArray", "format conversion table, constant data"),
    (RCONST, "RR2b", "re:D3DFormatToWW3D(Z)?FormatConversionArray", "the reverse format tables, rebuilt from the constant ones at every render boot (Init_D3D_To_WW3_Conversion, the same values): built once for the process"),
    (RCONST, "", "DynamicMeshModel::Render(RenderInfoClass&)::default_uv", "a const local static, (0, 0)"),
    (RPROC, "", "DX8WebBrowser::hWnd", "the embedded web browser's window, a Linux stub never initialised"),
    (RCONST, "", "re:_BoxVerts|_BoxFaces|_BoxVertexNormals|W3DVolumetricShadow::RenderMeshVolumeBounds\\(.*\\)::_Box(Verts|Faces)", "unit-box geometry (boxrobj.cpp's, and W3DVolumetricShadow::RenderMeshVolumeBounds'), initialised statically from constants and only read"),
    (RCONST, "RR2b", "_BoxShader", "the box shader, set to the same preset by every BoxRenderObjClass::Init: set once"),
    (RCONST, "RR2b", "BoxRenderObjClass::DisplayMask", "the collision boxes' display mask, rewritten with the same value (0) by every render boot (W3DDisplay::init's WW3D::Set_Collision_Box_Display_Mask): set once"),
    (RSCR, "RR2b", "re:BoxRenderObjClass::(render_box|vis_render_box)\\(.*\\)::verts", "the box's vertices, filled and consumed within one draw"),
    (RCONST, "", "re:DAZZLE_(LIST|INTENSITY_POW|SIZE_POW|AREA|SCALE_X|SCALE_Y|INTENSITY|DIRECTION_AREA|DIRECTION|TEXTURE|COLOR|TEST_COLOR|LENSFLARE)_STRING|HALO_(INTENSITY_POW|SCALE_X_STRING|SCALE_Y_STRING|INTENSITY_STRING|TEXTURE_STRING|COLOR_STRING)|FADEOUT_(START|END)_STRING|SIZE_OPTIMIZATION_LIMIT_STRING|HISTORY_WEIGHT_STRING|USE_CAMERA_TRANSLATION|BLINK_(PERIOD|ON_TIME)_STRING|LENSFLARE_(LIST|TEXTURE)_STRING|FLARE_(COUNT|LOCATION|SIZE|COLOR|UV)_STRING|RADIUS_STRING", "dazzle.ini key names, never reassigned"),
    (RPER, "RR2b", "re:temp_ptrs|types|type_count|current_dazzle_layer|lensflares|lensflare_count|_VisibilityHandler|DazzleRenderObjClass::_dazzle_rendering_enabled", "the dazzle types and lens flares loaded from dazzle.ini at WW3D::Init and freed at Shutdown, the current dazzle layer and the visibility handler and switch: per render engine. Every engine's dazzle objects read the types (the constructors, Set_Transform, Get_Type_ID): since RR1 only the engine that owns the render device sees them (dazzle.cpp's Engine_Types and Engine_Type_Count), so a headless engine beside a renderer reads none, as in its solo run"),
    (RCONST, "RR2b", "re:default_dazzle_shader|default_halo_shader|vis_shader|debug_shader", "dazzle shaders, set to the same presets at every init: set once"),
    (RCONST, "", "_DefaultVisibilityHandler", "the default dazzle visibility handler, a stateless object"),
    (RSCR, "RR2b", "re:_DecalPoly[01]|_VisPoly[01]|temp_apt|_TempTransformedVertexBuffer|_TempClipFlagBuffer|LocationHash|DuplicateLocationHash|SideHash", "clipping, decal and mesh-model scratch (decalmsh, visrasterizer, mesh, meshmdl): filled and consumed within one call"),
    (RCONST, "", "re:_LightingLODCutoff2?", "written only by LightEnvironmentClass::Set_Lighting_LOD_Cutoff, which Zero Hour never calls: the initial values for every engine"),
    (RSCR, "RR2b", "re:LineGroupClass::Render\\(.*\\)::offset(_[abc])?", "LineGroupClass::Render's corner offsets, recomputed within each draw"),
    (RCONST, "", "MeshMatDescClass::NullShader", "the 'no shader' marker, built at static initialisation and only compared against"),
    (RSCR, "RR2b", "re:ParticleBufferClass::Render_Line\\(RenderInfoClass&\\)::tmp_(points|diffuse|id)", "ParticleBufferClass::Render_Line scratch: filled and consumed within one draw"),
    (RSCR, "RR2b", "SimpleSceneClass::Customized_Render(RenderInfoClass&)::lenv", "the light environment each draw rebuilds before it uses it"),
    (RCONST, "", "re:TexProjectClass::Init_(Multiplicative|Additive)\\(\\)::(mult|add)_shader", "shaders built once, under the static-init guard, from constants"),
    (RPER, "RR2b", "re:_(Min|Mag)TextureFilters|_MipMapFilters", "the texture filter tables TextureFilterClass::_Init_Filters fills from the device's caps and the options: per render engine"),
    (RPER, "RR2b", "re:TextureLoader::(TextureLoadSuspended|TextureInactiveOverrideTime)|_(Foreground|Background)Queue|_(TexLoad|CubeTexLoad|VolTexLoad)FreeList", "the texture loader's queues, free lists and switches: loads a render engine queues and TextureLoader::Deinit frees, so per render engine (TextureInactiveOverrideTime is reset by every Init)"),
    (RPROC, "RR2b", "re:_(Foreground|Background)CriticalSection", "the texture loader's locks: process-wide, but no longer held across a synchronous load (RR2b)"),
    (RPROC, "", "_TextureLoadThread", "the background texture loader thread, which never runs on Unix (ThreadClass::Execute returns at once): nothing is loaded off the engine's thread"),
    (RPER, "RR2b", "re:ThumbnailManagerList|GlobalThumbnailManager", "the texture thumbnail managers TextureLoader::Init makes and Deinit frees: per render engine (or process state refcounted under a mutex), so that one engine's Deinit cannot free another's. Every engine's TextureClass constructor looks its texture up in them: since RR1 only the engine that owns the render device sees them (Peek_Thumbnail_Instance_From_Any_Manager and Peek_Thumbnail_Manager answer null for any other), so a headless engine beside a renderer reads none, as in its solo run"),
    (RCONST, "", "ThumbnailManagerClass::CreateThumbnailIfNotFound", "never written: Create_Thumbnail_If_Not_Found has no caller"),
    (RCONST, "", "SortingRendererClass::_EnableTriangleDraw", "never written: _Enable_Triangle_Draw has no caller"),
    (RPER, "RR2b", "re:DEFAULT_SORTING_(POLY|VERTEX)_COUNT", "the sorting renderer's buffer sizes, which a render boot with -incrementalAGPBuf shrinks (W3DDisplay::init's SortingRendererClass::SetMinVertexBufferSize) and nothing restores, so they stick for every later render engine: per render engine, with the sorting renderer's lists"),
    (RPER, "RR2b", "re:sorted_list|unsorted_list|clean_list|temp_index_array(_count)?", "the sorting renderer's node lists and index buffer: per render engine"),
    (RSCR, "RR2b", "re:overlapping_(node|polygon|vertex)_count|overlapping_nodes", "the sorting renderer's overlap scratch, filled and consumed within one flush"),
    (RPER, "RR2b", "ShatterPatterns", "the shatter patterns ShatterSystem::Init loads and Shutdown frees: per render engine"),
    (RSCR, "RR2b", "re:ClipPools|MeshFragments|TmpVertPositions|TmpVertNormals", "the shatter system's clipping scratch, filled and consumed within one shatter"),
    (RSCR, "RR2b", "_LineRenderer", "segline.cpp's and streak.cpp's line renderers: set up and used within one draw"),
    (RSCR, "RR2b", "re:PredictiveLODOptimizerClass::\\w+", "the predictive LOD optimiser's object arrays and cost, filled and consumed within one frame's optimisation"),
    (RPROC, "", "re:AnimatedSoundMgrClass::\\w+", "W3D animated sounds (WWAudio), never initialised by the game (not used by its audio)"),
    (RPER, "RR2b", "Vector3Randomizer::Randomizer", "the RNG of the W3D particle emitters' randomizers: per engine with an identical seed (the render engines have no particles today: ParticleSystemManagerDummy while m_headless)"),
    (RCONST, "", "re:detailOpaqueShader|detailAlphaShader|zFillAlphaShader|PlayerColorShader", "terrain, tree, bib, bridge, road, status-circle, water and player-colour shaders, one per TU, built at static initialisation and only read (copied into objects, or applied)"),
    (RPER, "RR3", "re:FlatHeightMapRenderObjClass::updateCenter\\(.*\\)::prev\\w+|visM(in|ax)[XY]|HeightMapRenderObjClass::renderExtraBlendTiles\\(\\)::maxBlendTiles", "terrain draw state kept between frames: members of the height map"),
    (RPER, "RR3", "re:W3DFilters|W3DShaders|W3DShadersPassCount|W3DShaderManager::\\w+|screen(Default|BW|BWFilterDOT3|CrossFade|MotionBlur)Filter\\w*|Screen(BW|CrossFade|MotionBlur)Filter::\\w+|(shroud|flatShroud|mask|cloud)TextureShader|(terrain|flatTerrain|road)Shader(2Stage|8Stage|PixelShader)", "W3DShaderManager: its shader and filter tables, the chosen chipset, the render-to-texture surfaces, and the shader and filter objects (some holding D3D pixel shaders and textures; the screen filters' fade and motion-blur state is script-driven): one per-engine struct made by init and freed by shutdown"),
    (RCONST, "RR3", "re:\\w+(Shader|Filter)List", "tables of pointers to the shader and filter objects above, never written: they move with them"),
    (RPER, "RR3", "W3DView::update()::followFactor", "the camera's follow smoothing, kept between frames: a W3DView member"),
    (RCONST, "", "waveTypeInfo", "the water wave types' parameters, constant data"),
    (RPER, "RR3", "WaterTracksRenderSystem::update()::iLastTime", "the water tracks' last update, by the wall clock, which only a render engine's draw writes (WaterTracksRenderSystem::flush calls update). It never reaches an image: the difference it yields goes to WaterTracksObj::update, which ignores it and returns TRUE, and the waves advance by the logic time step in WaterTracksObj::render once per draw, 33 ms under the launcher (checked in RR1: Tournament Desert draws two water-track modules every frame, and their clocks and the render digest match between render-solo runs, one of them with every core busy). A write-only value, per engine so that one engine's draw does not write it for another"),
    (RPER, "RR3", "re:shadowCameraFrustum|LightPosWorld|shadow(Decal)?(Vertex|Index)BufferD3D|nShadow(Decal)?(VertsInBuf|StartBatchVertex|IndicesInBuf|StartBatchIndex|PolysInBatch|VertsInBatch)|lastActiveVertexBuffer", "the shadow managers' shared vertex/index buffers, their cursors and the light and frustum they draw with: members of the per-engine shadow managers"),
    (RCONST, "", "re:SHADOW_(DECAL_)?(VERTEX|INDEX)_SIZE", "shadow buffer sizes, never changed"),
    (RSCR, "RR3", "re:b[ce][XYZ]|W3DVolumetricShadow::RenderMeshVolumeBounds\\(.*\\)::verts|W3DVolumetricShadow::updateVolumes\\(float\\)::(aaBox|sphere)|W3DProjectedShadowManager::(renderProjectedTerrainShadow|flushDecals)\\(.*\\)::mWorld|W3DProjectedShadowManager::renderShadows\\(RenderInfoClass&\\)::(aaBox|sphere)", "shadow scratch (bounds, boxes, matrices), filled and consumed within one call"),
    (RCONST, "", "guard variable for W3DProjectedShadow::updateTexture(Vector3&)::uvData", "the guard of a constant uv table (the table itself is not in the library's symbols)"),
    (RSCR, "RR3", "guard variable for W3DProjectedShadowManager::queueDecal(W3DProjectedShadow*)::objCenter", "the guard of queueDecal's centre scratch (the static itself is not in the library's symbols), set within each call"),
    (RCONST, "", "re:W3DVolumetricShadow::Update\\(\\)::originCompareVector|W3DProjectedShadowManager::(addShadow|createDecalShadow)\\(.*\\)::defaultDecalName", "constants built once under the static-init guard"),
    (RPER, "RR3", "re:W3DDisplay::draw\\(\\)::(now|timeMultiplierCounter|couldRender)", "W3DDisplay::draw's frame timing and device-lost state, kept between frames: W3DDisplay members (now is the wall clock: logic-frame time)"),
    (RPROC, "", "re:s_filtered(Resolutions|Dirty)", "the options menu's resolution list: " + UI),
    (RCONST, "", "RTS3DScene::updateFixedLightEnvironments(RenderInfoClass&)::id", "a constant (1, 1, 1) vector the infantry light is capped to, built once under the static-init guard"),
    (RPER, "RR3", "re:DummyTexture|W3DShroud::interpolateFogLevels\\(.*\\)::prevTime", "the shroud's dummy texture (made at init, released at shutdown) and its fog interpolation clock: members of the per-engine W3DShroud"),
    (RCONST, "", "animationDisableOverride", "the ghost objects' material override, default-constructed and only pointed at"),
    (RCONST, "", "(anonymous namespace)::LoadUnicodeFallbackFont(int, bool, char const*)::kFallbackUnicodeFonts", "font name list, constant data"),
    (RPROC, "", "re:W3DRadar::(drawHeroIcon|drawEvents)\\(.*\\)::\\w+", "W3DRadar's draw (the radar window): " + UI),
    (RPROC, "", "file:/W3DDevice/GameClient/GUI/", "W3D menus, control bar, gadgets and window borders: " + UI),
    (RPROC, "", "file:W3DMouse\\.cpp", "the W3D mouse cursor (its textures, models, images and animation thread): only an engine with a real mouse, " + UI),
    # ---------------------------------------------------------------------------------------------------
    # Render only (and UI only).
    # Drawable::s_modelLockCount was listed here as render-only, but while the one rendering engine's scene
    # locks it every headless engine's getDrawModules reads it on its own thread: it is an EngineContext
    # field now (drawableModelLockCount; PLAN-023 Phase 8, stage RR0a), so gone from the library.
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
    (RENDER, "", "file:Win32Mouse\\.cpp", "the Win32 mouse cursor: headless has MouseDummy and no input"),
    (RENDER, "", "re:SmudgeSet::m_freeSmudgeList", "heat-haze smudges, drawn only"),
    (GLOBAL, "", "DX8Wrapper_IsWindowed", "the process's assert switch (Debug.cpp's ignoringAsserts): every headless engine's command line (parseHeadless) sets it false so asserts stay off for the rest of the process, after TheGlobalData is gone too; a std::atomic, since boots write it while other engines' assert paths read it (PLAN-023 Phase 5b)"),
    # Only device-only WW3D2 files: mesh, meshmdl, meshgeometry, rendobj, texture, ww3d, dx8renderer,
    # part_emt and part_buf are reached by a headless engine too (models, bones, emitters, asset loading),
    # so their statics are classified by name below or above.
    (RENDER, "", "parent", "a menu's parent window, one per menu TU (in-game popup, lobby, score screen, map select, ...): " + UI),
    (GLOBAL, "", "pingImages", "the online lobby's and WOL game setup's ping images: " + NET),
    (DEBUG, "", "statistics_requested", "DX8 mesh-renderer statistics request (debug display)"),
    (DEBUG, "", "ParticleBufferClass::TotalActiveCount", "particle-buffer statistics counter; its getter Get_Total_Active_Count has no caller"),
    (DEBUG, "", "RenderObjPersistFactoryClass::Load(ChunkLoadClass&) const::count", "warning limiters (two in the function)"),
    (CONST, "", "DX8TextureCategoryClass::m_gForceMultiply", "never written: SetForceMultiply has no caller"),
    (CONST, "", "MeshClass::Legacy_Meshes_Fogged", "never written"),
    (CONST, "", "ParticleBufferClass::LODMaxScreenSizes", "never written: Set_LOD_Max_Screen_Size has no caller"),
    (CONST, "", "re:ParticleEmitterClass::(DebugDisable|DefaultRemoveOnComplete)", "never written: Disable_All_Emitters / Set_Default_Remove_On_Complete have no caller"),
    (RENDER, "", "listboxLobbyGamesLarge", UI),
]
