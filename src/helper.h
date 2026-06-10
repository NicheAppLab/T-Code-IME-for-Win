#pragma once

#include <string>
#include <wtypes.h>
#include <msctf.h>

// Represents the keyboard open/close state (0 = closed, 1 = open)
enum class OpenClose {
    Closed = 0,
    Open   = 1
};

// Represents the input conversion mode (maps to TF_CONVERSIONMODE_* values)
enum class InputMode {
    Direct = TF_CONVERSIONMODE_ALPHANUMERIC,  // 0x0000
    Tcode  = TF_CONVERSIONMODE_NATIVE         // 0x0001
};

struct ComReleaser {
    template<typename T>
    void operator()(T* ptr) const {
        if (ptr) {
            ptr->Release();
        }
    }
};

struct CompartmentMonitor {
    GUID guid;
    DWORD cookie;
    bool isGlobal;

    bool operator == (const GUID& other) const {
        return bool(::IsEqualGUID(guid, other));
    }
};

#define IID_IUNK_ARGS(pType) __uuidof(*(pType)), reinterpret_cast<IUnknown *>(pType)
#define IID_PUNK_ARGS(ppType) __uuidof(**(ppType)), reinterpret_cast<IUnknown **>(ppType)

