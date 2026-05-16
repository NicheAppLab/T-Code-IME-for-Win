#include "Registry.h"
#include "Globals.h"
#include <msctf.h>
#include <string>
#include <combaseapi.h>

const wchar_t* IME_DESCRIPTION = L"T-Code IME";
const GUID GUID_TCODE_PROFILE = { 0x5e5a266a, 0x1d7c, 0x4a1a, { 0x8c, 0x9b, 0x6e, 0x8a, 0x5a, 0x5a, 0x5a, 0x5a } };

static BOOL SetKeyAndValue(HKEY hKeyBase, const wchar_t* pszSubKey, const wchar_t* pszValueName, const wchar_t* pszData) {
    HKEY hKey;
    if (RegCreateKeyExW(hKeyBase, pszSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return FALSE;
    }

    BOOL bResult = TRUE;
    if (pszData != nullptr) {
        if (RegSetValueExW(hKey, pszValueName, 0, REG_SZ, (const BYTE*)pszData, (lstrlenW(pszData) + 1) * sizeof(wchar_t)) != ERROR_SUCCESS) {
            bResult = FALSE;
        }
    }
    RegCloseKey(hKey);
    return bResult;
}

static void StringFromGUID2W(REFGUID guid, std::wstring& out) {
    WCHAR szGUID[64] = { 0 };
    StringFromGUID2(guid, szGUID, 64);
    out = szGUID;
}

BOOL RegisterServer() {
    std::wstring clsidStr;
    StringFromGUID2W(CLSID_TCodeIME, clsidStr);

    // 1. COM Registration
    std::wstring keyPath = L"CLSID\\" + clsidStr;
    if (!SetKeyAndValue(HKEY_CLASSES_ROOT, keyPath.c_str(), nullptr, IME_DESCRIPTION)) return FALSE;

    std::wstring inprocPath = keyPath + L"\\InProcServer32";
    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(g_hInst, szModule, MAX_PATH);

    if (!SetKeyAndValue(HKEY_CLASSES_ROOT, inprocPath.c_str(), nullptr, szModule)) return FALSE;
    if (!SetKeyAndValue(HKEY_CLASSES_ROOT, inprocPath.c_str(), L"ThreadingModel", L"Apartment")) return FALSE;

    // 2. TSF Registration
    ITfInputProcessorProfiles* pProfiles;
    if (CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles) == S_OK) {
        pProfiles->Register(CLSID_TCodeIME);
        
        // Register as Japanese (0x0411)
        pProfiles->AddLanguageProfile(CLSID_TCodeIME, 0x0411, GUID_TCODE_PROFILE, IME_DESCRIPTION, (ULONG)wcslen(IME_DESCRIPTION), szModule, (ULONG)wcslen(szModule), 0);
        pProfiles->Release();
    }

    // 3. Category Registration
    ITfCategoryMgr* pCategoryMgr;
    if (CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr) == S_OK) {
        pCategoryMgr->RegisterCategory(CLSID_TCodeIME, GUID_TFCAT_TIP_KEYBOARD, CLSID_TCodeIME);
        pCategoryMgr->Release();
    }

    return TRUE;
}

BOOL UnregisterServer() {
    ITfInputProcessorProfiles* pProfiles;
    if (CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles) == S_OK) {
        pProfiles->Unregister(CLSID_TCodeIME);
        pProfiles->Release();
    }

    std::wstring clsidStr;
    StringFromGUID2W(CLSID_TCodeIME, clsidStr);
    
    std::wstring inprocPath = L"CLSID\\" + clsidStr + L"\\InProcServer32";
    std::wstring keyPath = L"CLSID\\" + clsidStr;

    RegDeleteKeyW(HKEY_CLASSES_ROOT, inprocPath.c_str());
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath.c_str());

    return TRUE;
}
