# The embedding contract

A host can run several engines in one process. Each engine keeps all of its
state in its own `rts::EngineContext`, and the host gives each engine its
services through interfaces GeneralsX declares. GeneralsX depends on nothing in
the host.

This applies to builds with `RTS_ENGINE_CONTEXT` on. With it off, the same
sources build the upstream-shaped game, one engine per process.

## The context

`rts::EngineContext` (`Core/GameEngine/Include/Common/EngineContext.h`) holds
one engine's state:

- every singleton (`Common/EngineSingletons.inl`), as a field;
- the direct fields of `Common/EngineContextFields.inl` and the random seeds;
- the engine's copy of every per-engine static (below);
- the engine's memory manager (below);
- the host's `EmbeddingConfig`, for a context made for an embedded engine.

Code reaches the current context through one `thread_local` pointer,
`rts::t_engine`, read by `rts::ctx()`. An `rts::Scope` makes a context current
for the duration of a call and restores the previous one. Outside every engine
the current context is `rts::g_noEngine`, whose singletons are all null: a
constant in read-only storage, never destroyed, so nothing can write it. A
per-engine static used there is refused.

GeneralsX reaches the host's services through `rts::currentEmbedding`, which
returns the current context's `EmbeddingConfig`. With no embedded engine
current, or in a context made without a config, it throws `std::logic_error`,
so a service used outside its engine is refused rather than guessed.

A resource that must be current on every thread inside its engine, and on no
other, is a thread binding of the context
(`rts::EngineContext::bindThreads`). Every `rts::Scope` that makes the context
current installs the context's bindings on its thread, and removes them when it
restores the previous context, also when it switches a thread from one engine
to another. Anything that makes an engine current on a thread does it through
an `rts::Scope`. An audible engine's OpenAL context is one: it is the thread
context of exactly the threads inside its engine, and the process-wide current
context stays null.

## The accessors

`TheXxx` names a singleton of the current context. Each is a macro,
`#define TheGameLogic (::rts::ctx()->TheGameLogic_)`, generated from
`Common/EngineSingletons.inl` into `Common/EngineSingletonMacros.h`, so no use
site changes. A class static data member that is a context field is reached
through an `rts::ContextField` stand-in.

## Per-engine statics

A static variable that holds state is a per-engine static
(`Common/EngineStatic.h`): each context holds its own copy, made on the first
use in that engine, zero-filled and then initialised as the static was, and
destroyed with the context. Two threads of one engine that reach it at once
make it once. The upstream declaration stays, under `#if !RTS_ENGINE_CONTEXT`,
for the build without the context.

| Upstream declaration | Per-engine declaration |
|---|---|
| a function-local static | `RTS_ENGINE_STATIC(Type, name, initializer)` |
| a file-scope static | `RTS_ENGINE_GLOBAL(Type, name, initializer)`, then `#define name RTS_ENGINE_GLOBAL_REF(name)` |
| a global, declared `extern` elsewhere | `RTS_ENGINE_EXTERN_GLOBAL_DEFINE(Type, name, initializer)`, and `RTS_ENGINE_EXTERN_GLOBAL(Type, name)` with the `#define` where it was declared |
| a static data member | `RTS_ENGINE_CLASS_STATIC(Type, name)` in the class and `RTS_ENGINE_CLASS_STATIC_DEFINE(Type, Class, name, initializer)`, with the `#define` before the class and in the files that use it, `#undef`'d at the end of the header |
| an object that stands in for a static | `constexpr rts::PerEngineStatic<Type> name{}`, used through `name.get()` |

`RTS_ENGINE_GLOBAL_REF(name)` is the call `name_rtsEngineGlobal()`, so a
qualified use (`Class::name`) and a use through an object still name the
static. Where a file also declares a local, a parameter or a member with the
static's name, the `#define` is suspended (`#undef`, then defined again) around
that declaration's function or block. Where one expression uses both, the use
of the static names `RTS_ENGINE_GLOBAL_REF(name)` itself, with no `#define`.

A context destroys its statics newest first, since a static's destructor may
use another. An allocator whose objects other statics may hold, such as an
`AutoPoolClass` pool, is declared with `RTS_ENGINE_STATIC_LAST` and destroyed
after every other static of its engine.

Each per-engine static is identified by the address of a read-only
`rts::EngineStaticKey` declared beside it, with the linkage the original static
had. An inline function's static is therefore still one static in the program,
and a file's static one per file. The key holds no engine state.

A self-registering object (a persist or definition factory, a save-load
subsystem) that upstream made at static initialisation is a per-engine static
made by its library's initialisation in each engine: `WWSaveLoad::Init`
(called by `WWMath::Init`), `WWMath::Init` and `WW3D::Init`.

A global that one of an engine's objects installs itself in is per engine
too, so each engine installs its own: each engine's `W3DFileSystem` sets that
engine's `_TheFileFactory`, as upstream does in its one engine.

Each engine holds its own reference to the D3D8 library: `DX8Wrapper::Init`
loads it and `DX8Wrapper::Shutdown` frees it. The host holds a reference of its
own while any render engine lives, so DXVK stays loaded between engines.

A hook the host installs is per engine as well, so the host installs it in
each engine it makes: the event-pump lock
(`ApplicationWindow_TryLockEventPump` and `ApplicationWindow_UnlockEventPump`),
which an engine with a window takes around its SDL calls. Nothing initialises
once per process, which is what lets engines be made again after a shutdown.

## Name keys

Each engine interns names in a `NameKeyGenerator` of its own, made by
`GameEngine::init` and deleted with the engine, so a name's key depends only on
the order in which that engine met its names. A static that caches a key (a
`NAMEKEY("...")` in a function, a module's `getModuleNameKey`, a window's ID)
is a per-engine static, and so is a flag that guards filling one.

## Constants

Data no engine writes is constant, so it is read-only and shared: tables are
`const`, and the constant vectors and matrices (`Vector3`, `Matrix3x3`,
`Matrix3D`) and bit masks (`BitFlags`) have `constexpr` constructors, and
`GameMakeColor` is `constexpr`, so they are constant-initialised. A function
that only reads a table takes it as `const char * const *`, so the table can
be constant. The empty `StringClass` and `WideStringClass`
buffers are constants that no constructor writes, and the empty strings
`AsciiString::TheEmptyString` and `UnicodeString::TheEmptyString` are
`constexpr` constants. A string constant is a
`constexpr` literal, not a `std::string`, which static initialisation builds
in writable storage, and a pointer to a string literal is itself `const`. An
object that only marks something by its address, such as the grid water's
`WaterHandle`, is a constant too, so every engine compares against the same
address.

C++20's `std::bitset` sets a bit at compile time only below the 64th, so a
constant mask is constant-initialised only while all its bits lie there. A
constant mask with a higher bit (`KINDOFMASK_FS`, the weapon-slot model
conditions) is a per-engine static instead, built on its first use.

The default render states, `W3DRenderState::Defaults` and
`WW3DState::Defaults`, are `constexpr` constants. An engine that does not
render reads them in place of a render state of its own. The vectors, strings,
math types and shaders inside them construct and destroy an empty one at
compile time, so the defaults lie in read-only storage: `.rodata`, or
`.data.rel.ro` where they hold a vtable pointer, which the loader makes
read-only before any code runs. Nothing can write them.

## The memory manager

The engine's memory manager, `TheDynamicMemoryAllocator` and
`TheMemoryPoolFactory`, is per engine: `initMemoryManager` makes it in the
current context and `shutdownMemoryManager` destroys it. With no engine current
there is none. Every engine allocates from the system heap, so a block one
engine's allocator gave can be freed in any context, or none
(`DynamicMemoryAllocator::freeBytes` is static).

GeneralsX's own `operator new` and `operator delete` (plain, array and sized)
zero-fill what they allocate and are hidden in the engine's module. They
forward to the process's operators through versioned symbol references
(`_Znwm@GLIBCXX_3.4` and the others), which the dynamic linker binds, so they
hold no state of their own and reach whatever the process uses: libstdc++, an
allocator it preloads, or a sanitiser's runtime.

An engine's ordinary teardown frees what it allocated. An engine that faulted
is abandoned instead: its state is left as the fault found it, so none of its
destructors run, and its memory stays allocated until the process exits.

## The interfaces

`Core/GameEngine/Include/Common/EmbeddingContract.h` declares what the host
implements, and `rts::EmbeddingConfig`, which names an engine's roots, modes and
services:

| Interface | What the host does |
|---|---|
| `rts::FaultSink` | Latches the engine's fault and unwinds out of the engine's code. |
| `rts::DecisionSink` | Receives the decisions an engine records. Null for an engine that does not record them. |
| `rts::QuitListener` | Is told when the game asks to quit. |
| `rts::ThreadStarter` | Starts every thread the engine runs, with the engine's context current. |
| `rts::ResourceLedger` | Records every operating-system resource the engine acquires, so the host can release it by handle. |

`rts::EmbeddingConfig` holds:

- the asset root and the user data root;
- the render mode (`rts::RenderMode`) and the audio mode (`rts::AudioMode`);
- the host's implementation of each interface above.

The game's own executable is a host too: `GeneralsMD/Code/Main` gives its one
engine an `EmbeddingConfig` whose services keep the upstream game's behaviour.

## The services

### Faults

Every crash path ends in the context's `rts::FaultSink`: the release crash
(`ReleaseCrash`, its localised form and `ReleaseCrashNoReturn`), the assertion
crash, and the catch-alls at the top of a frame (`GameEngine::guardedUpdate`),
of start-up (`GameEngine::init`), of a load (`GameState::loadGame`) and of
teardown (`~GameEngine`), which turn an exception that escapes into a fault.
Each writes the crash report, `ReleaseCrashInfo.txt`, into the user data root,
keeping the one before as `ReleaseCrashInfoPrev.txt`, and then hands the reason
to the sink as raw bytes. The sink does not return.

The engine also marks the fault raised (`rts::faultRaised`, in
`Common/EngineFault.h`), so that a
catch-all that catches the sink's unwind throws it on
(`rts::rethrowIfFaultRaised`), and a second fault keeps the first report.
Once the teardown has begun (`rts::beginEngineTeardown`), a fault cannot unwind
through the destructors running it: the crash path hands the fault to the sink,
the unwind stops where the fault was raised, and the teardown stops at the end
of that step.

A foreign exception, one that is no GeneralsX error, which escapes a frame
becomes a fault whose reason says so: an uncaught exception left the frame,
with the exception's message.

Nothing about faults is process-wide: there is no embedded mode, no process
fault flag and no teardown flag. An assertion's dialog shows only for an engine
that renders to a window.

### Decisions

GeneralsX's decision hooks
(`GeneralsMD/Code/GameEngine/Include/GameLogic/AIDecisionObserver.h`) see each
order an AI player, an AI script, the message stream or a unit's own reaction
issues, and each production or construction cancel a player, a script or an AI
orders. GeneralsX's own cancel-and-refund routine has no hook, so a sale, a
death or a capture records no cancel. Each hook hands its decision to the
context's `rts::DecisionSink::record` as an `rts::Decision`
(`GeneralsMD/Code/GameEngine/Include/GameLogic/Decision.h`), on the logic
thread, inside the engine's frame. The record owns everything it holds, game
things by id and names as strings, so the sink may keep it.

The sink is the only place decisions go: there is no process-wide observer.
An engine whose `EmbeddingConfig` has no sink records nothing, and each hook
is then a null check that reads and allocates nothing, so whether an engine
records its decisions cannot change its simulation. The host owns the sink and
keeps it alive for the engine's life.

No hook reports while the game logic clears a game's data, so a game's teardown
makes no decisions: the next game's begin with its own.

### Games

`rts::startedGames` (`Common/EngineGames.h`) counts the games an engine has
started through `GameLogic::startNewGame`: skirmishes, loads and replays. The
count is per engine, starts at zero and never decreases, and is part of no
checksum, save or replay. A host uses it to drop what it kept for a game a new
one has replaced.

A host that drives every human slot itself sets the context's
`anchoredLocalPlayer`. A game whose info names no local slot then makes the
always-revealed ReplayObserver its local player as its player list is made
(`PlayerList::newGame`), rather than its first human side. Everything
`startNewGame` caches about the local player is then the ReplayObserver's,
live and in playback alike. A skirmish's replay records the local slot its
game info names, -1 for none, rather than slot 0. A game whose info names a
local slot plays as it does without the switch. A save keeps its game info's
local IP, so a load names no local slot again. A save made without the switch,
whose local IP is its human slots' (`GameStateMap::xfer`), is given a local IP
no slot holds as it loads, before its player list is made, so it too loads
with the ReplayObserver as its local player and every human slot alike.

Such a host posts several players' messages on one message stream. The
stream drops a `MSG_DESTROY_SELECTED_GROUP` that the next message makes
redundant (`MessageStream::isRedundantMessage`) only when that next message is
the same player's, so one player's new selection never drops another
player's emptied selection. A stream holding one player's messages, as in
every game GeneralsX plays by itself, is unchanged.

### Queries

A host may query an engine between frames, and a query must not change what
the simulation does next. The pathfinder's own searches leave state behind, so
`Pathfinder::QueryScope` saves what they change and puts it back when it goes
out of scope: the tunnelling flag and the count of cells searched (both part of
the logic checksum), the zone blocks' passable marks, which later searches read,
the obstacle a search ignores, the depth of allies moved aside, and the switch
to fixed pathfinding that exhausting the cell-information pool throws. A host
makes every pathfinding query inside one, through its `queryServices()`. A pool
a query leaves to be cleaned is still cleaned before its next use, as after any
search. A spatial query changes only the partition manager's iteration stamp,
which removes duplicates within one search and is part of no checksum.

The values a host names in its own vocabularies are read from tables in
GeneralsX, so that they cannot drift from the engine: `TheAIStateTypeNames`,
`TheCellShroudStatusNames`, `TheCanMakeTypeNames`, `TheLegalBuildCodeNames`,
`TheCanAttackResultNames` and `TheLocalLegalToBuildOptionNames`, which names
the placement options' bits in bit order.

### Replays and saves

Every skirmish an engine starts records the rolling replay into the replay
folder of its user data root; `RecorderClass::isRecording` says whether that
recording is still open.

- **The mismatch latch.** The context's `playbackMismatchFrame` holds the frame
  of a playback's first logic checksum that disagreed with the live
  simulation, or -1 while none has. Each playback starts with it cleared, and a
  host stops running frames once it is set.
- **The end of a playback.** The replay's last order does not end its game:
  only the replay file is closed, and the game plays on until the host stops it
  at the replay's frame count. A stopped playback posts the game's exit only
  while a game is loaded.
- **A cleared game's checksums.** Clearing a game's data discards the logic
  checksum its last frame posted for the next frame, so a playback started
  next reads the replay's checksums from its own first one.
- **Truncated replays.** A playback reads every order of the replay before its
  game starts, and a replay that ends partway through an order, or holds an
  argument of no known type, is invalid input.
- **The replay's camera.** A playback follows the replay's camera while
  `GlobalData::m_useCameraInReplay` is set. With no player looked at, as for a
  host without the observer control bar, it follows the camera of the player
  who recorded the replay.
- **A host's own replay cameras.** A host that sets the context's
  `replayCameraRoute` records its cameras into a replay itself, as
  `MSG_SET_REPLAY_CAMERA` messages stamped with the player whose camera each
  is, and the game records none of its own (`LookAtXlat`). In a playback,
  every camera message is handed to the route in place of moving the
  tactical view (`GameLogic::onSetReplayCamera`), so that the host can move
  each of its views by the recorded camera it follows.
- **Host map paths.** A map the host names by its absolute path, outside the
  map and save directories, is written into a game's options by that path and
  read back by it, so the replay of a game on it finds it again.
- **The generator in saves.** The `CHUNK_LogicRandom` save block holds the
  logic random generator's state, saved last and restored when a load
  succeeds, so a loaded game resumes with the generator its save had. A save
  without the block restores nothing, and upstream skips the block.
- **Scratch maps.** Each engine's `GameStateMap` tracks the scratch maps its
  loads extracted into the save folder, and deletes only those, through the
  file layer, at its next load and at its teardown. A map whose delete failed,
  or whose existence could not be checked for any reason but "not found", stays
  on the list and is retried. The context's `failingScratchMapDeletes` is a
  test hook that makes that many of the next deletes fail.

A load whose data fails partway takes every update module off the logic's
update lists before it clears the half-loaded game, and leaves no game loaded.

### Test hooks

A host's tests can make an engine fault, or ask to quit, on purpose, through
`Common/EngineTestHooks.h`. `rts::armInjectedFault` arms a fault for the next
time the engine reaches one of its points: start-up, a frame, a load or
teardown. The fault is a fatal error, raised through the real crash path, or a
foreign exception, each optionally inside a catch-all that tries to swallow it;
a swallowed fatal error still faults the engine, since the sink has already
latched it. `rts::armInjectedQuit` makes the engine ask to quit at its next
frame. `rts::armWholeTerrainDraws` makes an engine that draws on demand draw
every tile of its terrain, as it did before it culled them per draw (Drawing
on demand, below), until it is disarmed. The hooks are per engine, and no GeneralsX source arms one: the banned
symbol check's "test hooks" rule refuses it.

### Input errors

The readers of what a host hands an engine throw `rts::InvalidInputData`, with
the file's path and the reason, for data they cannot use: the INI parser for the
game files in the asset root, the map loader, the save reader and the replay
reader when it starts a playback. It is never a fault: the sink is not called,
and the catch-alls let it through. GeneralsX's own files are not host input, so
bad data in them is still a fault. `InvalidInputData` has default visibility,
so a host built with hidden visibility can catch it
(`Common/InvalidInputData.h`).

### Quits

Every request the game makes to quit goes through `GameEngine::setQuitting`.
When the engine starts quitting, it calls the context's
`rts::QuitListener::quitRequested` once; clearing the request and asking again
tells it again. GeneralsX never ends the process to quit: the host decides
what quitting means, and the game executable's listener lets its main loop end.

### Threads

GeneralsX starts every thread through `Common/EngineThread.h`
(`rts::startEngineThread`, and `rts::startEngineLoop` for a thread that polls),
which hands it to the context's `rts::ThreadStarter`. The host creates the
thread and makes the engine's context current on it. The body is given a stop
token that the host, the engine's teardown or the ledger can stop, and the
layer checks it before the body runs and between a loop's steps; a body waits
only in ways that wake for it (`rts::sleepUnlessStopped`). While its body runs,
the thread is recorded in the ledger as `thread <name>`, and releasing that
record stops it and waits for its body to return. The engine's teardown stops
every thread it started before it frees what they use. No GeneralsX source
creates a thread itself.

### Files

Every path GeneralsX forms lies under one of its engine's two roots. A relative
path names a file under the asset root, which is only read; the user data root
is the only place GeneralsX writes, and is `GlobalData::getPath_UserData`. A
write, removal or directory creation anywhere else is refused with `EACCES`.
An absolute path the host hands in, such as a replay to play, is read as given.
GeneralsX never reads or changes the working directory, and its archives are
the `.big` files at the asset root and in its `ZH_Generals` directory.

`Common/EngineFiles.h` is the file layer, the only code that opens files
(`rts::openEngineFile`, `rts::closeEngineFile`, `rts::removeEngineFile`). Each
file open there is recorded in the ledger as `file <path>`, and forgotten when
the layer closes it. A host passes absolute roots, and makes and checks the
user data root before the engine starts. Font faces are read through the file
layer and made from memory, so no font file stays open outside it.

### Audio

The audio mode chooses an engine's audio manager when the engine is made. With
audio off it gets the device-free manager, which opens no device, starts no
thread and makes no call into the audio library, drops every request, and
reports that it cannot play sounds. With audio on it gets the OpenAL manager,
which records its device (`audio device`) and its context (`audio context`) in
the ledger and forgets them when it closes them; a device without
`ALC_EXT_thread_local_context` is refused. The OpenAL context is a thread
binding of the engine (above). An audible device runs OpenAL Soft's own mixer
and event threads, which belong to the device, not to GeneralsX.

### Sockets

`Common/EngineSockets.h` is the socket layer, the only code that opens sockets
(`rts::openEngineSocket`, `rts::acceptEngineSocket`,
`rts::closeEngineSocket`). Each socket is recorded in the ledger as
`socket <what>`, and forgotten when the layer closes it; a socket the layer
did not open is left alone. The engine's own name lookups
(`rts::resolveEngineHostName`) ask the name servers directly over a recorded
socket and return within a short time of a stop request, so a thread waiting
on one stops when asked. The sockets that third-party libraries open for
themselves (the GameSpy SDK, libcurl) are theirs.

### The render device

Only an engine that renders makes a D3D8 device; one with no rendering refuses
to (`std::logic_error`) and records nothing. The device-wrapper layer
(`Core/Libraries/Source/WWVegas/WW3D2/dx8ledger.h`) loads the D3D8 library and
records the engine's reference to it, then records the interface, the device
and every reference it holds on an object made on the device, one record per
reference, each released by the object's own `Release`. It forgets a record
when it releases that reference itself. Releasing the ledger newest first
therefore releases every object, then the device, then the interface, and then
closes the library, each exactly once and without running any of the engine's
code. A host keeps a reference of its own to the library while any of its
engines renders, so an engine closing its own never unloads it under another.

A headless engine (`RenderMode::headless`) and a window engine
(`RenderMode::window`) given the host's window in its context
(`dx8PresentWindow`) draw the same way: into an off-screen render target and
depth surface of the render size, in pinned formats, so their images are the
same. A window engine's device is created on the host's window, with a back
buffer of the render size. Its frames never present;
`DX8Wrapper::Present_Off_Screen_Image` copies the last image into the back
buffer and presents it when the host asks. Its own window handles stay null,
so it makes no SDL call and pumps no events. A window engine with no present
window, such as the game's executable's, keeps upstream's window path.

### Drawing on demand

A host that sets the context's `onDemandDrawing` draws a render engine's
images only when it asks for them (`Common/EngineDrawing.h`,
`rts::drawsOnDemand`, which also needs `m_headless` and `m_headlessRender`).
`GameClient::update` then draws nothing. Instead, after each logic frame,
`GameEngine::update` runs the display's client tick (`Display::tickClient`,
`W3DDisplay::tickClient`), which advances, once and whether or not anything is
drawn, everything a draw used to advance:

- WW3D's clock: the logic frame time, the sync time and the client tick count
  that the ping-pong particle buffers follow (`WW3D::Count_Client_Tick`);
- the cameras, through `updateViews`, without the drawable loop that each
  view's update ran;
- every drawable's draw modules and icons, once, whichever views can see it,
  and its shroud-clear frame for the watcher and for every slot's player,
  for a drawable that renders through the scene's drawable pass alone
  (`DrawModule::rendersWithDrawableInfo`), as GeneralsX notes it only as it
  renders one (`RTS3DScene::renderOneObject`), so that a projectile stream, a
  laser or a rope, which has none, is fully obscured as soon as it is fogged;
- the particle simulations (below), the field-particle count they read, and
  the scene's per-frame updates: lights, W3D emitters, model hierarchies,
  spheres and rings, and the registration of meshes for rendering;
- the texture mappers, the projected and volumetric shadows' caches, the
  fade of every terrain shroud over the whole map (the terrain's own and each
  player's; Fog fade, below), the trees' sway and toppling, the terrain and
  water tracks, the dynamic LOD and the cinematic text's countdown.

A draw (`W3DDisplay::draw`) then renders the state the tick left. It freezes
WW3D's frame times while it renders, and steps nothing, so two draws at one
frame are the same and an image does not depend on which views were drawn
before, at that frame or at earlier ones. A draw also leaves out what made it
depend on the draws before it: the camera-locked drawable is drawn with the
rest of the scene, not hidden and drawn again alone; terrain tracks are
flushed from the tracks that are visible now; shadow volumes are rebuilt
exactly every time; and the terrain shader resets the sampler states it set.
No cursor pick is made.

The tick steps every model's animation clock, but poses a model's hierarchy
only when something needs the pose (`Animatable3DObjClass::Advance_Client_Tick`).
For a top-level model in a single, timed animation, with no embedded sounds and
no posed sub-objects, the tick steps the frame and leaves the pose pending
(`Is_Pose_Pending`). The first read of a bone (`Get_Bone_Transform`) or of a
sub-object's transform, or a render, poses it at the frame the tick stepped to,
without stepping again. A real change to the model, such as a new transform, a
new animation or a captured bone (`Set_Hierarchy_Valid(false)`), drops the
pending pose, so the next read steps and poses as it did when the tick posed
every model. The tick's bounding-volume pass also skips a model whose pose is
pending, since its sphere would pose it. A pose depends only on its frame, so
it is the pose the tick would have posed, and a model that no view draws and
nothing reads is never posed. In a busy game of eight computer players this
takes the scene's per-frame step from about 1 ms to 0.3 ms.

Such an engine simulates particles though it is headless. The manager is the
watcher's simulation. When a game starts, `beginSimulations` gives each
slot's player a simulation of its own (`newSimulation`), which copies the
particle systems made in the watcher's, with the same ID, and every change
made to them. Each simulation tests shroud for its own player and draws from
its own client random stream, so its particles are those GeneralsX shows the
player whose view it is, and no player's emission shifts another's. The tick
updates every simulation, and a draw renders the one that
`ParticleSimulationScope` makes current.

GeneralsX makes some effects only where its observed or local player can see:
an FX list's (`FXList::doFXPos` needs the place clear, `doFXObj` the object no
worse than partly clear), a stealth detector's ping, a Spectre gunship's
strafing smoke, a laser's flares and a Chinook's rotor wash. The logic still
makes them for the anchor, which sees everything, so the watcher's simulation
gets every one. Around each such test an `EffectViewersScope` asks the same
question of every player whose simulation exists, and a system made inside it
is copied only into the simulations of the players who pass (scopes nest, so a
player must pass every test around the making). A player left out makes
nothing and draws nothing from its client random stream, as GeneralsX would
not had that player been its local player. The placement picks of an FX
list's particle nugget come from the client stream of the frame, which the
test does not move. The tests read the players' cell shroud and objects'
shrouded status, which change no logic in these engines (they have no ghost
objects). With no simulations (an engine with no rendering) nothing is asked.

The FX list's other effects read the same players (`effectViewers`, every
player outside any scope):

- **Scorch marks.** Every player with a view has scorch marks of its own,
  made and deleted with its terrain shroud (`BaseHeightMapRenderObjClass::
  addPlayerShroud`). A scorch goes to the terrain's own marks, which are the
  watcher's, and to those of the players who could see it made. A draw for a
  player draws its own. Each player's marks keep GeneralsX's limit and
  duplicate rule on their own.
- **Light pulses.** Each light pulse's light keeps the players who could see it
  made. A draw for any other player turns the light off for that draw alone
  (`RTS3DScene::drawLightsSeenBy`), and the client tick fades it as before.
- **Tracers and ray effects.** A drawable made inside a scope keeps the
  players who could see it made (`Drawable::getEffectViewers`, saved with the
  drawables by the game client's block, version 4). A draw for any other
  player hides it, the tracer's line included.
- **View shakes.** A view shake shakes the tactical view and then each player
  view of a player who could see it made, in the direction the tactical
  view's shake drew (`Display::shakePlayerViews`). That draws no further
  random number, so no player's shake moves the frame's client stream.

Effects made outside any FX list, such as the scorch marks of the particle
uplink cannon, a firestorm or a nuke, go to every player, as GeneralsX makes
them whatever its local player can see. The particle uplink cannon's own test
is not per player: it takes its beam's effects away while its local player
cannot see the cannon and puts them back once it can, and for the anchor it
never takes them away.

A save holds the watcher's particle systems and then, since version 2 of the
manager's block, every player's simulation as it is, with its own systems,
particles and ID counter (`xferSimulations`); a load reads each into its own
simulation, where the load's post-processing links each system to the
masters, slaves and controlling particles of that simulation. A save from
before version 2 holds the watcher's alone, and a load copies each of them,
with its ID and its particles, into every player's simulation
(`copyLoadedSystemsToSimulations`). Neither changes a client random stream,
and each restored system of the watcher's that is neither a slave nor driven
by a particle then copies its changes, as a system made outside an update
does. A reset lets go of the textures the last draw left on each
simulation's point group and streaks, so none outlives the render device.

An engine with no rendering runs `ParticleSystemManagerDummy`, which makes no
particle system, so the logic must not depend on whether one exists. Where game
logic makes a particle system and then places it with a random pick (the random
bone of a damage state's system in `TransitionDamageFX`, the place, height and
delay of an EMP's sparks in `EMPUpdate`, the place of a special ability's
disable effect in `SpecialAbilityUpdate`), an engine that draws on demand picks
from the client random stream (`rts::placesParticlesWithClientRandom`), so its
logic random generator, and the game, are those of an engine with no rendering,
and replays cross between the two. GeneralsX's own play keeps the logic random
pick.

A playback that follows the replay's camera shows the 3D camera the replay
holds as it is, since the tick builds no camera over it before a draw. An
engine with no rendering builds no 3D camera, so the replay camera it records
holds none (`LookAtXlat`). Playback leaves out a recorded 3D camera at the
origin too, as such an engine recorded before, and builds the camera from the
pivot, angle, pitch and zoom (`GameLogic::onSetReplayCamera`).

A window engine draws the same way, and `Present_Off_Screen_Image` presents
the last image drawn, whichever view it was drawn for.

### Drawing as a player

`Display::drawPlayerView` draws the game as one player sees it, or as the
game's local player sees it for a player index of -1. For the draw it makes
the player the context's `viewingPlayer`, which
`rts::getObservedOrLocalPlayer` returns ahead of the control bar's observed
player, so the draw's shroud tests and tints are that player's. It points the
ghost objects at the player, refreshes the radar's objects, draws the
player's particle simulation (`ParticleSimulationScope`), and gives every
drawable the looks it has in the player's view (`Drawable::beginViewLook`):
its stealth look, worked out for the player as `StealthUpdate` works it out
for the observed or local player, and whether the player's shroud obscures it
fully (`Drawable::isFullyObscuredByShroudFor`), with each draw module hiding
the shadows of what is hidden (`DrawModule::beginViewLook`). These looks change
only what the draw shows: no deselection, model change, shadow release, track
cap or particle start or stop. When the draw ends, all of it is put back as it
was, so no logic ever runs with a viewing player set and the next draw starts
from the same state. None of it has `rts::changeLocalPlayer`'s or
`rts::changeObservedPlayer`'s other effects.

The team colours a disguise or a garrison shows, and a garrison's garrisoned
model, are each player's own too (Per-player looks, below). The terrain's
shroud is the player's own (Per-player shrouds, below).

`Radar::copyToRGBForPlayer` reads the radar as a player sees it, in the same
way: for the read, the player is the viewing player, so its blips are the ones
`W3DRadar::canRenderObject` lets that player see (a stealthed enemy by the
player's own relationship to it), and the radar's shroud is filled from the
player's shroud (`PartitionManager::refreshRadarShroudForPlayer`). The blips
whose colour depends on who sees them, disguised units and containers such as
garrisons, are coloured for the player (`Radar::recolorViewerDependentObjects`).
Afterwards the radar's shroud and colours are the observed or local player's
again. A radar read asks for no shroud status the client tick did not already
ask for.

### Per-player looks

GeneralsX works out some looks for its local player alone and bakes them into
a model's render objects: the team colour of a disguised unit
(`StealthUpdate::changeVisualDisguise`: its enemies that are still playing see
the colours of the team it is disguised as, its own side and everyone else its
true colours), and a garrison's team colour and garrisoned model
(`GarrisonContain::recalcApparentControllingPlayer`: a garrison of stealthy,
undetected units shows its original team's colours and ungarrisoned model to
everyone but its own side and allies). Changing such a look rebuilds the model,
which restarts its animation and its particle systems, so a draw cannot change
it per player. Built with the engine context, the same code records, when
it sets the look, the look GeneralsX would set for every player were it the
local player (`Drawable::setPlayerLook`); `Drawable::setIndicatorColor`
forgets them, since the drawable then looks the same to everyone. In the client
tick of an engine that draws on demand, a model draw whose drawable some player
sees with another look than its own keeps a variant of itself with that look
(`W3DModelDraw::syncViewLookVariant`): a second module of the same class, made
as the drawable made the first, with its own model, which every call the first
module gets is passed on to first, with the client random stream kept as it
was, so that its model condition, animation, sub-objects, turret, recoil,
shadow and terrain decal follow the first's. A variant makes no particle
systems, track marks or weapon effects, and cannot be picked. Its model is
hidden except in a draw of the view of a player who sees its look, in which it
is drawn, and the first module's model, shadow and decal hidden, for the length
of the draw (`W3DModelDraw::beginViewLook`). The watcher's view is the local
player's, the drawable's own look. A cave's colour (`CaveContain`) needs none
of this: its look code asks `OpenContain::getApparentControllingPlayer`, which
always answers no player, so a cave shows its team's colours to everyone.

A fake structure's terrain decal (`KINDOF_FS_FAKE`, the template's shadow
texture) is another such look: GeneralsX gives the drawable the decal when its
local player is the structure's owner, an ally or neutral to it (an observer),
and none for an enemy, deciding it when the drawable is bound to its object,
when the object changes team, and in `GameClient::updateFakeDrawables` (a
player's defeat, a change of local player). Built with the engine context, the
same places also record which players GeneralsX would not show the decal to
were each the local player (`Drawable::recordFakeStructureDecalViewers`), and a
load records it again, since a save keeps only the local player's decal. The
local player's decal stays on the drawable; a draw of the view of a player
who would not see it hides it for the length of the draw
(`Drawable::hidesTerrainDecalInViewLook`, read by
`W3DModelDraw::beginViewLook`).

A projectile stream's lines (`W3DProjectileStreamDraw`: the Dragon Tank's
flames, the Toxin Tractor's toxin, the Ambulance's cleaning spray) are taken
out of the scene while the stream is fully obscured for GeneralsX's local
player (`setFullyObscuredByShroud`); its stealth or a hidden drawable does not
matter. They stay in the scene in an engine that draws on demand, and a draw of
a view in which the stream is fully obscured hides them, and only them, for the
length of the draw (`W3DProjectileStreamDraw::beginViewLook`).

### Per-player views

A host that sets the context's `playerViews` keeps a view of its own for every
slot's player. Each game's start, live, loaded or played back, replaces the
last game's views (`GameLogic::startNewGame`) with a view per slot player,
made as the tactical view is (`InGameUI::createPlayerView`): a `W3DView` in an
engine that renders and a `ViewDummy` stand-in otherwise. Each is set up as
the tactical view is at a game's start, but looking at its own player's start
position. The display keeps them (`Display::addPlayerView`,
`Display::findPlayerView`), and updates and steps them with the tactical view,
so every camera follows and settles by logic-frame time whether or not it is
drawn. The tactical view stays the view of the game's local player: the
watcher's, for a host that also anchors its local player.
`Display::drawPlayerView` draws a player's own view: for the draw, it makes
that view the tactical view and the display's only view.

A draw of an engine that draws on demand sets up the drawn view's own state
before it renders (`W3DView::prepareForDrawOnDemand`): it builds the view's
camera from its pose if the pose changed since the client tick built it, so a
camera aimed between two frames shows its aim at once, culls the terrain's
bridges, trees and props for that camera, and drops the props' cached shroud,
so they are tested again for the drawn view's player. Each draw loads the trees
that camera sees into the tree buffers from the trees as they are
(`W3DTreeBuffer::drawTrees`), rather than keeping what earlier draws' cameras
saw, which left a pushed-aside tree out of date. Such an engine draws the
terrain whole (`W3DView::drawsEntireTerrain`), as GlobalData's
`m_drawEntireTerrain` asks for: the terrain and its relighting then belong to
no camera, and no camera's far plane depends on where another view left the
terrain. Its extra blend tiles get room for all those a draw draws at once
(`HeightMapRenderObjClass::renderExtraBlendTiles`, which counts them first),
rather than 16 more after each draw that ran out of room.

A draw of the whole terrain still draws only what its own camera can see
(`HeightMapRenderObjClass::cullTiles`). The terrain's vertex buffer tiles, of
32 by 32 cells, cover the whole map and never move; each keeps a box around
its vertices, worked out again only after `updateBlock` rewrote them. A draw
tests every tile's box against the frustum of the camera it draws with (the
water's mirror pass with its reflected camera) and skips the tiles wholly
outside it, with the extra blend tiles and the shoreline tiles of their cells.
A tile wholly outside one of the frustum's planes holds no triangle that could
cover a pixel, so the image is the same as a draw of every tile, and what a
draw draws depends on its camera, the frame and its player's shroud alone. The
terrain's frame update no longer relights the tiles under the dynamic lights
itself: it adds the quads the lights' current and previous regions cover to
each tile's rectangle of quads to relight, and the draw relights, from the
static lighting and that frame update's lights, the rectangles of the tiles it
draws (`addQuadsToRelight`, `relightTiles`), so a draw pays only for the light
it can see and a tile drawn later shows the light it would have shown had
every draw relit it. The draw runs the terrain's frame update once, before
reflective water's mirror pass renders the scene, and the mirror pass relights
the tiles it draws as the main pass does, so a reflection shows the draw's own
lights, not those of an earlier draw of another view. Scorch marks keep one buffer for the map, rebuilt only
when a scorch is added and drawn with one call, so they are not culled. A
host's tests can turn the culling off (`rts::armWholeTerrainDraws`,
`Common/EngineTestHooks.h`) to compare a culled draw with a draw of every
tile; nothing in GeneralsX turns it off.

### Per-player shrouds

In an engine that renders, the same game starts give every slot's player a
terrain shroud of its own (`Display::addPlayerShroud`,
`BaseHeightMapRenderObjClass::addPlayerShroud`), beside the terrain's own
shroud, which stays the local player's. Each is made for the map as the
terrain's own is, with the same border, and the partition manager fills it
from its player's shroud (`PartitionManager::refreshPlayerShrouds`), which it
does again whenever it refreshes the local player's: after a load, a change of
the map's active boundary or a device reset. Every change of a cell's shroud
is written to the player's own shroud as it is made
(`Display::setPlayerShroudLevel`). `Display::drawPlayerView` draws the terrain,
water and trees with the drawn player's own shroud
(`BaseHeightMapRenderObjClass::setDrawnShroud`, which `getShroud` follows), and
with the terrain's own again when the draw ends. A device reset releases and
reacquires them all, and `BaseHeightMapRenderObjClass::reset` deletes them with
the map. An engine with no rendering has no terrain shroud, so it makes none.

### Fog fade

GeneralsX compiles its fog fade out (`DO_FOG_INTERPOLATION` is never
defined), so its fog changes at once. An engine that draws on demand fades
its fog anyway, at the same rate the fade has (255 levels a second, on
WW3D's sync time, so about 30 logic frames from shroud to clear): a cell's
new level is the target its terrain shroud fades towards
(`W3DShroud::setShroudLevel`), and the client tick moves every cell of every
terrain shroud one step towards its target (`W3DShroud::advanceFade`), so a
draw only uploads the faded levels and the fade does not depend on which
frames were drawn. The levels a shroud is given before the first tick after
it is made for a map (a new game, a load or a playback) are its seed, and show
at once rather than fading in from black. A device reset keeps the fade's
levels (`W3DShroud::releaseKeepingFade`), so a fade goes on over the reset as
if there had been none. Every other engine shows fog as GeneralsX does.

### Script cameras

A host whose cameras are its own sets the context's `scriptsMoveNoCamera`.
A map script's camera actions (moves, pans, zooms, rotations, pitches,
shakes, follows, tethers, the scripted camera's modifiers and the default
view; `ScriptActions::executeAction`) then move no camera. A script's other
actions run as before.

### The clock

Logic code reads no clock. Every seeding names its seed: an engine starts on
seed 0, and each game reseeds from its set-up. What waits in real time beside
the simulation, such as the load screen's pacing and a network game's load
timeout, measures it on a steady clock outside the logic code, and none of it
reaches the simulation.

### The C library

GeneralsX calls no C library function that keeps process-wide state: none with
a static result buffer or position (`strtok`, `localtime`, `gethostbyname`
and the like), no hidden random generator, nothing that changes the
environment or signal handling, and nothing that ends the process or runs at
its exit. It calls the reentrant forms instead, or keeps the state per engine
(`Common/EngineCLibrary.h`): `rts::engineRand` and `rts::engineSrand` give each
engine the C library's own generator, seeded as a fresh process's is, so an
engine draws the sequence a single-engine process would; `rts::engineStrtok`
keeps a token position per engine; and `rts::lookUpHostAddresses` returns the
caller's own list. A bad command line is `rts::InvalidInputData`, not an exit.
The environment, signals and exit belong to the host. The one piece of C
library state engines still share is the time-zone cache, which every engine
computes the same from an environment none of them changes.

### The resource ledger

The host's `rts::ResourceLedger` holds a record of every operating-system
resource an engine holds, each with the raw handle and the release that frees
it, so that a host can release them all after a fault without running any of
the engine's code. The layers that acquire resources record them as they do,
and forget a record when they release the resource themselves:

| Kind | Recorded by |
|---|---|
| `file <path>` | the file layer |
| `thread <name>` | the thread layer |
| `audio device`, `audio context` | the OpenAL audio manager |
| `socket <what>` | the socket layer |
| `D3D8 library`, `D3D8 interface`, `D3D8 device`, and one kind per D3D8 object (`D3D8 texture`, `D3D8 surface`, ...) | the device-wrapper layer |

A ledger may be called from any of the engine's threads, and must not hold a
lock of its own while it runs a release, since a release can wait for a thread
that forgets its record.

## What may stay process-wide

Only compile-time constants may stay process-wide: the constant tables,
strings, masks and colours, the default render states, the empty strings and
the no-engine context (above). They lie in read-only storage, so no engine can
write them, and every engine reads the same values.

Writable storage outside every context holds only:

- the current-context pointer, `rts::t_engine`, one per thread;
- third-party libraries' own state, which GeneralsX does not write: gli's
  lookup tables, stb_image_write's settings and tables, and libstdc++'s wait
  pool for `std::atomic::wait`.

Nothing else is shared between engines, and nothing initialises once per
process. A host can check this on the built library: every symbol in writable
static storage (`.data`, `.bss`, `.tdata`, `.tbss`) must be one of those
above.
