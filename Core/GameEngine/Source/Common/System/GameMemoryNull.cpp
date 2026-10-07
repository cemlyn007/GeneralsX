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
#include <mutex>
#endif

// GeneralsX @bugfix BenderAI 24/02/2026 Phase 5 - malloc.h not on macOS
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

// GeneralsX @bugfix cemlyn007 27/09/2026 For the ELF operator new/delete forwarding below
#if defined(__ELF__) && !defined(DISABLE_GAMEMEMORY_NEW_OPERATORS)
#include <atomic>
#include <dlfcn.h>
#include <new>
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

#if RTS_ENGINE_CONTEXT
// GeneralsX @feature cemlyn007 28/09/2026 The memory manager is process-wide and every engine in the
// process uses it, so initMemoryManager/shutdownMemoryManager are refcounted: only the last shutdown
// destroys it (PLAN-023 Phase 1).
static std::mutex theMemoryManagerUsersMutex;
static Int theMemoryManagerUsers = 0;
#endif

/**
	Initialize the memory manager, and create TheMemoryPoolFactory and TheDynamicMemoryAllocator.
*/
void initMemoryManager()
{
#if RTS_ENGINE_CONTEXT
	std::lock_guard<std::mutex> users(theMemoryManagerUsersMutex);
	if (theMemoryManagerUsers++ > 0)
		return;
#endif
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
#if RTS_ENGINE_CONTEXT
	std::lock_guard<std::mutex> users(theMemoryManagerUsersMutex);
	if (theMemoryManagerUsers == 0 || --theMemoryManagerUsers > 0)
		return;
#endif
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

// GeneralsX @bugfix cemlyn007 27/09/2026 Hide the replaceable new/delete on ELF and forward them to the process's
// Exported, the replaceable forms are bound through the PLT, so the first operator new in the lookup scope wins. An
// LD_PRELOAD allocator (jemalloc), or a C++ library loaded RTLD_GLOBAL before a dlopen'd engine, then hands the
// engine memory that is not zeroed. In a clean load these instead replace operator new for every C++ library loaded
// after the engine. So on ELF they are hidden: every engine call binds to them at link time, and nothing else does.
// Memory still crosses the boundary (libstdc++'s out-of-line code allocates and inlined engine code frees, or the
// reverse), so the hidden forms must not use an allocator of their own. They forward to the process's operator
// new/delete (libstdc++, an LD_PRELOAD allocator, or the sanitizer runtime), found with dlsym, which cannot see the
// hidden definitions, and new then zero-fills. Every allocation in the process therefore uses one allocator.
// The pooled GameMemory.cpp operators are a real allocator of their own, so they must stay exported instead.
// The platform-specific parts stay in this file: the directives must name the symbols defined here, and the
// forwarding is the body of these operators.
#if defined(__ELF__)

namespace
{

// The replaceable forms' mangled names depend on the type of size_t: m for unsigned long, j for unsigned int.
constexpr char SizeCode = std::is_same<size_t, unsigned long>::value ? 'm'
	: std::is_same<size_t, unsigned int>::value ? 'j'
	: 0;

// A visibility attribute cannot hide them: <new> declares the replaceable forms with default visibility, so GCC
// ignores the attribute and Clang rejects it. The assembler directives set it on the definitions instead. The
// function is never called; it only carries the directives for the size_t in use. With any other size_t type
// the operators stay exported, and forwarding to the process's operators is then skipped.
__attribute__((used)) void hideReplaceableOperators()
{
	if constexpr (SizeCode == 'm')
	{
		__asm__(
			".hidden _Znwm\n"   // operator new(size_t)
			".hidden _Znam\n"   // operator new[](size_t)
			".hidden _ZdlPv\n"  // operator delete(void *)
			".hidden _ZdaPv\n"  // operator delete[](void *)
			".hidden _ZdlPvm\n" // operator delete(void *, size_t)
			".hidden _ZdaPvm\n" // operator delete[](void *, size_t)
		);
	}
	else if constexpr (SizeCode == 'j')
	{
		__asm__(
			".hidden _Znwj\n"
			".hidden _Znaj\n"
			".hidden _ZdlPv\n"
			".hidden _ZdaPv\n"
			".hidden _ZdlPvj\n"
			".hidden _ZdaPvj\n"
		);
	}
}

struct ProcessOperators
{
	void *(*newScalar)(size_t);
	void *(*newArray)(size_t);
	void (*deleteScalar)(void *);
	void (*deleteArray)(void *);
	void (*deleteScalarSized)(void *, size_t); // null when the process has no sized form
	void (*deleteArraySized)(void *, size_t);
	bool throwsBadAlloc;
};

void *mallocOrThrow(size_t size)
{
	void *p = malloc(size);
	if (p == nullptr)
		throw ERROR_OUT_OF_MEMORY;
	return p;
}

void freeUnsized(void *p)
{
	free(p);
}

void freeSized(void *p, size_t)
{
	free(p);
}

template <typename Function>
Function findProcessSymbol(const char *name)
{
	return reinterpret_cast<Function>(dlsym(RTLD_DEFAULT, name));
}

ProcessOperators findProcessOperators()
{
	if constexpr (SizeCode != 0)
	{
		// RTLD_DEFAULT searches this module's lookup scope, the same one its PLT would use, and a hidden symbol is
		// not in the dynamic symbol table, so these find the operators the rest of the process is bound to.
		// dlsym is C code in libc and can only call malloc, never these hidden operators, so it cannot recurse.
		static constexpr char newScalarName[] = { '_', 'Z', 'n', 'w', SizeCode, '\0' };
		static constexpr char newArrayName[] = { '_', 'Z', 'n', 'a', SizeCode, '\0' };
		static constexpr char deleteScalarSizedName[] = { '_', 'Z', 'd', 'l', 'P', 'v', SizeCode, '\0' };
		static constexpr char deleteArraySizedName[] = { '_', 'Z', 'd', 'a', 'P', 'v', SizeCode, '\0' };

		ProcessOperators found;
		found.newScalar = findProcessSymbol<void *(*)(size_t)>(newScalarName);
		found.newArray = findProcessSymbol<void *(*)(size_t)>(newArrayName);
		found.deleteScalar = findProcessSymbol<void (*)(void *)>("_ZdlPv");
		found.deleteArray = findProcessSymbol<void (*)(void *)>("_ZdaPv");
		found.deleteScalarSized = findProcessSymbol<void (*)(void *, size_t)>(deleteScalarSizedName);
		found.deleteArraySized = findProcessSymbol<void (*)(void *, size_t)>(deleteArraySizedName);
		found.throwsBadAlloc = true;

		if (found.newScalar != nullptr && found.newArray != nullptr
			&& found.deleteScalar != nullptr && found.deleteArray != nullptr)
		{
			return found;
		}
	}

	// No C++ runtime exports them (for example a static libstdc++, which then binds to these operators), or the
	// operators stay exported. The new and delete forms fall back together, so they always pair.
	ProcessOperators fallback = { mallocOrThrow, mallocOrThrow, freeUnsized, freeUnsized, freeSized, freeSized, false };
	return fallback;
}

// Resolved on first use, since the engine can allocate during static initialisation. Both are constant
// initialised, so they are valid before any constructor runs.
ProcessOperators TheProcessOperators;
std::atomic<int> TheProcessOperatorsState(0); // 0 unresolved, 1 being published, 2 ready

__attribute__((noinline)) const ProcessOperators &resolveProcessOperators()
{
	// Threads racing here all look the symbols up, then one publishes its result. The loser waits for a
	// few stores only, never for a lookup, so every thread uses the same table for the process lifetime.
	const ProcessOperators found = findProcessOperators();
	int expected = 0;
	if (TheProcessOperatorsState.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
	{
		TheProcessOperators = found;
		TheProcessOperatorsState.store(2, std::memory_order_release);
	}
	else
	{
		while (TheProcessOperatorsState.load(std::memory_order_acquire) != 2)
		{
		}
	}
	return TheProcessOperators;
}

inline const ProcessOperators &processOperators()
{
	if (TheProcessOperatorsState.load(std::memory_order_acquire) == 2)
		return TheProcessOperators;
	return resolveProcessOperators();
}

void *allocateZeroed(void *(*allocate)(size_t), bool throwsBadAlloc, size_t size)
{
	void *p;
	if (throwsBadAlloc)
	{
		// Keep the engine's out of memory error, which GameEngine::execute catches.
		try
		{
			p = allocate(size);
		}
		catch (const std::bad_alloc &)
		{
			throw ERROR_OUT_OF_MEMORY;
		}
	}
	else
	{
		p = allocate(size);
	}
	memset(p, 0, size);
	return p;
}

} // namespace

void * __cdecl operator new(size_t size)
{
	const ProcessOperators &ops = processOperators();
	return allocateZeroed(ops.newScalar, ops.throwsBadAlloc, size);
}

void __cdecl operator delete(void *p)
{
	processOperators().deleteScalar(p);
}

void __cdecl operator delete(void *p, size_t size)
{
	const ProcessOperators &ops = processOperators();
	if (ops.deleteScalarSized != nullptr)
		ops.deleteScalarSized(p, size);
	else
		ops.deleteScalar(p);
}

void * __cdecl operator new[](size_t size)
{
	const ProcessOperators &ops = processOperators();
	return allocateZeroed(ops.newArray, ops.throwsBadAlloc, size);
}

void __cdecl operator delete[](void *p)
{
	processOperators().deleteArray(p);
}

void __cdecl operator delete[](void *p, size_t size)
{
	const ProcessOperators &ops = processOperators();
	if (ops.deleteArraySized != nullptr)
		ops.deleteArraySized(p, size);
	else
		ops.deleteArray(p);
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
