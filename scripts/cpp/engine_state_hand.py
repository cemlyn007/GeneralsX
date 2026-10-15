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
# There are no per-engine entries left: PLAN-023 Phases 2-4 moved every one into the engine, and Phase 5b (engines on
# separate threads) the last 18, the "threads only" scratch that was correct for several engines stepped on ONE
# thread, into thread_locals, PER_ENGINE_STATICs, locals or (AssetStatusClass::Instance) a lock. A symbol classified
# per-engine is therefore a regression, and `check` fails on it in every mode. A thread_local is process-global here
# (per thread by design), and must stay small: the library's TLS is initial-exec, so it comes out of glibc's static
# TLS surplus for a dlopened library (a few KB at most; Phase 2's 5 KB XferLoad buffers failed to load).
#
# Process-wide state must be written once (at static initialisation or the first boot) and only read after
# that, or be atomic or locked: engines boot while others step on their threads (PLAN-023 Phase 5b). A note
# that says "boot/shutdown only, serialized by the host" names state that only boots and teardowns touch,
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
# freeStaticStrings (GameWindowManagerScript.cpp: the .wnd parse's callback strings, a PER_ENGINE_STATIC).
HEADLESS_GUI = (
    "ControlBar/|LoadScreen\\.cpp|GameWindowManager\\.cpp|Shell/(Shell|ShellMenuScheme)\\.cpp|"
    "GUICallbacks/(Diplomacy|InGameChat|InGamePopupMessage|ControlBarPopupDescription)\\.cpp"
)
W3D_RENDER = "W3D render path (device, shaders, draw lists): only the one rendering engine per process reaches it (PLAN-023 Phase 8)"
LOADER = "stateless W3D prototype loader/persist factory, registered once at static initialization"

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
    # thread_local (per thread by design, so process-global here): PathNode::computeDirectionVector()'s dir.
    # The two XferLoad buffers are locals now, and both checkfortransitionsnum and inCRCGen per-engine
    # statics (so gone).
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 3: device-layer state a headless engine uses.
    # Done (PLAN-023 Phase 3 PR): W3DDisplay's scenes and asset manager, WW3DAssetManager::TheInstance and
    # WW3D's timing statics are EngineContext fields behind rts::ContextField stand-ins; TheDX8MeshRenderer
    # and its three lists, the particle buffers' rand_gen and the mesh/material/texture/decal ID counters are
    # PER_ENGINE_STATICs; WorldHeightMap::m_alphaTiles is a member (so gone from the library, the slot
    # indexes rule:per-engine-static). What is left here is process-wide by design.
    # ---------------------------------------------------------------------------------------------------
    # PLAN-023 Phase 4: caches and remaining statics.
    # Done (PLAN-023 Phase 4 PR): every function-local cache of a per-engine pointer (the seven ActiveBody
    # particle templates, the upgrade/thing template caches, muzzle, debrisTemplate, genericBridgeTemplate,
    # the seven WaveGuideUpdate particle templates), the fifteen static AudioEventRTS (PER_ENGINE_STATICs,
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
    # ---------------------------------------------------------------------------------------------------
    # Process-global on purpose.
    (GLOBAL, "", "re:rts::t_engine|rts::g_noEngine|rts::\\(anonymous namespace\\)::theNextSlotIndex", "the EngineContext mechanism itself (PLAN-023 Decision 1)"),
    (GLOBAL, "", "rts::(anonymous namespace)::theEngineFloatingPointMode", "the engines' floating-point mode (x87 control word and MXCSR) learnt from setFPMode() by the first Scope, and atomic (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:rts::\\(anonymous namespace\\)::(t_engineLocale|threadEngineLocaleKey\\(\\)::key)", "the Scope's per-thread engine locale (thread_local, per thread by design) and the pthread key that frees it at thread exit, made once (PLAN-023 Phase 5b)"),
    (GLOBAL, "", "re:TheMemoryPoolFactory|TheDynamicMemoryAllocator|theMainInitFlag|theMemoryManagerUsers(Mutex)?|\\(anonymous namespace\\)::TheProcessOperators(State)?", "refcounted process-wide memory manager (PLAN-023 Phase 1b, Decision 3)"),
    (GLOBAL, "", "GlobalData::m_theOriginal", "empty stand-in: the value lives in EngineContext::originalGlobalData (Phase 1b)"),
    (GLOBAL, "", "theEngineEmbeddedMode", "embedding host's fatal-error mode: atomic (PLAN-023 Phase 5 moves the release-crash log to per-engine output paths)"),
    (GLOBAL, "", "theReleaseCrashLogFile", "thread_local handle of the release-crash log being written: per thread by design (PLAN-023 Phase 5 moves the log to per-engine output paths)"),
    (GLOBAL, "", "theEngineHasFaulted", "embedding host's sticky fault latch: atomic (PLAN-023 Phase 5 moves the release-crash log to per-engine output paths)"),
    (GLOBAL, "", "theInReleaseCrashNoReturn", "thread_local re-entry guard of ReleaseCrashNoReturn: per thread by design"),
    (GLOBAL, "", "re:__argc|__argv", "the process's argv (the executable's, set by rlgenerals' launcher; PLAN-023 Phase 5 BootConfig replaces it): boot/shutdown only, serialized by the host (only CommandLine's boot-time parse reads it)"),
    (GLOBAL, "", "re:critSec[1-5]|FilterSoftwareVulkanICDs\\(\\)::hw_icds", "the game executable's own (SDL3Main), unused by an embedding host"),
    (GLOBAL, "", "re:rts::WorkingDirectory::\\w+", "the startup working directory (one per process): boot/shutdown only, serialized by the host (CommandLine's boot-time parse writes and reads it)"),
    (GLOBAL, "", "re:s_thread|s_done|s_hasUpdate|s_latestTag", "update checker (menus), one per process"),
    (GLOBAL, "", "re:thread_id_map(_mutex)?|next_thread_id", "pthread-to-Win32 thread id map (CompatLib), process-wide by nature"),
    (GLOBAL, "", "NGMP_OnlineServicesManager::getInstance()::instance", "the process's GeneralsOnline session, logged into from the online menus of the one UI engine: with RTS_ENGINE_CONTEXT only a non-headless engine calls getInstance() (GameEngine::init, every update, its menus and ~GameEngine; headless engines skip it), and there is at most one such engine per process, so no other engine touches the instance (PLAN-023 Phase 5b)"),
    # Not the whole of GameNetwork/: GameInfo.cpp (skirmish setup), LANGameInfo.cpp, GameMessageParser.cpp and
    # NetworkUtil.cpp are reached without a network, so their statics are classified by name.
    (GLOBAL, "", "file:/GameNetwork/(GameSpy/|GeneralsOnline/|WOLBrowser/|GameSpy\\w*\\.cpp|LANAPI\\w*\\.cpp|NAT\\.cpp|FirewallHelper\\.cpp|Connection(Manager)?\\.cpp|DisconnectManager\\.cpp|Network\\.cpp|GUIUtil\\.cpp|Transport\\.cpp|udp\\.cpp|FileTransfer\\.cpp|DownloadManager\\.cpp|IPEnumeration\\.cpp|NetPacket\\w*\\.cpp|NetCommand\\w*\\.cpp|NetMessageStream\\.cpp|FrameData\\w*\\.cpp|FrameMetrics\\.cpp|User\\.cpp)", NET),
    (CONST, "", "re:rts::WorkingDirectory::saveStartupWorkingDirectory\\(\\)::len", "captures the process's own startup cwd (::GetCurrentDirectory): the same OS-level value for every engine in the process, written once"),
    (GLOBAL, "", "OPENSSL_ia32cap_P", "OpenSSL CPU capability word (third-party)"),
    (GLOBAL, "", "re:stbi__flip_vertically_on_write|stbi_write_(force_png_filter|png_compression_level|tga_with_rle)", "stb_image_write configuration, never changed"),
    # ---------------------------------------------------------------------------------------------------
    # Constant.
    (CONST, "", "re:LZHL(De|En)coderStat::\\w+Table0", "LZHL static coding tables"),
    (CONST, "", "re:gli::.*::Table|stbiw__encode_png_line\\(.*\\)::(firstmap|mapping)|pi_lo|tiny", "table/constant in header-only third-party code"),
    (CONST, "", "re:replayExtention|lastReplayFileName|PORTABLE_(SAVE|MAPS|USER_MAPS)|g_csfFile|g_strFile|statsDir|ignoredChars", "constant string, never reassigned"),
    # ---------------------------------------------------------------------------------------------------
    # Debug only.
    (DEBUG, "", "re:TheDebugIgnoreSyncErrors|TheCurrentIgnoreCrashPtr|g_LastErrorDump", "debug"),
    # ScriptEngine's debugger-window and particle-editor hooks (st_CurrentFrame, st_DebugDLL, st_ParticleDLL,
    # ...) were listed here as debug-only, but every ScriptEngine's constructor, init() and update() write
    # them: they are a PER_ENGINE_STATIC now (PLAN-023 Phase 5b, found by the TSan build), so gone from the
    # library; the slot index is rule:per-engine-static.
    # ScriptEngine::getTeamNamed(AsciiString const&)::warnCount, Object::setTriggerAreaFlagsForChangeInPosition()::
    # didWarn and PathfindCell::~PathfindCell()::warn were listed here as debug-only "warn-once" flags, but each
    # is written on a release sim path by every engine's own thread (getTeamNamed's own script-evaluation thread,
    # the object update that changes trigger-area membership, or the pathfinder freeing its cells): they are
    # PER_ENGINE_STATICs now (02/10/2026), so gone from the library as function-local
    # statics; the slot index is rule:per-engine-static (same treatment as the st_* hooks above). The same
    # audit found LocalFile.cpp's s_totalOpen and ParticleBufferClass::TotalActiveCount had no reader (the
    # former a commented-out DEBUG_LOG, the latter a getter with no caller) and no debug assert either, so
    # both were deleted rather than reclassified; they are gone from the library too.
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
    #                      constants (a phase: constant today, but that stage still changes it: its note says how);
    #   render-process     one per process by design: the D3D8 library, the host's hooks, and what only a user
    #                      interface reaches (every rlgenerals engine keeps GlobalData::m_headless TRUE, the
    #                      viewer's included, so no menu, mouse cursor, control bar or radar is drawn).
    # `engine_state_symbols.py check --render` prints the render-per-engine and render-scratch rows by phase.
    # The runtime probe (engine_state_probe.py) checks this list against the writes a render engine makes.
    (RPROC, "", "DX8Wrapper_FinalReleaseHook", "the embedding host's check before a render device's or Direct3D interface's last release (RR1): set once, before any render device"),
    # RR2a-1 moved DX8Wrapper's device state (its class statics, dx8wrapper.cpp's file statics and globals) and
    # Debug_Statistics' counters (statistics.cpp) into W3DRenderState, one per render engine (EngineContext::w3dRender,
    # heap-allocated by DX8Wrapper::Init, freed by Destroy_Render_State at the end of WW3D::Shutdown from RR2a-2); the host's two render switches are EngineContext fields.
    (RCONST, "", "W3DRenderState::Defaults", "the render state an engine without one reads (RR2a-1): every field at its upstream initial value, built at static initialization and never written (an engine that renders reads and writes its own W3DRenderState; a host makes its pages read-only, W3D_Protect_Render_Defaults, RR2a-2)"),
    # RR2a-2 moved WW3D's statics (its render settings and render-loop state, ww3d.cpp's file statics) into WW3DState,
    # one per render engine (EngineContext::ww3dState, allocated and freed with w3dRender), and ShaderClass's device
    # state (CurrentShader, ShaderDirty, _PolygonCullMode) into W3DRenderState; the 22 preset shaders are const
    # (constant-initialised, so read-only data); TheSDL3Window and ApplicationHWnd are EngineContext fields; and
    # W3DDisplay's FPS history and debug statistics are its members.
    (RCONST, "", "WW3DState::Defaults", "the WW3D state an engine without one reads (RR2a-2): every field at its upstream initial value, constant-initialized and never written (a host makes its pages read-only, W3D_Protect_Render_Defaults)"),
    # RR2b made WW3D2's device-dependent subsystems and draw scratch per engine, all PER_ENGINE_STATICs (so gone
    # from the library; the slot indexes are rule:per-engine-static): the missing texture, the dynamic vertex and
    # index buffers with their cursors and statistics, the vertex material presets, the box render system, the point
    # group tables, material, index buffers and scratch, the shatter system, the sorting renderer's lists and
    # scratch, the managed-texture list, the texture filter tables, Render2DClass's screen resolution, font3d's
    # surface, the caps log string, the dazzle types, lens flares, layer and handler, the predictive LOD optimiser,
    # the decal, visibility, mesh and mesh-model scratch, the scene's light environment, the particle line scratch,
    # TexProjectClass's two shaders (changed at every call: scratch, not constants), the ring and sphere LOD meshes
    # (every draw rescales or recolours them), the texture loader's queues, free lists and switches, the thumbnail
    # managers and Vector3Randomizer's RNG (the same seed for every engine). LineGroupClass::Render's offsets and
    # the box vertices are locals; the unused _LineRenderer, temp_ptrs and _TempClipFlagBuffer are left out.
    # The terrain, tree, bib, bridge, road, status-circle, water and player-colour shaders (detailOpaqueShader,
    # detailAlphaShader, zFillAlphaShader, PlayerColorShader; one per TU) left the writable data at RR2a-2: they
    # are `static const`, and ShaderClass's constructor from bits is constexpr, so they are constant-initialised
    # read-only data.
    # RR3 made W3DDevice's draw state per engine. W3DShaderManager's statics, its shader and filter objects and their
    # lists, and the screen filters' statics are one PER_ENGINE_STATIC (W3DShaderManagerState_perEngine, so gone from
    # the library). The shadows' buffers, cursors, culling box, camera frustum and light position are members of the
    # per-engine shadow managers; the height map's, flat height map's, view's, display's, water tracks', shroud's and
    # smudge manager's statics are members of their owners; the shadow and height-map scratch, draw's letterbox time,
    # the infantry light cap and the shroud's debug texture are locals. The reverse format tables are constexpr. The
    # snow table, WWMath::Random_Float and the shatter pattern draw from per-engine generators (PER_ENGINE_STATICs).
    # ---------------------------------------------------------------------------------------------------
    # Render only (and UI only).
    # Drawable::s_modelLockCount was listed here as render-only, but while the one rendering engine's scene
    # locks it every headless engine's getDrawModules reads it on its own thread: it is an EngineContext
    # field now (drawableModelLockCount; PLAN-023 Phase 8, stage RR0a), so gone from the library.
    (RENDER, "", "file:/GUI/(?!" + HEADLESS_GUI + ")", UI),
    (GLOBAL, "", "DX8Wrapper_IsWindowed", "the process's assert switch (Debug.cpp's ignoringAsserts): every headless engine's command line (parseHeadless) sets it false, and a rendering engine's DX8Wrapper::Init (false) and Set_Render_Device (the device's windowed flag) write it too, so it holds the last writer's value, not a fixed one; a std::atomic, since boots write it while other engines' assert paths read it (PLAN-023 Phase 5b)"),
    # Only device-only WW3D2 files: mesh, meshmdl, meshgeometry, rendobj, texture, ww3d, dx8renderer,
    # part_emt and part_buf are reached by a headless engine too (models, bones, emitters, asset loading),
    # so their statics are classified by name below or above.
]
