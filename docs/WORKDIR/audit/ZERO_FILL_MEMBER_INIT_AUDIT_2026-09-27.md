# Zero-Fill Dependent Member Initialisation Audit (27/09/2026)

> [!NOTE]
> **AI-Generated Content Disclosure**: This document was written by an AI coding agent.

## Background

`Core/GameEngine/Include/Common/GameMemoryNull.h` and `GameMemory.cpp` make the engine's global
`operator new` / `new[]` zero-fill, and `MemoryPool::allocateBlockImplementation` zero-fills
pooled objects. Upstream notes that many engine types are "not properly zero initialized" and
rely on this. Correctness therefore depends on which allocator built the object. An object gets
garbage in the members its constructor skips when it is built:

- by an embedding host's own `operator new` (rlgenerals' `libgeneralsx.so` hides the engine's
  overloads since PR #11, so `NEW SkirmishGameInfo` in rlgenerals uses the host's `new`),
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

`GameInfo::reset()` skips `m_localIP` on purpose (it would clobber the IP `LANGameInfo` sets in
its constructor), so the fix sets it in `GameInfo::GameInfo()` before `reset()`. Derived
constructors (`LANGameInfo`, `GameSpyStagingRoom`, `NGMPGame`) run afterwards and still set their
own value. `ReplayGameInfo` and `SkirmishGameInfo` keep 0, as zero-fill gave them.

## Remaining candidates (not fixed)

These are only built by engine code (`NEW` / pool), so they still get zero-fill today. They
become live bugs if they are ever built on the stack, by a host, or under a non-zeroing
allocator.

| Type | File | Members no constructor sets |
|---|---|---|
| `NGMPGame` | `GeneralsMD/Code/GameEngine/Include/GameNetwork/GeneralsOnline/NGMPGame.h` | `m_id`, `m_requiresPassword`, `m_allowObservers`, `m_version`, `m_exeCRC`, `m_iniCRC`, `m_isQM`, `m_pingInt`, `m_reportedNumPlayers`, `m_reportedMaxPlayers`, `m_reportedNumObservers` (only callers of the setters fill them) |
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
