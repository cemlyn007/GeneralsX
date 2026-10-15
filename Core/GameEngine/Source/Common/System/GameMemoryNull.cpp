/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

#include "PreRTS.h"

#if RTS_ENGINE_CONTEXT
#include <stdexcept>
#endif

// GeneralsX @bugfix BenderAI 24/02/2026 Phase 5 - malloc.h not on macOS
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

// GeneralsX @bugfix cemlyn007 27/09/2026 For the ELF operator new/delete forwarding below
#if defined(__ELF__) && !defined(DISABLE_GAMEMEMORY_NEW_OPERATORS)
#include <new>
#include <type_traits>
#endif

#include "Common/GameMemoryNull.h"

#if !RTS_ENGINE_CONTEXT
static Bool theMainInitFlag = false;
#endif

// ----------------------------------------------------------------------------
// PUBLIC DATA
// ----------------------------------------------------------------------------

#if !RTS_ENGINE_CONTEXT
MemoryPoolFactory *TheMemoryPoolFactory = nullptr;
DynamicMemoryAllocator *TheDynamicMemoryAllocator = nullptr;
#endif

//-----------------------------------------------------------------------------
// METHODS for DynamicMemoryAllocator
//-----------------------------------------------------------------------------

/**
	allocate a chunk-o-bytes from this DMA and return it, but don't bother zeroing
	out the block. if unable to allocate, throw ERROR_OUT_OF_MEMORY. this
	function will never return null.

  added code to make sure we're on a DWord boundary, throw exception if not
*/
void *DynamicMemoryAllocator::allocateBytesDoNotZeroImplementation(Int numBytes)
{
	void *p = malloc(numBytes);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	return p;
}

/**
	allocate a chunk-o-bytes from this DMA and return it, and zero out the contents first.
	if unable to allocate, throw ERROR_OUT_OF_MEMORY.
	this function will never return null.
*/
void *DynamicMemoryAllocator::allocateBytesImplementation(Int numBytes)
{
	void* p = allocateBytesDoNotZeroImplementation(numBytes);	// throws on failure
	memset(p, 0, numBytes);
	return p;
}

/**
	free a chunk-o-bytes allocated by this dma. it's ok to pass null.
*/
void DynamicMemoryAllocator::freeBytes(void* pBlockPtr) noexcept
{
	free(pBlockPtr);
}

Int DynamicMemoryAllocator::getActualAllocationSize(Int numBytes)
{
	return numBytes;
}

#ifdef MEMORYPOOL_DEBUG
void DynamicMemoryAllocator::debugIgnoreLeaksForThisBlock(void* pBlockPtr)
{
}
#endif

//-----------------------------------------------------------------------------
// METHODS for MemoryPoolFactory
//-----------------------------------------------------------------------------

void MemoryPoolFactory::memoryPoolUsageReport( const char* filename, FILE *appendToFileInstead )
{
}

#ifdef MEMORYPOOL_DEBUG
void MemoryPoolFactory::debugMemoryReport(Int flags, Int startCheckpoint, Int endCheckpoint, FILE *fp )
{
}
void MemoryPoolFactory::debugSetInitFillerIndex(Int index)
{
}
#endif

//-----------------------------------------------------------------------------
// GLOBAL FUNCTIONS
//-----------------------------------------------------------------------------

#if RTS_ENGINE_CONTEXT
// GeneralsX @refactor cemlyn007 15/10/2026 Each engine has its own memory manager, made and destroyed with the engine in
// its context, so nothing counts the engines that use one (docs/EMBEDDING.md). Every one allocates from the system heap.

/**
	Initialize the current engine's memory manager, and create its TheMemoryPoolFactory and TheDynamicMemoryAllocator.
*/
void initMemoryManager()
{
	if (!rts::hasCurrentEngine())
	{
		DEBUG_CRASH(("the memory manager is made with no engine current"));
		throw std::logic_error("the memory manager is made with no engine current");
	}
	if (TheMemoryPoolFactory != nullptr || TheDynamicMemoryAllocator != nullptr)
	{
		DEBUG_CRASH(("Null Memory Manager is already initialized"));
		return;
	}

	void* const factory = malloc(sizeof(MemoryPoolFactory));
	void* const allocator = malloc(sizeof(DynamicMemoryAllocator));
	if (factory == nullptr || allocator == nullptr)
	{
		free(factory);
		free(allocator);
		throw ERROR_OUT_OF_MEMORY;
	}
	TheMemoryPoolFactory = new (factory) MemoryPoolFactory;
	TheDynamicMemoryAllocator = new (allocator) DynamicMemoryAllocator;

	DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
	DEBUG_LOG(("*** Initialized the Null Memory Manager"));
}

//-----------------------------------------------------------------------------
Bool isMemoryManagerOfficiallyInited()
{
	return TheDynamicMemoryAllocator != nullptr;
}

//-----------------------------------------------------------------------------
/**
	shutdown the current engine's memory manager. Blocks it allocated stay allocated until they are freed.
*/
void shutdownMemoryManager()
{
	if (TheDynamicMemoryAllocator != nullptr)
	{
		TheDynamicMemoryAllocator->~DynamicMemoryAllocator();
		free((void *)TheDynamicMemoryAllocator);
		TheDynamicMemoryAllocator = nullptr;
	}

	if (TheMemoryPoolFactory != nullptr)
	{
		TheMemoryPoolFactory->~MemoryPoolFactory();
		free((void *)TheMemoryPoolFactory);
		TheMemoryPoolFactory = nullptr;
	}

	DEBUG_SHUTDOWN();
}

#else

/**
	Initialize the memory manager, and create TheMemoryPoolFactory and TheDynamicMemoryAllocator.
*/
void initMemoryManager()
{
	if (TheMemoryPoolFactory == nullptr && TheDynamicMemoryAllocator == nullptr)
	{
		TheMemoryPoolFactory = new (malloc(sizeof(MemoryPoolFactory))) MemoryPoolFactory;
		TheDynamicMemoryAllocator = new (malloc(sizeof(DynamicMemoryAllocator))) DynamicMemoryAllocator;

		DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
		DEBUG_LOG(("*** Initialized the Null Memory Manager"));
	}
	else
	{
			DEBUG_CRASH(("Null Memory Manager is already initialized"));
	}

	theMainInitFlag = true;
}

//-----------------------------------------------------------------------------
Bool isMemoryManagerOfficiallyInited()
{
	return theMainInitFlag;
}

//-----------------------------------------------------------------------------
/**
	shutdown the memory manager and discard all memory. Note: if preMainInitMemoryManager()
	was called prior to initMemoryManager(), this call will do nothing.
*/
void shutdownMemoryManager()
{
	if (TheDynamicMemoryAllocator != nullptr)
	{
		TheDynamicMemoryAllocator->~DynamicMemoryAllocator();
		free((void *)TheDynamicMemoryAllocator);
		TheDynamicMemoryAllocator = nullptr;
	}

	if (TheMemoryPoolFactory != nullptr)
	{
		TheMemoryPoolFactory->~MemoryPoolFactory();
		free((void *)TheMemoryPoolFactory);
		TheMemoryPoolFactory = nullptr;
	}

	theMainInitFlag = false;

	DEBUG_SHUTDOWN();
}

#endif // RTS_ENGINE_CONTEXT


#ifndef DISABLE_GAMEMEMORY_NEW_OPERATORS

// GeneralsX @bugfix cemlyn007 27/09/2026 Hide the replaceable new/delete on ELF and forward them to the process's
// Exported, the replaceable forms are bound through the PLT, so the first operator new in the lookup scope wins. An
// LD_PRELOAD allocator (jemalloc), or a C++ library loaded RTLD_GLOBAL before a dlopen'd engine, then hands the
// engine memory that is not zeroed. In a clean load these instead replace operator new for every C++ library loaded
// after the engine. So on ELF they are hidden: every engine call binds to them at link time, and nothing else does.
// Memory still crosses the boundary (libstdc++'s out-of-line code allocates and inlined engine code frees, or the
// reverse), so the hidden forms must not use an allocator of their own. They forward to the process's operator
// new/delete (libstdc++, an LD_PRELOAD allocator, or the sanitiser runtime), and new then zero-fills. Every
// allocation in the process therefore uses one allocator.
// The pooled GameMemory.cpp operators are a real allocator of their own, so they must stay exported instead.
// The platform-specific parts stay in this file: the directives must name the symbols defined here, and the
// forwarding is the body of these operators.
// GeneralsX @refactor cemlyn007 15/10/2026 Reach the process's operators through versioned symbol references
// The forwarding used to look the process's operators up with dlsym and cache them in a table, which was writable
// process-wide state shared by every engine (docs/EMBEDDING.md). Each process operator is now an undefined
// reference to libstdc++'s versioned symbol (for example _Znwm@GLIBCXX_3.4), which the dynamic linker binds
// through the GOT like any other call into a shared library. A versioned reference is never satisfied by the
// hidden, unversioned definitions in this file, and it still binds to an LD_PRELOAD allocator or a sanitiser
// runtime that defines the operator ahead of libstdc++ in the lookup scope, as the unversioned definitions of
// libasan and libtsan do. The version tags are libstdc++'s: plain and array forms since GLIBCXX_3.4, sized deletes
// since CXXABI_1.3.9. GeneralsX defines no nothrow or aligned forms, so those stay the process's own.
// So it needs libstdc++ as a shared library; with any other C++ runtime the operators below are the exported,
// malloc-based ones, as on platforms other than ELF.
#if defined(__ELF__) && defined(__GLIBCXX__)

// The replaceable forms' mangled names depend on the type of size_t: m for unsigned long, j for unsigned int.
#if defined(__LP64__)
#define RTS_SIZE_T_CODE "m"
static_assert(std::is_same<size_t, unsigned long>::value, "size_t is unsigned long on LP64");
#else
#define RTS_SIZE_T_CODE "j"
static_assert(std::is_same<size_t, unsigned int>::value, "size_t is unsigned int on ILP32");
#endif

// The process's operators, under names of their own so that they do not collide with the definitions below.
extern "C"
{
void *rts_process_operator_new(size_t size);
void *rts_process_operator_new_array(size_t size);
void rts_process_operator_delete(void *p) noexcept;
void rts_process_operator_delete_array(void *p) noexcept;
void rts_process_operator_delete_sized(void *p, size_t size) noexcept;
void rts_process_operator_delete_array_sized(void *p, size_t size) noexcept;
}

// A visibility attribute cannot hide the definitions: <new> declares the replaceable forms with default
// visibility, so GCC ignores the attribute and Clang rejects it. The assembler directives set it instead.
__asm__(
	".symver rts_process_operator_new, _Znw" RTS_SIZE_T_CODE "@GLIBCXX_3.4\n"
	".symver rts_process_operator_new_array, _Zna" RTS_SIZE_T_CODE "@GLIBCXX_3.4\n"
	".symver rts_process_operator_delete, _ZdlPv@GLIBCXX_3.4\n"
	".symver rts_process_operator_delete_array, _ZdaPv@GLIBCXX_3.4\n"
	".symver rts_process_operator_delete_sized, _ZdlPv" RTS_SIZE_T_CODE "@CXXABI_1.3.9\n"
	".symver rts_process_operator_delete_array_sized, _ZdaPv" RTS_SIZE_T_CODE "@CXXABI_1.3.9\n"
	".hidden _Znw" RTS_SIZE_T_CODE "\n"   // operator new(size_t)
	".hidden _Zna" RTS_SIZE_T_CODE "\n"   // operator new[](size_t)
	".hidden _ZdlPv\n"                    // operator delete(void *)
	".hidden _ZdaPv\n"                    // operator delete[](void *)
	".hidden _ZdlPv" RTS_SIZE_T_CODE "\n" // operator delete(void *, size_t)
	".hidden _ZdaPv" RTS_SIZE_T_CODE "\n" // operator delete[](void *, size_t)
);

#undef RTS_SIZE_T_CODE

namespace
{

void *allocateZeroed(void *(*allocate)(size_t), size_t size)
{
	void *p;
	// Keep the engine's out of memory error, which GameEngine::execute catches.
	try
	{
		p = allocate(size);
	}
	catch (const std::bad_alloc &)
	{
		throw ERROR_OUT_OF_MEMORY;
	}
	memset(p, 0, size);
	return p;
}

} // namespace

void * __cdecl operator new(size_t size)
{
	return allocateZeroed(rts_process_operator_new, size);
}

void __cdecl operator delete(void *p)
{
	rts_process_operator_delete(p);
}

void __cdecl operator delete(void *p, size_t size)
{
	rts_process_operator_delete_sized(p, size);
}

void * __cdecl operator new[](size_t size)
{
	return allocateZeroed(rts_process_operator_new_array, size);
}

void __cdecl operator delete[](void *p)
{
	rts_process_operator_delete_array(p);
}

void __cdecl operator delete[](void *p, size_t size)
{
	rts_process_operator_delete_array_sized(p, size);
}

#else

void * __cdecl operator new(size_t size)
{
	void *p = malloc(size);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	memset(p, 0, size);
	return p;
}

void __cdecl operator delete(void *p)
{
	free(p);
}

void __cdecl operator delete(void *p, size_t)
{
	free(p);
}

void * __cdecl operator new[](size_t size)
{
	void *p = malloc(size);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	memset(p, 0, size);
	return p;
}

void __cdecl operator delete[](void *p)
{
	free(p);
}

void __cdecl operator delete[](void *p, size_t)
{
	free(p);
}

#endif

// additional overloads to account for VC/MFC funky versions
// GeneralsX @bugfix cemlyn007 27/09/2026 Forward to the forms above, and keep these exported
// The NEW and newInstance macros call these, and no other library defines them, so code built outside the engine
// module (an embedding host) gets the engine's zero-filling new too. The memory comes from the same operator new
// as everything else, so a plain delete on either side of the module boundary pairs with it.
void* __cdecl operator new(size_t size, const char *, int)
{
	return ::operator new(size);
}

void __cdecl operator delete(void *p, const char *, int)
{
	::operator delete(p);
}

void* __cdecl operator new[](size_t size, const char *, int)
{
	return ::operator new[](size);
}

void __cdecl operator delete[](void *p, const char *, int)
{
	::operator delete[](p);
}

#endif
