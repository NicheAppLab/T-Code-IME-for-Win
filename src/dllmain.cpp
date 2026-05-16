#include <windows.h>
#include "Globals.h"
#include "ClassFactory.h"
#include "Registry.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;

    if (!IsEqualIID(rclsid, CLSID_TCodeIME)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    CClassFactory* pClassFactory = new CClassFactory();
    if (pClassFactory == nullptr) return E_OUTOFMEMORY;

    HRESULT hr = pClassFactory->QueryInterface(riid, ppv);
    pClassFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return g_cRefDll == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer(void) {
    return RegisterServer() ? S_OK : E_FAIL;
}

STDAPI DllUnregisterServer(void) {
    return UnregisterServer() ? S_OK : E_FAIL;
}
