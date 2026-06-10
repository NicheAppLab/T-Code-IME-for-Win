#include "Globals.h"

HINSTANCE g_hInst = nullptr;
LONG g_cRefDll = 0;

// e1cedb7a-7947-4464-a357-1998170ba4db
const CLSID CLSID_TCodeIME = {
    0xe1cedb7a, 0x7947, 0x4464, {0xa3, 0x57, 0x19, 0x98, 0x17, 0x0b, 0xa4, 0xdb}
};

// bbcb576b-a5ea-4f54-b02e-c5d1cb5fbdbd
const GUID GUID_Profile = {
    0xbbcb576b, 0xa5ea, 0x4f54, {0xb0, 0x2e, 0xc5, 0xd1, 0xcb, 0x5f, 0xbd, 0xbd}
};

void DllAddRef() {
    InterlockedIncrement(&g_cRefDll);
}

void DllRelease() {
    InterlockedDecrement(&g_cRefDll);
}
