# PLAN-023: Multiple Engine Instances in One Process

**Status**: Phase 0 done (see [Phase 0](#phase-0-prerequisites-that-are-bugs-today)); Phase 1's mechanism done behind `RTS_ENGINE_CONTEXT` (see [Phase 1](#phase-1-enginecontext-core)); the rest of Phase 1 and Phases 2-4 in delivery
**Scope**: Zero Hour first (GeneralsMD + Core), then the Generals backport
**Consumer**: rlgenerals (embeds `libgeneralsx.so`, wants N engines per process)
**Baseline**: `main` @ `7ea7bf8ef`. The investigation's line references were checked against `f9c894410` and may have drifted; re-check them before editing.

## Delivery scope

This plan is being delivered as a stack of PRs (each paired with the rlgenerals PR that bumps the submodule to it). The stack stops at **several engines alive in one process and stepped alternately on ONE thread**, each bit-identical to its solo run. Everything that only matters once engines run on different threads at the same time is deferred:

- dropping the global string locks, and the atomic `AsciiString`/`UnicodeString` refcount;
- the `calculateZones` stack frame (it only matters for small secondary-thread stacks);
- `checkfortransitionsnum` as `thread_local` (on one thread the depth count is already correct);
- the TSan work in Phase 7;
- the thread-migration, GIL-release and vector-env work on the consumer side.

## Consumer answers

rlgenerals' answers to the [open questions](#open-questions) (from its `docs/planning/MULTI_ENGINE_CONSUMER.md`):

1. **Thread migration: not needed.** One engine stays on one thread for its whole life. `Scope` stays per call, so migration remains possible, but it is not tested first.
2. **Render mode: N headless engines plus at most one rendering engine per process is enough.** Phase 8 is off the critical path.
3. **Create/destroy: create N at the start, `reset()` in place, destroy only on close or after a fault.** Create, destroy, create must work (tests and fault recovery), but it need not be fast.
4. **Shared INI/asset data: not a goal now.** Revisit after the consumer's threaded benchmarks.
5. **Upstream shape: fork-only behind `RTS_ENGINE_CONTEXT` is fine.**

## Goal

Run N independent engines inside one process without dlopen()ing a renamed copy of the engine library per instance. Every engine must stay bit-identical to a solo run of the same seed and actions.

## Summary

It is feasible, and most of the work is mechanical. Most of the risk is in a few places:

1. **Mechanism.** Add one `EngineContext` struct and a single 8-byte `thread_local` (initial-exec) "current context" pointer. The ~150 `TheXxx` singletons become macros that resolve through it (`#define TheGameLogic (::rts::ctx()->TheGameLogic_)`). This was compile-tested over all 956 Zero Hour + Core translation units: 18k use sites and the consumer's 326 uses need **no edits**, and only 5 sites needed hand fixes. Access costs the same as today's GOT-relative global access (about 0.3 ns).
2. **About 1,250 cached `NameKeyType` statics** (`getModuleNameKey()::nk`, `static NameKeyType k = NAMEKEY("…")`, `TheKey_*`, GUI IDs). These are handled by making `TheNameKeyGenerator` **process-wide, immortal and thread-safe**, with the first engine booting alone ("priming"). Static analysis shows key *values* never reach simulation or CRC, so none of the cache sites change. An empirical perturbation test gates this decision (see [Decision 2](#decision-2-namekeys-shared-immortal-generator-with-priming)).
3. **About 25 statics hold live simulation state** and must move into their owners. Examples: the pathfinder cell-info pool, the polygon-trigger list, the partition-manager scratch, the RNG seeds and the map-object list. Some of them are hard blockers even for two engines that are merely *alive* at once.
4. **Platform bugs** that a single image exposes and the dlopen copies were hiding. Examples: the `CRITICAL_SECTION` emulation leaking locks, libc `strtok` in the INI parser, the process-global OpenAL current context, and `chdir()` at teardown.
5. **Render mode** (DX8Wrapper/WW3D are all-static, and SDL has one event queue) is a separate, larger phase that can be deferred. Headless multi-instance with at most one rendering engine per process comes first.

Rough size, in engineer-days of focused work:

| Scope | Estimate |
|---|---|
| Headless multi-instance, Zero Hour | about 40-60 |
| Generals backport | about +10-15 |
| Validation harness and CI gates (Phase 7) | about +8-12 |
| Linux tools entering a context (WorldBuilder, GUIEdit, MapCacheBuilder, …) | about +3-5 |
| Render-mode multi-instance | about +25-45 |

Phases 2-4 edit upstream code, unlike Phase 1's guard-only edits, so their weekly-sync conflict cost is **not yet measured**. Before starting Phase 2, replay the last few months of upstream merges against a branch carrying the Phase 2-4 edits and count the conflicts. Keep the edits minimal and local (move the static into its owner and keep the accessor names) so that conflicts stay trivial.

## Why the dlopen copies work, and what they hide

Each dlopen'd copy of the library gets its own `.data`/`.bss`, so every singleton and static is per-copy for free. The spike (rlgenerals, 2026-09-24) ran 32 engines per process deterministically, but it only worked after three fixes:

- `-fno-gnu-unique`: 643 `STB_GNU_UNIQUE` statics are otherwise bound process-wide across copies.
- A thread-local `strtok`: libc is not copied.
- The engine had to be created and stepped on the same thread, because of the `CRITICAL_SECTION` bug below.

Anything that lives *outside* the copied image is still shared, even with copies: libc (`strtok`, cwd, env, malloc), `libSDL3.so`, DXVK and the user-data directory. With a single image, everything below becomes shared as well.

The mutable state in `libgeneralsx.so`, from `nm` over the engine archives plus the current `.so`:

| Kind | Count |
|---|---|
| Unique mutable symbols | 3,684 |
| `The*` singletons (plus `TheKey_*` and name tables) | 313 |
| Class statics | 448 |
| File or global statics | 1,380 |
| Function-local statics | 964 |
| Guard variables | 579 |

Most of these are constant tables that sit in `.data` only because they hold pointers, or render/GUI-only state. The per-instance mutable subset is a few hundred. The "about 25 simulation statics" in Phase 2 comes from subsystem surveys plus an `nm` cross-check, not from a complete classification. That is why the `nm` classification is a Phase 1 deliverable that drives Phases 2-4.

## Target model

- **Engine ↔ thread.** Normally one engine per OS thread. The design must *not* depend on that, so engines can later be stepped from a pool (EnvPool style). The current context is therefore set per call by an RAII `Scope`, not bound to a thread.
- **Determinism.** Engine *k* in an N-engine process must match a solo run on the main thread, per frame, on `TheGameLogic->getCRC(CRC_RECALC)` and on the observations.
- **Headless first.** `-headless`, plus optionally *one* engine per process with `m_headlessRender`.
- **Same data.** All engines in a process load the same install dir and INI/mod data (this is required by [Decision 2](#decision-2-namekeys-shared-immortal-generator-with-priming)).

## Decision 1: Mechanism: `EngineContext` + initial-exec `thread_local` pointer + macros

| Option | Verdict |
|---|---|
| **(b) Context struct + TLS current pointer + `The*` macros** | **Chosen.** Call sites are untouched and upstream merges stay cheap (see below). Per-access cost is about equal to today's global. This is the V8 `Isolate` / SQLite `SQLITE_OMIT_WSD` pattern. |
| (a) `thread_local` on every static | Rejected. State would be per *thread*, not per engine, so helper threads see empty singletons and engines can never migrate between threads. The statics total about 400 KB, which exceeds the 1,664 B static-TLS surplus of a dlopen'd `.so`, so general-dynamic TLS would be forced: `__tls_get_addr` costs about 1.2-1.8 ns per access, and GCC does not hoist it. It also does nothing for destroy/recreate. |
| (c) Pass the context explicitly | Rejected as a first step: about 21.7k sites, and every weekly upstream merge would conflict. It remains a possible long-term target for hot paths. |
| (d) `dlmopen` namespaces | Rejected: 15 namespaces at most (about 3 with default glibc tunables), not available on macOS, and worse than the existing spike. |
| (e) Process pool + shared memory | Needs no engine changes, so it stays the fallback and the crash-isolation layer (K processes × M engine threads). |

Measured on this machine (glibc 2.43, x86-64):

- An 8-byte `initial-exec` pointer loads fine via `dlopen` from both C and Python.
- The static-TLS limit is 1,664 B. Anything larger fails with "cannot allocate memory in static TLS block".
- Per-access cost over the call baseline:

  | Access | Cost |
  |---|---|
  | Plain GOT global | +0.37 ns |
  | Initial-exec pointer, then member load | +0.34 ns |
  | General-dynamic `thread_local` | +1.15 to 2.0 ns |

- Declare the pointer `extern constinit thread_local`, or build with `-fno-extern-tls-init`. Otherwise every cross-TU access pays a `_ZTH` init-wrapper check.
- On macOS only the TLV model exists. There is no initial-exec equivalent, so `tls_model` must be `#if __ELF__`. The cost there is an estimated 1-3 ns and was not measured.

Merge friendliness was measured over 265 upstream commits (06-08 → 09-20). They touched 0 singleton `extern` lines and 0 `initSubsystem` lines, while 1,262 changed lines contain `The*` uses. Macros leave all of those untouched. Guarding the declarations with `#if !RTS_ENGINE_CONTEXT` only *adds* lines, so merges stay clean.

Sketch (a prototype passed 8 threads doing create/step/destroy/create, and an engine migrating between threads, under TSan and ASan):

```cpp
namespace rts {
struct EngineContext {
#define RTS_ENGINE_SINGLETON(T, n) T* n##_ = nullptr;
#include "Common/EngineSingletons.inl"      // X-macro list, ~134 entries, #if RTS_ZEROHOUR where needed
#undef RTS_ENGINE_SINGLETON
    UnsignedInt gameLogicSeed[6], gameClientSeed[6], gameAudioSeed[6], gameLogicBaseSeed; // hot fields
    // pathfinder pool, polygon triggers, map objects, ... (Phase 2/3)
    std::vector<void*> slots;                 // PER_ENGINE_STATIC storage, destroyed in reverse order
};
extern EngineContext g_noEngine;             // all-null: static init, foreign threads
extern constinit thread_local EngineContext* t_engine RTS_TLS_IE;
inline EngineContext* ctx() { return t_engine; }
struct Scope {                                // also sets FPU mode, C locale, audio thread context
    EngineContext* prev;
    explicit Scope(EngineContext* c) : prev(t_engine) { t_engine = c; }
    ~Scope() { t_engine = prev; }
};
}
#define TheGameLogic (::rts::ctx()->TheGameLogic_)   // generated for every entry
```

The build switch is a CMake option, `RTS_ENGINE_CONTEXT`: OFF by default (upstream-shaped), ON for the embedding library. It requires C++20, so VC6 stays on the legacy path.

- **Same data is enforced, not assumed.** `BootConfig` carries an install/mod fingerprint: a hash of the `.big` archive list, sizes and mtimes, plus loose `Data\INI` overrides. Any engine whose fingerprint differs from the priming engine's is refused before `GameEngine::init`.

## Decision 2: NameKeys: shared immortal generator with priming

`NameKeyGenerator` assigns sequential IDs (`NameKeyGenerator.cpp:224-231`). About 1,250 process-wide caches hold those IDs:

| Cache | Count |
|---|---|
| `getModuleNameKey()::nk` via `Module.h:139` | 233 |
| Function-local `static NameKeyType` | 199 |
| GUI file statics | 428 |
| `TheKey_*` | 127 |
| `FunctionLexicon` entries | 247 |

**Chosen: Option A.** One process-wide, append-only, thread-safe generator that is never deleted with an engine. The first engine completes `GameEngine::init` alone, and every later engine then finds its boot names already interned with the same IDs. None of the cache sites change.

- Static analysis covered all 12 NameKey-keyed container types, all 399 `crc()` bodies and every `GameMessage` argument. Key values never affect simulation or the logic CRC: the containers are lookup-only or are iterated order-independently (e.g. `TeamFactory` searches by unique ID and takes a max, `Team.cpp:293-331, 562-590`), and `crc()` hashes names and values, not keys.
- Retail multiplayer already tolerates different post-boot IDs on each client, because `.wnd` names are interned as players move through menus.
- A single-engine process already carries keys interned in episode *n* into episode *n+1*.
- A 32-thread model over 37,730 real Zero Hour INI tokens produced 0 ID mismatches after priming and was clean under TSan.
- Where values *do* matter: `MSG_QUEUE_UPGRADE`, `MSG_CANCEL_UPGRADE` and `MSG_PURCHASE_SCIENCE` payloads, and rlgenerals' observations/actions, use raw upgrade and science keys. These are interned at boot, so they are identical. The exception is upgrades or sciences defined in a `map.ini`; for those, rlgenerals should map by name.
- `verifyNameKeyID`/`syncNameKeyID` exist only under `RETAIL_COMPATIBLE_CRC`, which is 0 by default.

**Priming failure.** If the priming engine fails partway through `GameEngine::init`, the generator is left partly primed and its ID sequence can no longer be trusted. The latch then poisons the process: every later create fails with a clear error, and the host restarts the process. Retrying in-process is not safe, because a retry would intern names after the partial prefix.

Changes: `std::atomic` bucket heads with lock-free reads, a mutex on insert, immutable name storage, and a `keyToName` that returns a fresh string. Make `StaticNameKey::m_key` atomic, remove the delete in `~GameEngine` (`GameEngine.cpp:310-311`), and add a priming latch that hard-fails if a non-first engine interns any key before `TheUpgradeCenter` init.

**Gate before committing to A.** Run a solo perturbation test: intern junk names, skip IDs, or allocate descending IDs after boot, then require per-frame CRC equality over 10k+ frames on several maps, in both games. If it fails, fall back to **Option B**:
- A per-engine generator.
- `StaticNameKey` becomes per-context slots. That covers `TheKey_*` and a one-line `Module.h:139` change.
- A sed-able rewrite of 175 literal caches, plus the GUI IDs as per-engine statics.
- About 1-2 weeks of extra work.

Option C (hash-derived IDs) is rejected. `Dict` packs keys into 23 bits, and 23-bit hashes of the retail names collide 74-119 times.

## Decision 3: Allocator: keep one shared malloc

- Keep `RTS_GAMEMEMORY_ENABLE=OFF`. rlgenerals already builds `GameMemoryNull`, so no per-class pool state exists, and the global `operator new` zero-fills via calloc.
- A pointer-order audit (clang-query over 926 TUs) found **no heap-address dependence** in simulation or CRC. There are three pointer-keyed containers:
  - `ScriptEngine::AttackPriorityMap` and `ScoreKeeper::ObjectCountMap` are lookup/sum-only. Only their save-game xfer order depends on addresses.
  - `InGameUI::selectMatchingAcrossRegion` orders a `std::set<const ThingTemplate*>`. That affects the simulation, but only for live human input.
  - Cheap fix for all three: key by `getTemplateID()`.
- **Hazard that exists today:** the engine's `operator new`/`delete` are *exported* from `libgeneralsx.so` and bound through the PLT. With `LD_PRELOAD=libjemalloc.so` (common in RL training), or with `libstdc++` loaded `RTLD_GLOBAL` first, the engine silently binds a **non-zeroing** `new`. Upstream documents that zeroing is needed for correctness. Fix: hidden visibility (or `-Bsymbolic-functions`) for the Null implementation's operators. This is safe only while `GameMemoryNull` is used.

## Decision 4: Audio: device-free dummy for headless

- `SDL3GameEngine::createAudioManager` ignores its `dummy` argument, so every headless engine opens a real OpenAL device plus two threads.
- `alcMakeContextCurrent` is process-global in the statically linked openal-soft, and any engine's `closeDevice` clears the current context for everyone (`OpenALAudioManager.cpp:1587`).
- `-noaudio` exists only in `RTS_DEBUG` builds.
- Audio does **not** affect the simulation on Linux: `getFileLengthMS` always returns 0 because `SAGE_USE_FFMPEG` is never defined, and `hasMusicTrackCompleted` always returns FALSE. So a dummy is safe.
- Write a real device-free `AudioManager` subclass, with MiniAudio parity. It must keep `getFileLengthMS` semantics identical to the real backend.
- If audio is ever needed with N engines, use `alcSetThreadContext` (entered by `Scope`) or the exported `AL_EXT_direct_context` `*Direct` API, and never `alcMakeContextCurrent`.

## Change list

### Phase 0: Prerequisites that are bugs today

These are independently useful, so land them first.

**Status (on `main` after fork PRs #10 and #11, plus the Phase 0 PR):**

| Row | Status |
|---|---|
| `LeaveCriticalSection` | **Done** (Phase 0 PR). Both copies of `threads_compat.h` unlock on every leave. The mutex is `PTHREAD_MUTEX_RECURSIVE`, so each enter takes one level and each leave releases one, and nested pairs balance. |
| Drop the string locks / atomic refcount | **Deferred** (thread-only, see [Delivery scope](#delivery-scope)). |
| INI `strtok` | **Done** (Phase 0 PR). `INI` keeps its own `strtok_r` save pointer (`m_tokenSavePtr`), `getNextToken`/`getNextTokenOrNull`/`getNextSubToken` are members (every caller already used member syntax), `getNextAsciiString`'s buffer is a local, and `s_xfer` is the member `m_xfer`. The WWLib `INIClass` (`ini.cpp`) and `SimpleFileFactoryClass` (`ffactory.cpp`) loops use `strtok_r` too. Still on libc `strtok`: `GameWindowManagerScript.cpp` (the `.wnd` parser, 138 calls; each parse runs to completion on one thread, so it is only a hazard for concurrent boots) and `debug_except.cpp` (Windows crash dialog only). |
| Scratch-pad maps / `iterateSaveFiles` | **Done** by fork PR #10: `clearScratchPadMaps` and `iterateSaveFiles` list the save directory by absolute path through `TheLocalFileSystem->getFileListInDirectory` and never change the working directory, in both games; `~GameStateMap` reads its cached `m_saveDirectory`. The unused `Directory` class (another runtime `SetCurrentDirectory`) was deleted in both games. The only remaining `SetCurrentDirectory` is the one-off startup `WorkingDirectory.cpp`. |
| Hidden Null `operator new`/`delete` | **Done** by fork PR #11 (`GameMemoryNull.cpp`, Core, so both games). |
| Deep-CRC buffers | **Deferred here**: rlgenerals builds with `-DDEEP_CRC_TO_MEMORY=0` on every compile line. |
| Device-free audio | **Done** (Phase 0 PR). `OpenALAudioManagerDummy` and `MiniAudioManagerDummy`; `SDL3GameEngine::createAudioManager` (both games) honours `dummy` (headless) and `-noaudio`; `-noaudio` is registered in release builds. `GlobalData::m_headlessAudio` (both games) lets an embedding host keep the real device in headless mode. `OpenALAudioManager::closeDevice` only clears the current ALC context if it is its own. `MiniAudioManager` tracks each `openDevice` stage, so a partly failed open is fully released. Neither dummy reaches its audio library: every inherited method that would (for example `isMusicPlaying`, and the OpenAL buffer cache via `getFileLengthMS`) is overridden. **Known pre-existing parity gap:** `getFileLengthMS` reports 0 under OpenAL (it only decodes when `SAGE_USE_FFMPEG` is defined, which no build does) but the real decoded length under MiniAudio; each dummy keeps its own backend's semantics. |
| Embedded fatal errors | **Done** (Phase 0 PR). `Common/FatalEngineError.h`: `SetEngineEmbeddedMode(true)` makes `ReleaseCrash`/`ReleaseCrashLocalized` throw `FatalEngineError` after writing `ReleaseCrashInfo.txt` (and throw without writing it when `TheGlobalData` is gone); `GameEngine::init` rethrows it (both games). `GameLogic::friend_awakenUpdateModule` already goes through `RELEASE_CRASH`, so it needs no change. The `catch (...)` blocks that would swallow it or turn it into an `INIException` rethrow it first: `INI::load` and `INI::initFromINIMulti`, `TerrainLogic::loadMap`'s legacy waypoint read, `getMapPreviewImage`, `GameEngine::execute`'s update loop and `GameState`'s save/load paths (both games). Known gaps: a `catch (...)` added later without that rethrow would swallow it again, and one raised while a destructor runs still ends the process (`std::terminate`). With no `TheGlobalData` they throw only outside the teardown window: once `~GameEngine` has started (`SetEngineTearingDown`, per engine context), or while another exception is propagating, they return as before embedded mode (Phase 1 PR). |
| `calculateZones` | **Deferred** (thread-only). |

The rows below are the original investigation.

| Change | Where | Why |
|---|---|---|
| `LeaveCriticalSection` must always `pthread_mutex_unlock`; keep `ref_count` only for bookkeeping | `Generals/Code/CompatLib/Include/threads_compat.h:74-82` **and** the identical GeneralsMD copy. The Generals copy wins the include order for ZH (`CMakeLists.txt:43-45`). | Every nested enter leaks a lock level. `AsciiString::set` nests (`AsciiString.cpp:259` → `:220`), so the first thread to copy a string owns the lock forever, and every other thread deadlocks. This also causes a verified **hang at `sys.exit()`** when an engine ran on a non-main thread. Reproduced with the real header. |
| Drop the global string locks (leave `TheAsciiStringCriticalSection`/`TheUnicodeStringCriticalSection` null, a documented mode at `CriticalSection.h:101-102`). Make `m_refCount` `std::atomic<unsigned short>` as a safety net. | `AsciiString.h:90`, `UnicodeString.h:90`, launcher bindings | Once per-engine data is confined to its engine, the lock only adds contention (every string destructor takes it). |
| INI: `strtok` → `strtok_r` with a per-`INI` save pointer; make `getNextToken`/`getNextTokenOrNull`/`getNextSubToken` non-static (all 263 callers already use member syntax); make `static char buff` (`INI.cpp:857`) local; make `static Xfer* s_xfer` (`INI.cpp:80`) a member | `Core/GameEngine/Source/Common/INI/INI.cpp:432,1568,1639,1648` | glibc `strtok` keeps one process-global save pointer (demonstrated). Concurrent boots or `map.ini` loads corrupt each other. `s_xfer` points into another engine's stack frame. |
| `GameStateMap::clearScratchPadMaps` / `GameState::iterateSaveFiles`: iterate absolute paths with no `SetCurrentDirectory` | `GeneralsMD/.../SaveGame/GameStateMap.cpp:458-523`, `GameState.cpp:1361-1391`, Generals twins | It runs at **every engine teardown** and chdirs the whole process. **Data-loss bug in single-instance too:** when `<userdata>/Save/` does not exist (the default), the `SetCurrentDirectory` shim fails silently and the loop deletes every `*.map` in the cwd (the install dir). |
| Hidden visibility for the Null `operator new`/`delete` | `GameMemoryNull.cpp:164-233` | See [Decision 3](#decision-3-allocator-keep-one-shared-malloc). |
| Make deep-CRC buffers lazy, or build with `-DDEEP_CRC_TO_MEMORY=0` | `GameLogic.cpp:304-312`; `GameDefines.h:163-166` re-enables it when the CMake option is OFF | 72 MiB of RSS per engine, plus a full deep serialisation every CRC frame. The macro changes class layout and a vtable, so it must be set identically on every compile line, including rlgenerals' `defs.bzl`. |
| Device-free audio managers honouring `dummy`: an OpenAL one **and** a MiniAudio one (golden rule 4, parity); register `-noaudio` in release | `SDL3GameEngine.cpp:580-595` (both), `Core/GameEngineDevice/Source/OpenALAudioDevice/OpenALAudioManager.cpp`, `Core/GameEngineDevice/Source/MiniAudioDevice/MiniAudioManager.cpp` | See [Decision 4](#decision-4-audio-device-free-dummy-for-headless). |
| In embedded mode, `ReleaseCrash` throws `FatalEngineError`, and `GameEngine::init` rethrows instead of `_exit(1)` | `Debug.cpp:772-843`, `GameEngine.cpp:878-896`, `GameLogic::friend_awakenUpdateModule` | Otherwise one engine's fatal error kills all N. Exceptions are enabled. rlgenerals already calls `update()` directly, so step-time exceptions already propagate. Only the faulted instance is abandoned. |
| Move the `calculateZones` arrays to `PathfindZoneManager` members | `Core/.../AI/AIPathfind.cpp:2677-2744` | A 144 KB stack frame. Peak engine stack is 152 KiB, so a 128 KiB thread segfaults. macOS secondary threads default to 512 KiB. |

### Phase 1: `EngineContext` core

**Status (Phase 1a PR, the mechanism):**

| Row | Status |
|---|---|
| `EngineContext`, `t_engine`, `g_noEngine`, `rts::ctx()`, `rts::Scope` | **Done.** `Core/GameEngine/Include/Common/EngineContext.h` (+ `.cpp`): `extern constinit thread_local EngineContext* t_engine`, initial-exec on ELF, starting at `&g_noEngine` (all null), so `ctx()` is never null and a read outside every `Scope` sees "no engine". `EngineContext` also carries `engineTearingDown` (see the embedded-mode row above) and a working slot table (`allocateEngineSlotIndex`, `getSlot`/`setSlot`, destroyed newest first) for Phase 4's `PER_ENGINE_STATIC`; `countLiveSingletons()` and `rts::noEngineIsPristine()` are there for the lifecycle checks. Hot fields (RNG seeds, ...) are left to Phase 2. |
| X-macro list and `#define` header | **Done.** `Common/EngineSingletons.inl`: 119 pointer singletons (Zero Hour + Core), the ZH-only ones under `#if RTS_ZEROHOUR`; `TheFlatHeightMap` and `TheHeightMap` are in it. `Common/EngineSingletonMacros.h` is generated from it. The count is below the investigation's ~134 because the non-pointer objects and values (`TheDX8MeshRenderer`, `TheWaterTransparency`, `TheWeatherSetting`, `TheW3DFrameLengthInMsec`, `TheSupplyAndTechImageLocations`, `TheLobbyQueuedUTMs`, ...) cannot be `T*` fields; they stay globals and are classified in the script (Phase 3/4 or menu state), as are the dead online back ends (`TheWebBrowser`, `TheGPConnection`) and `TheSDL3Window` (one window per process). |
| CMake option | **Done.** `RTS_ENGINE_CONTEXT` (default OFF, `cmake/config-build.cmake`): ON adds `RTS_ENGINE_CONTEXT=1` and force-includes `EngineContext.h` into every C++ TU that links `core_config`, and needs C++20 (VC6 is refused). ON builds Zero Hour only: it turns off the Generals base game (its declarations are not guarded yet) and every tool (`*_TOOLS`/`*_EXTRAS`), which assign singletons outside any context; excluding them was cheaper than giving each tool a context. OFF is upstream-shaped: over every Zero Hour TU the preprocessed source is identical to the base commit's apart from `__LINE__` values and the new teardown hooks. |
| Guards | **Done**, by `scripts/cpp/engine_context_guards.py` (`guard` is idempotent and re-runnable after an upstream sync; `check` fails on a stale macro header, an unguarded extern or definition of a listed name, or an unclassified `extern ... The*;`; `list` prints the classification). 236 `#if !RTS_ENGINE_CONTEXT` blocks in 232 files (Core and GeneralsMD, not their Tools). |
| Hand fixes | `GlobalData.h` uses the `#define TheGlobalData` branch when ON. `TheSystemIsUnicode` is left alone: it is not in the list, so it stays the process-global const it already was. No other hand fix was needed. |
| Executables | `SDL3Main` (Zero Hour) enters a process-default context for all of `main()`. Generals' `SDL3Main` and `WinMain` do not build ON yet. |
| Lifecycle nulling, refcounted memory manager, `~EngineContext` asserts, NameKeys, the `nm` classification | **Next PR** (the rest of Phase 1). |

Measured (rlgenerals, one headless engine, Zero Hour, OpenAL): per-frame `getCRC(CRC_RECALC)` over 9,999 frames of a Hard-vs-Hard AI skirmish is identical OFF and ON, and throughput is unchanged within noise (the AI skirmish at about 4,200 logic frames/s with a CRC recalculation per frame, 2.3-2.5 s either way; an Env NOOP step loop at 16-17k steps/s either way).

- New files: `Core/GameEngine/Include/Common/EngineContext.h`, the source file, the `EngineSingletons.inl` X-macro list and the generated `#define` header. The header is force-included into every engine target and the consumer.
- Guard, rather than edit, the upstream lines with `#if !RTS_ENGINE_CONTEXT`. Scripted:
  - 147 header `extern`s
  - 21 `.cpp` `extern` redeclarations
  - 151 definitions
- Hand fixes (found by the compile experiment):
  - `TheSystemIsUnicode` (`GameEngine.cpp:1272-1274`, `Debug.cpp:866`). Make it a process-global const.
  - `GlobalData.h:629-633`. Use the `#define TheGlobalData` branch when the context is on. The C++17 `inline const GlobalData* const&` would bind once, at static init, to the no-engine context. It compiles cleanly and is silently wrong.
  - `TheFlatHeightMap`, `TheHeightMap` (`BaseHeightMap.cpp:100-101`) and `TheLobbyQueuedUTMs` (`WOLGameSetupMenu.cpp:73`). These only need list entries.
- These stay **process-global**:
  - the critical-section pointers, `TheMemoryPoolFactory`, `TheDynamicMemoryAllocator` and `TheNameKeyGenerator`
  - the debug globals and `TheVersion`
  - all constant name and field-parse tables
- Make `initMemoryManager`/`shutdownMemoryManager` refcounted. `shutdownMemoryManager` from any engine nulls `TheDynamicMemoryAllocator` for all of them.
- Lifecycle:
  - `initSubsystem` (`GameEngine.cpp:161-172`) records a hook that nulls the reference, and `shutdownAll` (`SubsystemInterface.cpp:217-226`) calls it. Today about 40 of the 43 pointers dangle after shutdown, which is why re-create fails.
  - `~EngineContext` destroys slots in reverse order. Debug builds assert that all singleton fields are null and that `g_noEngine` was never written.
- Define null-context semantics: a process-default context, or an assert. `SDL3Main`/`WinMain` and the Linux-built tools (WorldBuilder, GUIEdit, MapCacheBuilder, …) enter their own context, and destructors that read `The*` (e.g. `~SubsystemInterface`) must run inside a `Scope`.

**Phase 1 deliverable: the `nm` classification.** Snapshot every writable and `STB_GNU_UNIQUE` symbol of `libgeneralsx.so` into a checked-in list, each classified as per-engine, process-global, constant or debug-only. The per-engine entries are the work list for Phases 2-4. Phase 7 turns the same list into the CI gate.

**Acceptance criteria:**
- Phase 0: with `RTS_ENGINE_CONTEXT=OFF`, retail replays and CRCs are unchanged.
- Phase 0+1: solo parity (per-frame CRC) with the context ON, and sequential create/destroy/create of 2 engines in one process.

### Phase 2: Simulation statics that must become per-engine

| Static | Location | Severity | Fix |
|---|---|---|---|
| `PathfindCellInfo::s_infoArray` / `s_firstFree` (30k-entry pool) | `Core/.../AI/AIPathfind.cpp:1076-1077` | **Blocker at creation.** A second `Pathfinder` constructor frees the first engine's in-use pool. | Pathfinder members, reached via `TheAI->pathfinder()` (9 call sites) |
| `PolygonTrigger::ThePolygonTriggerListPtr` / `s_currentID` | `Core/.../Map/PolygonTrigger.cpp:39-40` | **Blocker.** Cleared on every map load. Walked on every object cell change (`Object.cpp:2632,2684`), so another engine's load means use-after-free during movement. | Per-engine list owned by `TerrainLogic` (31 references) |
| `MapObject` list / world `Dict` | defined in `WorldHeightMap.cpp` (device code) | **Blocker for parallel resets.** Rebuilt on every load; also writes `m_weather`. | Context fields |
| `theGameLogicSeed` / `theGameClientSeed` / `theGameAudioSeed` / `theGameLogicBaseSeed` | `Core/.../Common/RandomValue.cpp:50-65` | **Blocker.** One env's `InitRandom` reseeds every env. | Context fields. Only this file changes. |
| `getClosestObjects()::theIterFlag`, `TheContactList` | `GeneralsMD/.../Object/PartitionManager.cpp:3355, 116` | Blocker for concurrent stepping (hot loop). | `PartitionManager` members. **Not** `thread_local`: a stale done-flag can collide after an engine migrates threads. |
| `checkfortransitionsnum` (×2) | `GeneralsMD/.../Common/StateMachine.cpp:112, 205` | Blocker for concurrent stepping. Depth is summed across threads, giving spurious `STATE_FAILURE` (15.7k in a 32-thread model). | `thread_local` (it measures call-stack depth) |
| `ScriptList::s_readLists`, `SidesList` `static_readPlayerNames`, `s_mtScript` / `s_mtGroup` (xfer recovery scratch) | ScriptEngine / `SidesList.cpp:424` / `GeneralsMD/.../ScriptEngine/Scripts.cpp:72-73` | Major. Concurrent map loads swap scripts and players; concurrent xfer loads share the recovery objects. | Per-parse context, or `SidesList` members; make the recovery objects locals |
| MapUtil map-parse scratch: `m_width`, `m_height`, `m_borderSize`, `m_boundaries`, `m_dataSize`, `m_data`, `worldDict`, `m_waypoints`, `m_supplyPositions`, `m_techPositions`, `m_mapDX`…, and `s_failedMapLookups` | `Core/GameEngine/Source/GameClient/MapUtil.cpp:37,106-117` | Major. Filled by the file-static `loadMap()` (`:255-289`) on every `MapCache::addMap` miss (`:692`): at boot `updateCache`, and whenever `findMap` misses (e.g. `GameLogic.cpp:875`). Later engines boot concurrently after priming, so these race. `m_waypoints` gives the start positions. | Move into a stack-local parser object; move `s_failedMapLookups` into `MapCache`. Better still, build `MapCache` once per process and share it read-only. |
| `XferLoad::xferAsciiString`/`xferUnicodeString` `static` buffers | `Core/GameEngine/Source/Common/System/XferLoad.cpp:206,229` | Major, **only once save/load is used** (rlgenerals does not call `saveGame`/`loadGame` today). Every string read in every concurrent load shares one buffer. | Local or member buffers |
| `s_transportStatuses`, `theBuildPlan` / `thePlanSubject[64]` | `ScriptConditions`, `GameLogicDispatch` | Minor for RL (dormant or rare), but real. | Members. Reset the build plan in `clearGameData`. |
| `GlobalData::m_theOriginal` + map-driven `m_weather`/`m_timeOfDay` | `GlobalData.cpp:83,581`, `WorldHeightMap.cpp:728-742` | Blocker. Day/night and snow feed model conditions, and so bones and barrel counts. | `TheWritableGlobalData` per engine (Phase 1); move `m_theOriginal` into the context |
| 8 `AIDecisionObserver` statics | `AIDecisionObserver.cpp:34-43` | Minor | Per `GameLogic` |
| `Recorder::updateRecord()::lastFrame`, `REPLAY_CRC_INTERVAL` | `Recorder.cpp:58, 441` | Minor | Recorder members |

**Acceptance:** 2 headless engines running concurrently on different threads, each matching its solo run per frame, including staggered concurrent resets.

### Phase 3: Device-layer state that headless still uses

Headless still builds `W3DDisplay`, `W3DTerrainVisual`, `WorldHeightMap`s and `W3DAssetManager`, and logic reads bones and heights through them.

- `TheTerrainRenderObject` and `TheTerrainVisual`. Logic height, LOS and cliff queries go through them (`W3DTerrainLogic.cpp:235-343`, `BaseHeightMap.cpp:882,1025,1275`). Put them in the context, and stop the constructors registering themselves. The long-term fix is for `TerrainLogic` to own its logic height map.
- The four `W3DDisplay` class statics (`W3DDisplay.h:145-148`, 94 references). Use a `PerEnginePtr<T>` proxy (with `operator T*`, `->` and `=`) so the qualified uses stay unchanged.
- `WW3DAssetManager::TheInstance`. Change only the `Get_Instance`/`Delete_This` bodies (`assetmgr.h:203-204`); the 109 call sites stay unchanged.
- `_TheFileFactory`. Install one permanent process-wide factory that dispatches to the current engine's `TheFileSystem`, and never null it while any engine is alive.
- `TheDX8MeshRenderer` and its 5 file-static lists. These are mutated on every map load (`Free_Assets_With_Exclusion_List`) and on every mesh destruction.
- `WorldHeightMap::m_alphaTiles`. Build it once and make it immutable, or make it a member.
- Lazy tables. Make these `std::call_once`: motchan filter, sphere/ring meshes, `AssetStatus`. Refcount `WWMath::Init`/`Shutdown`.
- ID counters and RNGs. Make the mesh/material/texture ID counters, the `ParticleBuffer` `Random4Class` and the WW3D timing statics per-engine.
- `TheWaterTransparency`/`TheWeatherSetting`. Headless never frees them, so the second engine's `Water.ini` parse throws. Make them per-engine and free them outside `W3DWater`.

**Acceptance:** N (≥ 8) headless engines, concurrent boots after priming, parity over 10k+ frames on several maps.

### Phase 4: Caches and remaining statics

- **57 function-local statics** whose initialisers cache per-engine pointers (`Image*`, `ParticleSystemTemplate*`, `UpgradeTemplate*`, `ThingTemplate*`, `AudioEventRTS`, values derived from `TheGlobalData`) in 29 files, e.g. `TerrainLogic.cpp:233` and `ActiveBody.cpp:1118-1124`.
  - Use `PER_ENGINE_STATIC(T, name, init…)`: a slot registry, lazy per-context construction and reverse-order teardown. A TU-local `#define` keeps the uses unchanged. It costs about 2.5 ns per access, so hot state goes into direct context fields.
  - Static `AudioEventRTS` scratch objects become locals. This also removes lock-taking destructors at exit.
- **Mutable data inside "immutable" INI stores.** This only matters if template stores are ever shared between engines to save memory, which is a later optimisation. It covers:
  - `WeaponTemplate` historic damage
  - lazy `UpgradeMuxData`
  - `ThingTemplate` `SparseMatchFinder` caches
  - `map.ini` overrides
  Until then, each engine keeps its own stores.
- **Client state that feeds logic.** Drawable model state, the camera, selection and `MapCache` waypoints. This is covered by the client singletons going through the context. `TheSkirmishGameInfo`/`TheChallengeGameInfo` must move out of the menu `.cpp` files.
- **GUI statics that headless touches.** About 60 `ControlBar`/`InGameUI`/`Diplomacy` statics. The ~1,000 menu statics can stay process-global as long as only one engine per process runs a real UI.
- **Triage everything else** from the `nm` classification (Phase 1) into: per-engine, process-global, constant or debug-only. Name the known debug-only ones explicitly so they are not mistaken for gaps, for example the ScriptEngine `st_*` VTune/DLL handles (`ScriptEngine.cpp:63-114`), the particle-editor writer's `buff1`-`buff4` (`~9815`), `linebuff` (`~10198`) and the `warnCount` debug-message limiter (`~5980`).
- **Process-global strings once the string locks are dropped.** A process-global `AsciiString` (a function-local `static AsciiString`, such as `GetRegistryLanguage`'s `val` at `registry.cpp:492-500`, or a static table) can be copied by several engine threads at once. An atomic refcount makes the copies safe, but not copy-on-write: `ensureUniqueBufferOfSize` mutates in place when it sees `m_refCount == 1`. Rule: every process-global string is written exactly once (under `std::call_once`, before any engine thread starts) and never mutated afterwards; any that is written later moves into the context. `TheEmptyString` is safe, because it is const with a null buffer. List every such string in the triage and include them in the TSan targets.

**Acceptance:** every per-engine entry in the `nm` classification is resolved; TSan-clean with N engines.

### Phase 5: Host contract and process resources

- **`BootConfig`.** Passed to an engine factory in place of `__argc`/`__argv`, `ApplicationHWnd`, `TheSDL3Window` and the `DX8Wrapper_*` flags. It carries:
  - an absolute install root, which removes the reliance on cwd for `.big` files, `GameData.ini` and `Maps/`
  - a per-engine user-data root, replacing the `getenv(XDG_DATA_HOME)` in the `GlobalData` constructor
  - the audio, headless and render flags
- **Per-engine output paths**: `SagePatch.ini` (write it atomically or skip it when embedded), `Replays/00000000.rep`, `MapCache.ini` (atomic rename), `ReleaseCrashInfo`, `deep_crc_*` (`localtime_r`, instance id), `MapPreviews`.
- **Resource limits.**
  - File descriptors: each engine holds 35 `.big` fds, so raise `RLIMIT_NOFILE` in the host. Make an archive-open failure fatal in embedded mode; today it is silently skipped and the engine diverges.
  - Stacks: create engine threads with an explicit stack size of at least 1 MiB.
- **`Scope` must restore per-thread invariants.**
  - `setFPMode` on entry, restoring the host's `fenv` on exit.
  - `uselocale(C)`. `CrateSystem` uses `sscanf("%f")`, and INI parsing uses `strtod` on macOS.
  - `alcSetThreadContext`, if audio is on.
  - Optionally, a stack-headroom check.
- **Exit contract.** `Env.close()` destroys the engine inside its `Scope`. A module `atexit` hook joins engine threads before `libgeneralsx` static destructors run (about 215 are registered). It also covers:
  - **Env objects still alive at interpreter shutdown.** The hook closes every live engine itself, rather than relying on GC order.
  - **Abandoned engines** (below). Their threads are joined, and their contexts are never destroyed.
  - The documented fallback: `_exit` or `quick_exit` after flushing replays.
- **Faulted engines.** An engine that threw `FatalEngineError` is corrupt, so its context is not destroyed (`~GameEngine` on corrupt state, with dangling pointers, may crash). Its thread returns to the host and parks nothing. Its `.big` fds, pathfinder pool and RSS leak for the life of the process. The host counts faults and recycles the whole process after K of them, or when the fd budget or an RSS limit is reached. This is also the reason for the K processes × M threads layout.
- **Wall-clock and environment inputs.** Considered:
  - `timeGetTime` in `GameLogic.cpp:1976,2072` drives only load-progress UI updates. `lastHeardFrom`/`testTimeOut`/`initTimeOutValues` (`:4860-4912`) are network-load timeouts gated on `TheNetwork`, which is null in skirmish. Both are harmless for headless RL.
  - `getenv("HOME")` in `MapUtil.cpp:493,1234` (user map search) and `getenv("APPDATA"/"USERPROFILE"/"HOME")` in `WWLib/registryini.cpp:59-69`, as well as `XDG_DATA_HOME`. All of these move into `BootConfig`, and hosts call no `setenv` after the first engine starts.
- **Game LOD.** Pin static LOD and disable dynamic LOD per engine. Dynamic LOD follows wall-clock FPS, and LOD feeds logic in `SlowDeathBehavior`, `ObjectCreationList` and `GameLogic.cpp:1940`.

### Phase 6: Consumer (rlgenerals)

- The launcher returns an engine handle built from `BootConfig`. The `chdir` and argv synthesis go away, as does the `g_state` one-engine guard.
- Every public `Env`/`_engine`/launcher/window entry opens a `Scope` (28 files; none of the 326 uses themselves change). Delete the `extern` at `bindings/engine/terrain.cpp:12`, and make `setAIDecisionObserver` per context.
- **Release the GIL** around the C++ body of `step`/`reset` (`bindings/env/module.cpp`). Today there is no `gil_scoped_release` anywhere, so Python-thread-per-engine gets no parallelism.
- Enforce the install/mod fingerprint check at create (see [Target model](#target-model)).
- A thread-per-engine vector env, with K processes × M threads for crash isolation. SIGSEGV or abort in any engine still kills its whole process.
- Map `map.ini`-defined upgrades and sciences by name (see [Decision 2](#decision-2-namekeys-shared-immortal-generator-with-priming)).

### Phase 7: Validation and regression guards

- **Solo-vs-N harness.**
  - Compare per-frame `getCRC(CRC_RECALC)` and the observations of each of N threaded engines against a solo run on the main thread.
  - Run it with staggered concurrent resets, a create/destroy/create cycle, and `GLIBC_TUNABLES=glibc.malloc.arena_max=1` (forces interleaved addresses).
  - Also run solo with two `glibc.malloc.perturb` values to expose uninitialised reads.
  - Use long AI-vs-AI skirmishes on several maps.
- **The NameKey perturbation test** described in [Decision 2](#decision-2-namekeys-shared-immortal-generator-with-priming).
- **TSan and ASan runs** with N headless engines.
- **CI gate.**
  - Snapshot the writable and `STB_GNU_UNIQUE` symbols of `libgeneralsx.so` into a checked-in classification list (per-engine, process-global or const). Fail on unclassified new ones after each weekly upstream sync.
  - Lint for unguarded `extern … The…;`.
  - A clang-query check for pointer-keyed containers in logic code.
  These guards matter because the biggest long-term risk is state that compiles cleanly but is silently shared, and that upstream merges reintroduce.
- **Performance.** A replay benchmark with the context on vs off, on Linux and macOS.
- **OFF-build regression.** Phase 0 and Phase 2 changes land unconditionally (`strtok_r`, statics moved into members, and so on). Every change must therefore also pass a retail-replay CRC regression on the `RTS_ENGINE_CONTEXT=OFF` build, for both games, before merging.
- **macOS measurement before committing to the mechanism.** Measure the Mach-O TLV access cost for the context pointer on Apple Silicon. Verify that `constinit thread_local` compiles and behaves correctly with Apple Clang when `libgeneralsx.dylib` is loaded through Python's `dlopen`. macOS is a primary target, so if the cost is material, cache `ctx()` in a local in hot functions.

### Phase 8 (deferred): render-mode multi-instance

- `DX8Wrapper` (63 statics, about 730 in-class uses), `WW3D` (43) and `W3DShaderManager` (12) become a nested `State` struct in the context, reached through a `static State& S()`. The 2,588 `DX8Wrapper::` call sites stay unchanged.
- Shadow, water, track and smudge managers become per engine.
- `Drawable::s_modelLockCount` must be per engine. In render mode it reaches the simulation through `getBarrelCount` (`Weapon.cpp:2686`).
- SDL:
  - One host thread owns `SDL_Init`, window creation and `SDL_PollEvent`, and routes events to engines by window ID. This is mandatory on macOS, where video must run on the main thread.
  - Engines never poll SDL themselves.
  - Set `SDL_HINT_NO_SIGNAL_HANDLERS` and `SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE=0`.
- DXVK supports one device per engine thread (each `CreateDevice` gets its own `VkDevice`). Before the first device, set `dxvk.numCompilerThreads` and `DXVK_STATE_CACHE=0` once, process-wide.
- Headless and render mode are not guaranteed to simulate identically: `W3DView` vs `ViewDummy` camera completion is visible to scripts, and bridge registration is gated on the device. Test for this or decouple it.

### Generals backport

All `Core/` changes are shared. The duplicated files also need mirroring:
- `GameEngine`, `GlobalData`, `GameLogic`
- `PartitionManager`, `StateMachine`, `ScriptEngine`/`ScriptConditions`
- `Drawable`, `W3DDisplay`, `W3DTerrainLogic`
- `NameKeyGenerator`, `Module.h`, `Object`, `FunctionLexicon`, `Recorder`
- `SDL3GameEngine`, `SDL3Main`
- about 31 files in total

Add `Scripts.cpp` and the SaveGame twins (`GameStateMap.cpp`, `GameState.cpp`) to that list. `MapUtil.cpp` and `XferLoad.cpp` are Core files shared by both games, so they need no mirror.

The Linux-built tools (WorldBuilder, GUIEdit, MapCacheBuilder, W3DView, ImagePacker, ParticleEditor) assign engine singletons directly and must enter a context. The estimates table now includes them.

`EngineContext` needs a per-game layout, because Core compiles into both game libraries.

## Things checked and found harmless

- **No engine helper thread touches engine state in headless mode on Linux or macOS.**
  - `ThreadClass::Execute` is a no-op under `_UNIX`, so the texture loader, GameSpy and mouse threads never run.
  - NGMP, UpdateChecker and the screenshot threads are gated on menus or `!m_headless`.
  - openal-soft and DXVK threads touch only their own state.
  - Latent: NGMP lambdas and MiniAudio VFS callbacks would need to capture the context if they ever run.
- **No networking in headless skirmish.** No sockets are opened; LAN uses fixed ports 8086/8088 but only from menus.
- **FFmpeg** spawns no threads.
- **CPU detection** is a once-per-process static-init busy-wait.
- **Object, drawable and template IDs** are already members, so they become per-engine for free.

## Open questions

1. Does the RL library need engines to **migrate between threads** (pool stepping), or is one thread per engine enough? The design supports both, but pinning makes early validation simpler. `checkfortransitionsnum` as `thread_local` stays correct under migration, because it measures call-stack depth and returns to 0 after every call tree; `theIterFlag` is the one that must be a member.
2. Is **render mode with N > 1 engines per process** needed, or is "N headless + at most 1 rendering" acceptable? This decides whether Phase 8 is on the critical path.
3. Must engines be **destroyed and recreated** repeatedly in one process, or only created N-at-start? This decides how much Phase 1 lifecycle work is on the critical path.
4. Is **sharing immutable INI and asset data** across engines a goal? RSS is about 137 MB per engine after boot and about 275 MB after reset, of which 72 MiB is deep CRC. Without sharing, threads save little memory over processes.
5. Should this be shaped for **upstream acceptance** (TheSuperHackers chose worker processes for parallel replay simulation), or kept fork-only behind `RTS_ENGINE_CONTEXT`?
