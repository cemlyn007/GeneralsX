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
static.

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

## Name keys

Each engine interns names in a `NameKeyGenerator` of its own, made by
`GameEngine::init` and deleted with the engine, so a name's key depends only on
the order in which that engine met its names. A static that caches a key (a
`NAMEKEY("...")` in a function, a module's `getModuleNameKey`, a window's ID)
is a per-engine static, and so is a flag that guards filling one.

## Constants

Data no engine writes is constant, so it is read-only and shared: tables are
`const`, and the constant vectors and matrices (`Vector3`, `Matrix3x3`,
`Matrix3D`) and bit masks (`BitFlags`) have `constexpr` constructors, so they
are constant-initialised. The empty `StringClass` and `WideStringClass`
buffers are constants that no constructor writes.

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
