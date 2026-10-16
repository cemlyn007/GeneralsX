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
// FILE: dx8ledger.h
// GeneralsX @feature cemlyn007 16/10/2026 The device-wrapper layer's record of the D3D8 COM objects it holds.
//
// With RTS_ENGINE_CONTEXT every reference the layer holds on a D3D8 COM object (the interface, the device, and each
// texture, surface, vertex or index buffer and swap chain it makes or is handed through the device) is recorded in the
// engine's ResourceLedger, one record per reference, whose release is the object's own Release on the raw handle. The
// layer forgets a record when it releases that reference itself. So a host that releases a faulted engine's ledger
// newest first releases every object the engine still holds, then the device, then the interface, each exactly once,
// without running any of the engine's code (docs/EMBEDDING.md; design decision 15).
//
//   DX8_RECORDED(object, kind)   records the reference a creation or a Get handed the layer, and returns object
//   DX8_RECORD(object, kind)     the same, as a statement
//   DX8_ADD_REF(object, kind)    takes another reference and records it
//   DX8_RELEASE(object)          forgets one recorded reference to object and releases it; returns Release's count
//   DX8_SAFE_RELEASE(object)     SAFE_RELEASE (WWLib/WWCommon.h) for a recorded reference: releases and nulls it
//
// `kind` is one of the kinds below without its _KIND suffix (TEXTURE, SURFACE, ...). DX8_RECORDED and DX8_RECORD
// ignore a null object; DX8_ADD_REF and DX8_RELEASE need one, as AddRef and Release do. An engine with no
// rendering (RenderMode::none) records nothing: a record there is refused with std::logic_error, since such an engine
// makes no device. Without RTS_ENGINE_CONTEXT the macros are the plain COM calls.

#pragma once

#include <d3d8.h>

#if RTS_ENGINE_CONTEXT

namespace DX8Ledger
{
// The kinds a record names, one per COM interface the layer holds.
inline constexpr const char INTERFACE_KIND[] = "D3D8 interface";
inline constexpr const char DEVICE_KIND[] = "D3D8 device";
inline constexpr const char TEXTURE_KIND[] = "D3D8 texture";
inline constexpr const char CUBE_TEXTURE_KIND[] = "D3D8 cube texture";
inline constexpr const char VOLUME_TEXTURE_KIND[] = "D3D8 volume texture";
inline constexpr const char SURFACE_KIND[] = "D3D8 surface";
inline constexpr const char VERTEX_BUFFER_KIND[] = "D3D8 vertex buffer";
inline constexpr const char INDEX_BUFFER_KIND[] = "D3D8 index buffer";
inline constexpr const char SWAP_CHAIN_KIND[] = "D3D8 swap chain";
inline constexpr const char LIBRARY_KIND[] = "D3D8 library";

// Records one reference the layer already holds on object. If the ledger refuses the record, the reference is
// released and the ledger's exception propagates.
void Record(IUnknown* object, const char* kind);
// Records one reference the layer holds and returns object, for a creation's or a Get's result. Null is not recorded.
template <class Object>
Object* Recorded(Object* object, const char* kind)
{
	if (object != nullptr)
		Record(object, kind);
	return object;
}
// Takes and records one more reference on object.
ULONG Add_Ref(IUnknown* object, const char* kind);
// Forgets one recorded reference on object, then releases it. A release that leaves no reference while records of
// the object remain (one released past its record) drops them with an error on stderr, so that the ledger never
// releases a freed object.
ULONG Release(IUnknown* object);
// Loads the D3D8 library `name` for this engine and records the engine's reference to it, whose release closes it.
// Null, with nothing recorded, when the library does not load. The library is recorded before the interface made from
// it, so that the ledger closes it last.
void* Open_Library(const char* name);
// Forgets the record of a library Open_Library loaded, then closes the engine's reference to it.
void Close_Library(void* library);
// The number of references the layer has recorded on object, and on all objects; for tests.
unsigned Recorded_Count(IUnknown* object);
unsigned Recorded_Count();
} // namespace DX8Ledger

#define DX8_RECORDED(object, kind) DX8Ledger::Recorded((object), DX8Ledger::kind##_KIND)
#define DX8_RECORD(object, kind) ((void)DX8Ledger::Recorded((object), DX8Ledger::kind##_KIND))
#define DX8_ADD_REF(object, kind) DX8Ledger::Add_Ref((object), DX8Ledger::kind##_KIND)
#define DX8_RELEASE(object) DX8Ledger::Release(object)

#else

#define DX8_RECORDED(object, kind) (object)
#define DX8_RECORD(object, kind) ((void)0)
#define DX8_ADD_REF(object, kind) ((object)->AddRef())
#define DX8_RELEASE(object) ((object)->Release())

#endif

#define DX8_SAFE_RELEASE(object) { if (object) { DX8_RELEASE(object); (object) = nullptr; } }
