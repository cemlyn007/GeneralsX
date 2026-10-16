#pragma once

class CComModule
{
  public:
    void Init(void*, HINSTANCE hInstance)
    {
      m_hInstance = hInstance;
    }
    void Term() {}

  private:
    HINSTANCE m_hInstance;
};

bool GetModuleFileName(HINSTANCE hInstance, char* buffer, int size);

typedef uintptr_t (*FARPROC)();
typedef HANDLE HMODULE;

// GeneralsX @feature cemlyn007 16/10/2026 With RTS_ENGINE_CONTEXT nothing loads a library through LoadLibrary: the
// D3D8 library, the one library an engine loads, is loaded and recorded in the engine's ResourceLedger by the device
// wrapper (WW3D2/dx8ledger.h), so that a host can close it after a fault.
#if !RTS_ENGINE_CONTEXT
HMODULE LoadLibrary(const char* lpFileName);
#endif
FARPROC GetProcAddress(HMODULE hModule, const char* lpProcName);
void FreeLibrary(HMODULE hModule);