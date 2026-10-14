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
// held by its EngineContext (docs/EMBEDDING.md). Included by Common/EngineContext.h, in both builds.
//
// A static that holds state is declared with one of these in place of its `static` declaration:
//
//   RTS_ENGINE_STATIC(Type, name, initializer);   // a function-local static
//
//   RTS_ENGINE_GLOBAL(Type, name, initializer);   // a file-scope static, then on the next line
//   #define name RTS_ENGINE_GLOBAL_REF(name)      // so that every later use in the file names this engine's copy
//
// `Type` may be an array type (`GameWindow*[MAX_SLOTS]`). The initializer is what followed `=` in the original
// declaration (an expression or a braced list), `Type(arguments)` for a parenthesised one, or nothing for one
// that was zero-initialised.
//
// With RTS_ENGINE_CONTEXT, an engine's copy is made the first time that engine uses it, zero-filled and then
// initialised as the static was, and it is destroyed with the engine's context, newest first
// (EngineContext::destroyStatics). Two threads of one engine that reach it at once make it once, as a static's
// initialisation is made once. Each static is identified by the address of a `constexpr` key declared beside it
// with the original static's linkage: an inline function's static is still one static in the program, and a
// file's static is one per file. The key is read-only, so it is not engine state. Without RTS_ENGINE_CONTEXT,
// the macros declare the original static.
#pragma once

#include <type_traits>

#define RTS_ENGINE_STATIC_KEY_NAME(name) name##_rtsEngineStaticKey

#if defined(RTS_ENGINE_CONTEXT) && RTS_ENGINE_CONTEXT

#define RTS_ENGINE_STATIC_KEY(name) \
	static constexpr ::rts::EngineStaticKey RTS_ENGINE_STATIC_KEY_NAME(name) { &RTS_ENGINE_STATIC_KEY_NAME(name), #name }

#define RTS_ENGINE_STATIC_ACCESS(Type, name, ...) \
	::rts::engineStatic<::std::type_identity_t<Type>>(RTS_ENGINE_STATIC_KEY_NAME(name), \
		[&](void* rtsEngineStaticStorage) { \
			::new (rtsEngineStaticStorage) ::rts::EngineStaticHolder<::std::type_identity_t<Type>>{__VA_ARGS__}; \
		})

#define RTS_ENGINE_STATIC(Type, name, ...) \
	RTS_ENGINE_STATIC_KEY(name); \
	::std::type_identity_t<Type>& name = RTS_ENGINE_STATIC_ACCESS(Type, name, __VA_ARGS__)

#define RTS_ENGINE_GLOBAL(Type, name, ...) \
	RTS_ENGINE_STATIC_KEY(name); \
	[[maybe_unused]] static ::std::type_identity_t<Type>& name##_rtsEngineGlobal() \
	{ \
		return RTS_ENGINE_STATIC_ACCESS(Type, name, __VA_ARGS__); \
	} \
	static_assert(true)

#define RTS_ENGINE_GLOBAL_REF(name) (name##_rtsEngineGlobal())

#else

#define RTS_ENGINE_STATIC(Type, name, ...) static ::std::type_identity_t<Type> name __VA_OPT__(= __VA_ARGS__)

#define RTS_ENGINE_GLOBAL(Type, name, ...) \
	static ::std::type_identity_t<Type> name##_rtsEngineGlobal __VA_OPT__(= __VA_ARGS__)

#define RTS_ENGINE_GLOBAL_REF(name) name##_rtsEngineGlobal

#endif
