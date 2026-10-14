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
- the host's `EmbeddingConfig`, for a context made for an embedded engine.

Code reaches the current context through one `thread_local` pointer,
`rts::t_engine`, read by `rts::ctx()`. An `rts::Scope` makes a context current
for the duration of a call and restores the previous one.

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
destroyed with the context, newest first. Two threads of one engine that reach
it at once make it once.

| Original declaration | Per-engine declaration |
|---|---|
| a function-local static | `RTS_ENGINE_STATIC(Type, name, initializer)` |
| a file-scope static | `RTS_ENGINE_GLOBAL(Type, name, initializer)`, then `#define name RTS_ENGINE_GLOBAL_REF(name)` |
| an object that stands in for a static | `constexpr rts::PerEngineStatic<Type> name{}`, used through `name.get()` |

Each per-engine static is identified by the address of a read-only
`rts::EngineStaticKey` declared beside it, with the linkage the original static
had. An inline function's static is therefore still one static in the program,
and a file's static one per file. The key holds no engine state.

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
