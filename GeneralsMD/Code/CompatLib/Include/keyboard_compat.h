#pragma once

typedef unsigned int HKL;
// GeneralsX @bugfix cemlyn007 24/10/2026 Stubs defined in this header are static inline, so a source that includes it
// without calling them gets no unused-function warning.
static inline HKL GetKeyboardLayout(int)
{
  return 0;
}

#define VK_RETURN 0x0D