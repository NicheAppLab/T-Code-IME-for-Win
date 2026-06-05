// CTCodeModeButton.cpp
#include "CTCodeModeButton.h"
#include "TCodeIME.h"
#include "resource.h"
#include <windows.h>
#include <ctfutb.h>
#include "Globals.h"

#ifndef TF_LBI_STYLE_TEXT
#define TF_LBI_STYLE_TEXT 0x00000002
#endif

// Constructor stores owner pointer and adds reference
CTCodeModeButton::CTCodeModeButton(CTCodeIME* pOwner)
    : _cRef(1), _pOwner(pOwner), _pSink(nullptr) {
    if (_pOwner) {
        _pOwner->AddRef();
    }
}

CTCodeModeButton::~CTCodeModeButton() {
    if (_pOwner) {
        _pOwner->Release();
    }
    if (_pSink) {
        _pSink->Release();
    }
}

// IUnknown implementation
STDMETHODIMP CTCodeModeButton::QueryInterface(REFIID riid, void** ppvObj) {
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

STDMETHODIMP_(ULONG) CTCodeModeButton::AddRef() {
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CTCodeModeButton::Release() {
    ULONG ref = InterlockedDecrement(&_cRef);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ITfLangBarItem methods
STDMETHODIMP CTCodeModeButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;
    pInfo->clsidService = CLSID_TCodeIME; // same CLSID as IME service
    // Unique GUID for this button item
    static const GUID GUID_ModeButton = { 0xA1B2C3D4, 0x1122, 0x3344, {0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC} };
    pInfo->guidItem = GUID_ModeButton;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_TEXT;
    wcscpy_s(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"T‑Code Mode");
    pInfo->ulSort = 0;
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0; // always enabled/visible
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::Show(BOOL fShow) {
    // Always shown in the language bar
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;
    const wchar_t* tip = _pOwner && _pOwner->IsDirectInputMode() ? L"Direct Input Mode (A)" : L"T‑Code IME Mode (あ)";
    *pbstrToolTip = SysAllocString(tip);
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

// GetIcon – choose icon based on current mode
STDMETHODIMP CTCodeModeButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;
    int iconId = (_pOwner && _pOwner->IsDirectInputMode()) ? IDI_ICON_DIRECT : IDI_ICON_TCODE;
    *phIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(iconId), IMAGE_ICON,
                              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    return *phIcon ? S_OK : E_FAIL;
}

// GetText – visible text on the button
STDMETHODIMP CTCodeModeButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;
    const wchar_t* txt = (_pOwner && _pOwner->IsDirectInputMode()) ? L"A" : L"あ";
    *pbstrText = SysAllocString(txt);
    return *pbstrText ? S_OK : E_OUTOFMEMORY;
}

// ITfLangBarItemButton methods
STDMETHODIMP CTCodeModeButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    // Forward click to owner – toggle mode
    if (_pOwner) {
        // Simulate a Ctrl+/ press which toggles mode in CTCodeIME::OnKeyDown
        _pOwner->OnKeyDown(nullptr, VK_OEM_2, 0, nullptr);
    }
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::InitMenu(ITfMenu* pMenu) {
    return S_OK; // No custom menu needed
}

STDMETHODIMP CTCodeModeButton::OnMenuSelect(UINT wID) {
    return S_OK;
}

// ITfSource implementation
STDMETHODIMP CTCodeModeButton::AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie) {
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_NOINTERFACE;
    if (_pSink) return E_FAIL; // already advised
    if (pUnk->QueryInterface(IID_ITfLangBarItemSink, (void**)&_pSink) != S_OK) return E_NOINTERFACE;
    *pdwCookie = 1; // simple fixed cookie
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != 1) return E_INVALIDARG;
    if (_pSink) {
        _pSink->Release();
        _pSink = nullptr;
    }
    return S_OK;
}
