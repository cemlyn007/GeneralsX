#pragma once

#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <malloc.h>
#elif __APPLE__
#include <malloc/malloc.h>
#endif

#define GMEM_FIXED 0

// GeneralsX @bugfix cemlyn007 24/10/2026 Stubs defined in this header are static inline, so a source that includes it
// without calling them gets no unused-function warning.
static inline void *GlobalAlloc(int, size_t size)
{
  return malloc(size);
}

static inline void GlobalFree(void *ptr)
{
  free(ptr);
}

static inline size_t GlobalSize(void *ptr)
{
#ifdef __linux__
  return malloc_usable_size(ptr);
#elif defined(__APPLE__)
  return malloc_size(ptr);
#else
  #error "GlobalSize not implemented for this platform"
#endif
}

// MEMORYSTATUS - Windows memory status structure (for GlobalMemoryStatus)
// GeneralsX @build BenderAI 11/02/2026 Linux stub - memory profiling disabled
// GeneralsX @bugfix BenderAI 13/02/2026 Add conditional guard - DXVK may have defined it
// NOTE: Game uses this for DEBUG_LOG only (dwAvailPageFile, dwAvailPhys, dwAvailVirtual).
#ifndef _MEMORYSTATUS_DEFINED
typedef struct MEMORYSTATUS {
    unsigned long dwLength;
    unsigned long dwMemoryLoad;
    unsigned long dwTotalPhys;
    unsigned long dwAvailPhys;
    unsigned long dwTotalPageFile;
    unsigned long dwAvailPageFile;
    unsigned long dwTotalVirtual;
    unsigned long dwAvailVirtual;
} MEMORYSTATUS;
#endif // _MEMORYSTATUS_DEFINED

// Pointer typedef for MEMORYSTATUS (always define)
typedef MEMORYSTATUS *LPMEMORYSTATUS;

// GlobalMemoryStatus stub - no-op on Linux (returns zeros)
// Used for debug logging only - safe to stub
static inline void GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer) {
    if (lpBuffer) {
        memset(lpBuffer, 0, sizeof(MEMORYSTATUS));
        lpBuffer->dwLength = sizeof(MEMORYSTATUS);
    }
}