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
// FILE: EngineStatic.h
// GeneralsX @feature cemlyn007 14/10/2026 Per-engine statics: a static variable of which each engine has its own copy,
// held by its EngineContext (docs/EMBEDDING.md). Included by Common/EngineContext.h with RTS_ENGINE_CONTEXT.
//
// A static that holds engine state is declared with one of these, beside its upstream declaration, which stays
// for the build without RTS_ENGINE_CONTEXT:
//
//   #if RTS_ENGINE_CONTEXT
//   RTS_ENGINE_STATIC(Type, name, initializer);                        // a function-local static
//   #else
//   static Type name = initializer;
//   #endif
//
//   #if RTS_ENGINE_CONTEXT
//   RTS_ENGINE_GLOBAL(Type, name, initializer);                        // a file-scope static
//   #define name RTS_ENGINE_GLOBAL_REF(name)                            // every later use names this engine's copy
//   #else
//   static Type name = initializer;
//   #endif
//
//   RTS_ENGINE_EXTERN_GLOBAL(Type, name);                              // a global's declarations, each followed
//   #define name RTS_ENGINE_GLOBAL_REF(name)                            // by the same #define
//   RTS_ENGINE_EXTERN_GLOBAL_DEFINE(Type, name, initializer);          // and its one definition
//
//   RTS_ENGINE_CLASS_STATIC(Type, name);                               // a static data member, in its class
//   #define name RTS_ENGINE_GLOBAL_REF(name)
//   RTS_ENGINE_CLASS_STATIC_DEFINE(Type, Class, name, initializer);    // its definition
//
// `Type` may be an array type (`GameWindow*[MAX_SLOTS]`); a type with a comma in it needs an alias. A class static's
// type is named in the class's scope, as in its upstream definition. A class whose constructor is private, a
// singleton's, befriends rts::EngineStaticHolder<Class> to let its per-engine instance be made. The
// initializer is what followed `=` in the upstream declaration (an expression or a braced list), `Type(arguments)`
// for a parenthesised one, or nothing for one that was zero-initialised. RTS_ENGINE_GLOBAL_REF(name) is a call,
// `name_rtsEngineGlobal()`, so a qualified use (`Class::name`, `::name`) and a use through an object still name the
// static. The macros only paste or stringise `name`, so a `#define name ...` already in force does not change them.
//
// An engine's copy is made the first time that engine uses it, zero-filled and then initialised as the static was,
// and it is destroyed with the engine's context, newest first (EngineContext::destroyStatics). Two threads of one
// engine that reach it at once make it once, as a static's initialisation is made once. Each static is identified
// by the address of a `constexpr` key declared beside it, with the upstream static's linkage: an inline function's
// static is still one static in the program, and a file's static one per file. The key is read-only, so it is not
// engine state.
#pragma once

#include <new>
#include <type_traits>

#define RTS_ENGINE_STATIC_DECLARE_KEY(key, nameString) \
	static constexpr ::rts::EngineStaticKey key { &key, nameString }

#define RTS_ENGINE_STATIC_ACCESS(Type, key, ...) \
	::rts::engineStatic<::std::type_identity_t<Type>>(key, [&](void* rtsEngineStaticStorage) { \
		::new (rtsEngineStaticStorage) ::rts::EngineStaticHolder<::std::type_identity_t<Type>>{__VA_ARGS__}; \
	})

#define RTS_ENGINE_STATIC(Type, name, ...) \
	RTS_ENGINE_STATIC_DECLARE_KEY(name##_rtsEngineStaticKey, #name); \
	::std::type_identity_t<Type>& name = RTS_ENGINE_STATIC_ACCESS(Type, name##_rtsEngineStaticKey, __VA_ARGS__)

// A function-local static that is destroyed after every other per-engine static of its engine: an allocator whose
// objects other statics may still hold when they are destroyed.
#define RTS_ENGINE_STATIC_LAST(Type, name, ...) \
	RTS_ENGINE_STATIC_DECLARE_KEY(name##_rtsEngineStaticKey, #name); \
	::std::type_identity_t<Type>& name = ::rts::engineStatic<::std::type_identity_t<Type>, true>(name##_rtsEngineStaticKey, \
		[&](void* rtsEngineStaticStorage) { \
			::new (rtsEngineStaticStorage) ::rts::EngineStaticHolder<::std::type_identity_t<Type>>{__VA_ARGS__}; \
		})

#define RTS_ENGINE_GLOBAL(Type, name, ...) \
	[[maybe_unused]] static ::std::type_identity_t<Type>& name##_rtsEngineGlobal() \
	{ \
		RTS_ENGINE_STATIC_DECLARE_KEY(name##_rtsEngineStaticKey, #name); \
		return RTS_ENGINE_STATIC_ACCESS(Type, name##_rtsEngineStaticKey, __VA_ARGS__); \
	} \
	static_assert(true)

#define RTS_ENGINE_EXTERN_GLOBAL(Type, name) ::std::type_identity_t<Type>& name##_rtsEngineGlobal()

#define RTS_ENGINE_EXTERN_GLOBAL_DEFINE(Type, name, ...) \
	::std::type_identity_t<Type>& name##_rtsEngineGlobal() \
	{ \
		RTS_ENGINE_STATIC_DECLARE_KEY(name##_rtsEngineStaticKey, #name); \
		return RTS_ENGINE_STATIC_ACCESS(Type, name##_rtsEngineStaticKey, __VA_ARGS__); \
	} \
	static_assert(true)

#define RTS_ENGINE_CLASS_STATIC(Type, name) static ::std::type_identity_t<Type>& name##_rtsEngineGlobal()

#define RTS_ENGINE_CLASS_STATIC_DEFINE(Type, Class, name, ...) \
	auto Class::name##_rtsEngineGlobal() -> ::std::type_identity_t<Type>& \
	{ \
		RTS_ENGINE_STATIC_DECLARE_KEY(name##_rtsEngineStaticKey, #name); \
		return RTS_ENGINE_STATIC_ACCESS(Type, name##_rtsEngineStaticKey, __VA_ARGS__); \
	} \
	static_assert(true)

#define RTS_ENGINE_GLOBAL_REF(name) name##_rtsEngineGlobal()
