#pragma once

#include <string>
#include <wtypes.h>

enum class InputMode {
    Closed = 0,
    Direct = 1,
    Tcode = 2,
    Unknown = -1
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

InputMode CastVariantToInputMode(const VARIANT& var);
void CastInputModeToVariant(InputMode mode, VARIANT& outVar);