# Zero-Fill Dependent Member Initialisation Audit (27/09/2026)

> [!NOTE]
> **AI-Generated Content Disclosure**: This document was written by an AI coding agent.

## Background

`Core/GameEngine/Include/Common/GameMemoryNull.h` and `GameMemory.cpp` make the engine's global
`operator new` / `new[]` zero-fill, and `MemoryPool::allocateBlockImplementation` zero-fills
pooled objects. Upstream notes that many engine types are "not properly zero initialized" and
rely on this. Correctness therefore depends on which allocator built the object. An object gets
garbage in the members its constructor skips when it is built:

- by an embedding host's own plain `new`, or through `W3DNEW`/`W3DNEWARRAY`/`MSGW3DNEW`/`NEW_REF`
  (rlgenerals' `libgeneralsx.so` hides the engine's replaceable `operator new` overloads since
  PR #11, and those four macros still expand to a plain `new` in Null mode, so they reach the
  host's non-zeroing `new`). Since 11ed61c87, `NEW`, `MSGNEW`, `newInstance` and
  `newInstanceDesc` expand in Null mode to a call on the engine's exported
  `(size_t, const char *, int)` overload instead, so a host's `NEW SkirmishGameInfo` is
  zero-filled again, the same as an engine-built object. That overload originally took
  `__FILE__, __LINE__`; since 02/10/2026 it instead passes constant tag arguments
  (`new(static_cast<const char *>(nullptr), 0)`), since the Null-mode overload discards them
  either way (`GameMemoryNull.h`) and the zero-fill routing this audit cares about is unaffected,
- on the stack or in static storage with a non-trivial constructor,
- under a non-zeroing allocator (`LD_PRELOAD=libjemalloc.so.2`).

Rule for fixes: set the member to the value zero-fill produced (0, `FALSE`, `nullptr`), so
retail gameplay, replays and determinism are unchanged.

## Types an embedding host constructs (rlgenerals `b301390`)

Found by grepping rlgenerals for `new` / `NEW` and stack-constructed engine types.

| Type | Where the host builds it | Result |
|---|---|---|
| `SkirmishGameInfo` (+ `GameInfo`, `Snapshot`) | `bindings/engine/skirmish.cpp`, `bindings/env/RlgEnv.cpp` (`NEW`) | **`GameInfo::m_localIP` never set** (fixed) |
| `GameSlot` | stack, same files | All members set by `reset()` |
| `Money` (member of `GameInfo`) | via `GameInfo` | All members set by `init()` / `setStartingCash()` |
| `Version` | `launcher/launcher.cpp` (`NEW`) | All members set |
| `FramePacer` (+ `FrameRateLimit`) | `launcher/launcher.cpp` (`new`) | All members set |
| `Coord3D`, `ICoord3D` | stack | Plain structs; host initialises them |
| `SurfaceClass` | `launcher/window.cpp` (`NEW_REF`) | All members set: its constructor initialises `D3DSurface`, `SurfaceFormat` and `RefCountClass::NumRefs` itself. `NEW_REF` is still a plain host `new` in Null mode (it is not one of the four macros 11ed61c87 routed through the engine), so a future host `NEW_REF` of a W3D type whose constructor relies on zero-fill would not be safe |

`GameInfo::reset()` skips `m_localIP` on purpose (it would clobber the IP `LANGameInfo` sets in
its constructor), so the fix sets it in `GameInfo::GameInfo()` before `reset()`. Derived
constructors (`LANGameInfo`, `GameSpyStagingRoom`, `NGMPGame`) run afterwards and still set their
own value. `ReplayGameInfo` and `SkirmishGameInfo` keep 0, as zero-fill gave them.

## Remaining candidates

Except where a row says **Fixed**, these are only built by engine code (`NEW` / pool), so they still get zero-fill today. They
become live bugs if they are ever built on the stack, by a host, or under a non-zeroing
allocator.

| Type | File | Members no constructor sets |
|---|---|---|
| `NGMPGame` | `GeneralsMD/Code/GameEngine/Include/GameNetwork/GeneralsOnline/NGMPGame.h` | `m_id`, `m_requiresPassword`, `m_allowObservers`, `m_version`, `m_exeCRC`, `m_iniCRC`, `m_isQM`, `m_pingInt`, `m_reportedNumPlayers`, `m_reportedMaxPlayers`, `m_reportedNumObservers`. **Fixed**: `NGMPGame::NGMPGame()` now sets them to `0` / `FALSE` before `cleanUpSlotPointers()` / `enterGame()` / `SyncWithLobby()` (`NGMPGameSlot` already set all its members) |
| `GameSpyStagingRoom` | `Core/GameEngine/Include/GameNetwork/GameSpy/StagingRoomGameInfo.h` | `m_id`, `m_requiresPassword`, `m_allowObservers`, `m_version`, `m_exeCRC`, `m_iniCRC`, `m_isQM`, `m_pingInt`, `m_reportedNum*`. GameSpy is being retired (NGMP policy), so low priority |
| `GameSpyGameInfo` / `GameSpyGameSlot` | `Generals*/Code/GameEngine/Source/GameNetwork/GameSpyGameInfo.cpp` | Several; the file is commented out of both CMake builds, so dead code |
| Engine-wide | `MemoryPoolObject` subclasses | Pool allocation zero-fills; members not set by constructors are not audited. A wider sweep needs a tool (see below) |

## How to extend this audit

- No `clang-tidy` ships in `generalsx/linux-builder`. `cppcoreguidelines-pro-type-member-init`
  on `build/linux64-deploy/compile_commands.json` lists every constructor that leaves a scalar
  member unset; restrict it to the types above before fixing more.
- A runtime check: build with `RTS_GAMEMEMORY_ENABLE=OFF` and a non-zeroing `operator new`
  (or `LD_PRELOAD=libjemalloc.so.2` with `MALLOC_CONF=junk:true`) and run the smoke test /
  a replay comparison (`scripts/qa/smoke/docker-smoke-test-zh.sh`). Any CRC or behaviour
  change points at a zero-fill dependency.
