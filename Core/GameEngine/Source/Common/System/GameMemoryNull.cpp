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

// GeneralsX @bugfix BenderAI 24/02/2026 Phase 5 - malloc.h not on macOS
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

// GeneralsX @bugfix cemlyn007 27/09/2026 For the size_t check next to the hidden new operators
#if defined(__ELF__)
#include <type_traits>
#endif

#include "Common/GameMemoryNull.h"

static Bool theMainInitFlag = false;

// ----------------------------------------------------------------------------
// PUBLIC DATA
// ----------------------------------------------------------------------------

MemoryPoolFactory *TheMemoryPoolFactory = nullptr;
DynamicMemoryAllocator *TheDynamicMemoryAllocator = nullptr;

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


#ifndef DISABLE_GAMEMEMORY_NEW_OPERATORS

// GeneralsX @bugfix cemlyn007 27/09/2026 Keep the zero-filling operators out of the ELF dynamic symbol table
// Exported, they are bound through the PLT, so the first operator new in the lookup scope wins. An
// LD_PRELOAD allocator (jemalloc), or a C++ library loaded RTLD_GLOBAL before a dlopen'd engine, then
// hands the engine memory that is not zeroed. In a clean load these instead replace operator new for
// every C++ library loaded after the engine. Hidden, each engine call binds to them at link time.
// A visibility attribute cannot do this: <new> declares the replaceable forms with default visibility,
// so GCC ignores the attribute and Clang rejects it. The assembler directive sets it on the definitions.
// Memory still crosses the boundary: libstdc++'s out-of-line code allocates with its own operator new
// and engine code can free it, or the reverse. That is safe only because both sides are malloc/free,
// so the pooled GameMemory.cpp operators must not be hidden. Code outside the linked module (an
// embedding host) now allocates engine types with its own operator new, which does not zero.
// Address sanitizer builds keep the exported operators: ASan's operator new would serve the libraries,
// and it reports memory freed across the boundary as an alloc-dealloc-mismatch.
#if defined(__SANITIZE_ADDRESS__)
#define GAMEMEMORYNULL_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define GAMEMEMORYNULL_ASAN 1
#endif
#endif

#if defined(__ELF__) && !defined(GAMEMEMORYNULL_ASAN)
static_assert(std::is_same_v<size_t, unsigned long>, "The names below mangle size_t as unsigned long (m)");
__asm__(
	".hidden _Znwm\n"      // operator new(size_t)
	".hidden _ZdlPv\n"     // operator delete(void *)
	".hidden _ZdlPvm\n"    // operator delete(void *, size_t)
	".hidden _Znam\n"      // operator new[](size_t)
	".hidden _ZdaPv\n"     // operator delete[](void *)
	".hidden _ZdaPvm\n"    // operator delete[](void *, size_t)
	".hidden _ZnwmPKci\n"  // operator new(size_t, const char *, int)
	".hidden _ZdlPvPKci\n" // operator delete(void *, const char *, int)
	".hidden _ZnamPKci\n"  // operator new[](size_t, const char *, int)
	".hidden _ZdaPvPKci\n" // operator delete[](void *, const char *, int)
);
#endif

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

// additional overloads to account for VC/MFC funky versions
void* __cdecl operator new(size_t size, const char *, int)
{
	void *p = malloc(size);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	memset(p, 0, size);
	return p;
}

void __cdecl operator delete(void *p, const char *, int)
{
	free(p);
}

void* __cdecl operator new[](size_t size, const char *, int)
{
	void *p = malloc(size);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	memset(p, 0, size);
	return p;
}

void __cdecl operator delete[](void *p, const char *, int)
{
	free(p);
}

#endif
