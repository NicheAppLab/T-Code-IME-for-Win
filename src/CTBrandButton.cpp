// CTBrandButton.cpp
#include "CTBrandButton.h"
#include "TCodeIME.h"
#include "resource.h"
#include <windows.h>
#include <ctfutb.h>
#include "Globals.h"

#ifndef TF_LBI_STYLE_TEXT
#define TF_LBI_STYLE_TEXT 0x00000002
#endif

#ifndef TF_LBI_STYLE_SHOWNINTRAY
#define TF_LBI_STYLE_SHOWNINTRAY 0x10000000
#endif

CTBrandButton::CTBrandButton(CTCodeIME* pOwner)
    : _cRef(1), _pOwner(pOwner), _pSink(nullptr) {
    if (_pOwner) {
        _pOwner->AddRef();
    }
}

CTBrandButton::~CTBrandButton() {
    if (_pOwner) {
        _pOwner->Release();
    }
    if (_pSink) {
        _pSink->Release();
    }
}

STDMETHODIMP CTBrandButton::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppvObj = static_cast<ITfSource*>(this);
    }
    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTBrandButton::AddRef() {
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CTBrandButton::Release() {
    ULONG ref = InterlockedDecrement(&_cRef);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

STDMETHODIMP CTBrandButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;
    pInfo->clsidService = CLSID_TCodeIME;
    pInfo->guidItem = GUID_LBI_Branding;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_TEXT | TF_LBI_STYLE_SHOWNINTRAY;
    wcscpy_s(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"T‑Code IME Brand");
    pInfo->ulSort = 0;
    return S_OK;
}

STDMETHODIMP CTBrandButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0;
    return S_OK;
}

STDMETHODIMP CTBrandButton::Show(BOOL fShow) {
    return S_OK;
}

STDMETHODIMP CTBrandButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"T‑Code IME for Windows");
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTBrandButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;
    *phIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_ICON_TCODE), IMAGE_ICON,
                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    return *phIcon ? S_OK : E_FAIL;
}

STDMETHODIMP CTBrandButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;
    *pbstrText = SysAllocString(L"T‑Code");
    return *pbstrText ? S_OK : E_OUTOFMEMORY;
}

// ITfLangBarItemButton methods
STDMETHODIMP CTBrandButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    // Brand button: no action on click (could show about dialog)
    return S_OK;
}

STDMETHODIMP CTBrandButton::InitMenu(ITfMenu* pMenu) {
    return S_OK;
}

STDMETHODIMP CTBrandButton::OnMenuSelect(UINT wID) {
    return S_OK;
}

// ITfSource implementation
STDMETHODIMP CTBrandButton::AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie) {
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_NOINTERFACE;
    if (_pSink) return E_FAIL;
    if (pUnk->QueryInterface(IID_ITfLangBarItemSink, (void**)&_pSink) != S_OK) return E_NOINTERFACE;
    *pdwCookie = 1;
    return S_OK;
}

STDMETHODIMP CTBrandButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != 1) return E_INVALIDARG;
    if (_pSink) {
        _pSink->Release();
        _pSink = nullptr;
    }
    return S_OK;
}