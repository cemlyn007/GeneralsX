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
// FILE: EngineSingletons.inl
// GeneralsX @feature cemlyn007 28/09/2026 The per-engine singletons (PLAN-023 Phase 1)
//
// X-macro list of every TheXxx pointer singleton that belongs to one engine instance. With
// RTS_ENGINE_CONTEXT on, each entry becomes a field of rts::EngineContext (Common/EngineContext.h)
// and `TheXxx` becomes a macro that reads it through the current context
// (Common/EngineSingletonMacros.h, generated from this file). With it off this file is unused.
//
//   RTS_ENGINE_SINGLETON(Class, TheName)         -- `Class* TheName`, Class declared with `class`
//   RTS_ENGINE_SINGLETON_STRUCT(Struct, TheName) -- the same for a type declared with `struct`
//
//   RTS_ENGINE_SINGLETON_ZH(Class, TheName)      -- a singleton that exists in Zero Hour only
//
// Every file that includes this one defines all three macros (RTS_ENGINE_SINGLETON_ZH cannot default
// to RTS_ENGINE_SINGLETON: forwarding would macro-expand TheName where the macro header is in scope).
// The list has no preprocessor conditionals (the script rejects them), so rts::EngineContext has the
// same fields in every translation unit, whatever game or library it is compiled for. That matters
// because Common/EngineContext.h is force-included into Core libraries built without RTS_ZEROHOUR as
// well as into both games, and differing layouts would be an ODR violation that reads the wrong
// field. A Zero Hour-only entry is therefore a field in both games; only its `TheXxx` macro is under
// #if RTS_ZEROHOUR (in the generated macro header), because Core compiles into both games.
//
// After editing this list, run
//   python3 scripts/cpp/engine_context_guards.py guard
// to regenerate Common/EngineSingletonMacros.h and wrap the new names' upstream extern
// declarations and definitions in `#if !RTS_ENGINE_CONTEXT`. `... check` verifies both.
//
// Not listed, and so process-global: the critical-section pointers, TheMemoryPoolFactory,
// TheDynamicMemoryAllocator, TheNameKeyGenerator (PLAN-023 Decision 2), TheVersion, the debug
// globals, the constant name and field-parse tables, and the others the script's
// PROCESS_GLOBAL table names with a reason.

// Common: engine core, file systems, INI stores and players
RTS_ENGINE_SINGLETON(ActionManager, TheActionManager)
RTS_ENGINE_SINGLETON(ArchiveFileSystem, TheArchiveFileSystem)
RTS_ENGINE_SINGLETON(AudioManager, TheAudio)
RTS_ENGINE_SINGLETON(BuildAssistant, TheBuildAssistant)
RTS_ENGINE_SINGLETON(CommandList, TheCommandList)
RTS_ENGINE_SINGLETON(DamageFXStore, TheDamageFXStore)
RTS_ENGINE_SINGLETON(FileSystem, TheFileSystem)
RTS_ENGINE_SINGLETON(FramePacer, TheFramePacer)
RTS_ENGINE_SINGLETON(FunctionLexicon, TheFunctionLexicon)
RTS_ENGINE_SINGLETON(GameEngine, TheGameEngine)
RTS_ENGINE_SINGLETON(GameLODManager, TheGameLODManager)
RTS_ENGINE_SINGLETON(GameState, TheGameState)
RTS_ENGINE_SINGLETON(GameStateMap, TheGameStateMap)
RTS_ENGINE_SINGLETON(LocalFileSystem, TheLocalFileSystem)
RTS_ENGINE_SINGLETON(MessageStream, TheMessageStream)
RTS_ENGINE_SINGLETON(ModuleFactory, TheModuleFactory)
RTS_ENGINE_SINGLETON(MultiplayerSettings, TheMultiplayerSettings)
RTS_ENGINE_SINGLETON(PlayerList, ThePlayerList)
RTS_ENGINE_SINGLETON(PlayerTemplateStore, ThePlayerTemplateStore)
RTS_ENGINE_SINGLETON(Radar, TheRadar)
RTS_ENGINE_SINGLETON(RecorderClass, TheRecorder)
RTS_ENGINE_SINGLETON(ScienceStore, TheScienceStore)
RTS_ENGINE_SINGLETON(SpecialPowerStore, TheSpecialPowerStore)
RTS_ENGINE_SINGLETON(StatsCollector, TheStatsCollector)
RTS_ENGINE_SINGLETON(SubsystemInterfaceList, TheSubsystemList)
RTS_ENGINE_SINGLETON(TeamFactory, TheTeamFactory)
RTS_ENGINE_SINGLETON(TerrainTypeCollection, TheTerrainTypes)
RTS_ENGINE_SINGLETON(ThingFactory, TheThingFactory)
RTS_ENGINE_SINGLETON(UpgradeCenter, TheUpgradeCenter)
RTS_ENGINE_SINGLETON(GlobalData, TheWritableGlobalData)

// GameLogic: the simulation
RTS_ENGINE_SINGLETON(AI, TheAI)
RTS_ENGINE_SINGLETON(ArmorStore, TheArmorStore)
RTS_ENGINE_SINGLETON(CaveSystem, TheCaveSystem)
RTS_ENGINE_SINGLETON(CrateSystem, TheCrateSystem)
RTS_ENGINE_SINGLETON(GameLogic, TheGameLogic)
RTS_ENGINE_SINGLETON(GhostObjectManager, TheGhostObjectManager)
RTS_ENGINE_SINGLETON(LocomotorStore, TheLocomotorStore)
RTS_ENGINE_SINGLETON(ObjectCreationListStore, TheObjectCreationListStore)
RTS_ENGINE_SINGLETON(PartitionManager, ThePartitionManager)
RTS_ENGINE_SINGLETON(RankInfoStore, TheRankInfoStore)
RTS_ENGINE_SINGLETON(ScriptActionsInterface, TheScriptActions)
RTS_ENGINE_SINGLETON(ScriptConditionsInterface, TheScriptConditions)
RTS_ENGINE_SINGLETON(ScriptEngine, TheScriptEngine)
RTS_ENGINE_SINGLETON(SidesList, TheSidesList)
RTS_ENGINE_SINGLETON(TerrainLogic, TheTerrainLogic)
RTS_ENGINE_SINGLETON(VictoryConditionsInterface, TheVictoryConditions)
RTS_ENGINE_SINGLETON(WeaponStore, TheWeaponStore)

// GameClient: display, input, UI and client-side stores
RTS_ENGINE_SINGLETON(Anim2DCollection, TheAnim2DCollection)
RTS_ENGINE_SINGLETON(CampaignManager, TheCampaignManager)
RTS_ENGINE_SINGLETON_ZH(SkirmishGameInfo, TheChallengeGameInfo)
RTS_ENGINE_SINGLETON(ChallengeGenerals, TheChallengeGenerals)
RTS_ENGINE_SINGLETON(ControlBar, TheControlBar)
RTS_ENGINE_SINGLETON(CreditsManager, TheCredits)
RTS_ENGINE_SINGLETON(DisconnectMenu, TheDisconnectMenu)
RTS_ENGINE_SINGLETON(Display, TheDisplay)
RTS_ENGINE_SINGLETON(DisplayStringManager, TheDisplayStringManager)
RTS_ENGINE_SINGLETON_STRUCT(DrawGroupInfo, TheDrawGroupInfo)
RTS_ENGINE_SINGLETON(EstablishConnectionsMenu, TheEstablishConnectionsMenu)
RTS_ENGINE_SINGLETON(Eva, TheEva)
RTS_ENGINE_SINGLETON(FontLibrary, TheFontLibrary)
RTS_ENGINE_SINGLETON(FXListStore, TheFXListStore)
RTS_ENGINE_SINGLETON(GameClient, TheGameClient)
RTS_ENGINE_SINGLETON(GameTextInterface, TheGameText)
RTS_ENGINE_SINGLETON(GlobalLanguage, TheGlobalLanguageData)
RTS_ENGINE_SINGLETON(GraphDraw, TheGraphDraw)
RTS_ENGINE_SINGLETON(HeaderTemplateManager, TheHeaderTemplateManager)
RTS_ENGINE_SINGLETON(HotKeyManager, TheHotKeyManager)
RTS_ENGINE_SINGLETON(IMEManagerInterface, TheIMEManager)
RTS_ENGINE_SINGLETON(InGameUI, TheInGameUI)
RTS_ENGINE_SINGLETON(Keyboard, TheKeyboard)
RTS_ENGINE_SINGLETON(LanguageFilter, TheLanguageFilter)
RTS_ENGINE_SINGLETON(LookAtTranslator, TheLookAtTranslator)
RTS_ENGINE_SINGLETON(MapCache, TheMapCache)
RTS_ENGINE_SINGLETON(ImageCollection, TheMappedImageCollection)
RTS_ENGINE_SINGLETON(MetaMap, TheMetaMap)
RTS_ENGINE_SINGLETON(Mouse, TheMouse)
RTS_ENGINE_SINGLETON(ParticleSystemManager, TheParticleSystemManager)
RTS_ENGINE_SINGLETON_STRUCT(RankPoints, TheRankPointValues)
RTS_ENGINE_SINGLETON(RayEffectSystem, TheRayEffects)
RTS_ENGINE_SINGLETON(SelectionTranslator, TheSelectionTranslator)
RTS_ENGINE_SINGLETON(Shell, TheShell)
RTS_ENGINE_SINGLETON(SkirmishGameInfo, TheSkirmishGameInfo)
RTS_ENGINE_SINGLETON(SnowManager, TheSnowManager)
RTS_ENGINE_SINGLETON(View, TheTacticalView)
RTS_ENGINE_SINGLETON(TerrainRoadCollection, TheTerrainRoads)
RTS_ENGINE_SINGLETON(TerrainVisual, TheTerrainVisual)
RTS_ENGINE_SINGLETON(GameWindowTransitionsHandler, TheTransitionHandler)
RTS_ENGINE_SINGLETON(VideoPlayerInterface, TheVideoPlayer)
RTS_ENGINE_SINGLETON(GameWindowManager, TheWindowManager)

// GameNetwork: network, LAN and online state (idle in headless skirmish)
RTS_ENGINE_SINGLETON(DownloadManager, TheDownloadManager)
RTS_ENGINE_SINGLETON(FirewallHelperClass, TheFirewallHelper)
RTS_ENGINE_SINGLETON(GameInfo, TheGameInfo)
RTS_ENGINE_SINGLETON(GameResultsInterface, TheGameResultsQueue)
RTS_ENGINE_SINGLETON(GameSpyBuddyMessageQueueInterface, TheGameSpyBuddyMessageQueue)
RTS_ENGINE_SINGLETON(GameSpyConfigInterface, TheGameSpyConfig)
RTS_ENGINE_SINGLETON(GameSpyStagingRoom, TheGameSpyGame)
RTS_ENGINE_SINGLETON(GameSpyInfoInterface, TheGameSpyInfo)
RTS_ENGINE_SINGLETON(GameSpyPeerMessageQueueInterface, TheGameSpyPeerMessageQueue)
RTS_ENGINE_SINGLETON(GameSpyPSMessageQueueInterface, TheGameSpyPSMessageQueue)
RTS_ENGINE_SINGLETON(LadderList, TheLadderList)
RTS_ENGINE_SINGLETON(LANAPI, TheLAN)
RTS_ENGINE_SINGLETON(LANGameInfo, TheLANGameInfo)
RTS_ENGINE_SINGLETON(NAT, TheNAT)
RTS_ENGINE_SINGLETON(NetworkInterface, TheNetwork)
RTS_ENGINE_SINGLETON_ZH(NGMPGame, TheNGMPGame)
RTS_ENGINE_SINGLETON(PingerInterface, ThePinger)

// GameEngineDevice: W3D objects that register themselves as singletons
RTS_ENGINE_SINGLETON(FlatHeightMapRenderObjClass, TheFlatHeightMap)
RTS_ENGINE_SINGLETON(HeightMapRenderObjClass, TheHeightMap)
RTS_ENGINE_SINGLETON(ProjectedShadowManager, TheProjectedShadowManager)
RTS_ENGINE_SINGLETON(SmudgeManager, TheSmudgeManager)
RTS_ENGINE_SINGLETON(BaseHeightMapRenderObjClass, TheTerrainRenderObject)
RTS_ENGINE_SINGLETON(TerrainTracksRenderObjClassSystem, TheTerrainTracksRenderObjClassSystem)
RTS_ENGINE_SINGLETON(W3DBufferManager, TheW3DBufferManager)
RTS_ENGINE_SINGLETON(W3DFileSystem, TheW3DFileSystem)
RTS_ENGINE_SINGLETON(W3DProjectedShadowManager, TheW3DProjectedShadowManager)
RTS_ENGINE_SINGLETON(W3DShadowManager, TheW3DShadowManager)
RTS_ENGINE_SINGLETON(W3DVolumetricShadowManager, TheW3DVolumetricShadowManager)
RTS_ENGINE_SINGLETON(WaterRenderObjClass, TheWaterRenderObj)
RTS_ENGINE_SINGLETON(WaterTracksRenderSystem, TheWaterTracksRenderSystem)
