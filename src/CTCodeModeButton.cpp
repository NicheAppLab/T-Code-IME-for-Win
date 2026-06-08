// CTCodeModeButton.cpp
#include "CTCodeModeButton.h"
#include "TCodeIME.h"
#include "resource.h"
#include <windows.h>
#include <ctfutb.h>
#include "Globals.h"
#include <variant>
#include "helper.h"
#include <string>

// Official Microsoft Text Services Framework Language Bar Constants
#ifndef TF_LBI_STYLE_SHOWNINTRAY
#define TF_LBI_STYLE_SHOWNINTRAY   0x00000002
#endif
#ifndef TF_LBI_STYLE_SHOWINDESC
#define TF_LBI_STYLE_SHOWINDESC    0x00000010
#endif
#ifndef TF_LBI_STYLE_BTN_BUTTON
#define TF_LBI_STYLE_BTN_BUTTON    0x00000000
#endif

using InputModeVariant = std::variant<std::monostate, int>;

// Constructor stores owner pointer and adds reference
CTCodeModeButton::CTCodeModeButton(CTCodeIME* pOwner) : _cRef(1), _pOwner(pOwner), _pSink(nullptr) {
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

STDMETHODIMP CTCodeModeButton::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    // Resolve structural inheritance ambiguities cleanly
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
    } 
    else if (IsEqualIID(riid, IID_ITfLangBarItem)) {
        // Explicitly map to the dedicated ITfLangBarItem vtable path
        *ppvObj = static_cast<ITfLangBarItem*>(this);
    } 
    else if (IsEqualIID(riid, IID_ITfSource)) {
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

STDMETHODIMP CTCodeModeButton::GetInfo(TF_LANGBARITEMINFO *pInfo) {
    if (pInfo == nullptr) {
        return E_INVALIDARG;
    }

    pInfo->clsidService = CLSID_TCodeIME;
    pInfo->guidItem = GUID_LBI_INPUTMODE; // Required for Windows 10/11 taskbar tray
    pInfo->ulSort = 0;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;

    InputMode currentMode = _pOwner->GetInputMode();
    const wchar_t* pszDescriptionText = L"Direct Mode";

    if (currentMode == InputMode::Tcode) {
        pszDescriptionText = L"T-Code Mode";
    }

    wcscpy_s(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), pszDescriptionText);
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::GetText(BSTR *pbstrText) {
    if (pbstrText == nullptr) {
        return E_INVALIDARG;
    }
    *pbstrText = nullptr;

    InputMode currentMode = _pOwner->GetInputMode();
    const wchar_t* pszModeChar = L"A"; // Fallback to Direct alphanumeric mode or Closed

    if (currentMode == InputMode::Tcode) {
        pszModeChar = L"漢"; // Switch to Hanzi/Kanji representation for T-Code mode
    }

    *pbstrText = SysAllocString(pszModeChar);
    return (*pbstrText != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeModeButton::GetStatus(DWORD *pdwStatus) {
    if (pdwStatus == nullptr) return E_INVALIDARG;
    
    // Force it to be visible and operational (0 = Enabled and running)
    *pdwStatus = 0; 
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::Show(BOOL fShow) {
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (pbstrToolTip == nullptr) {
        return E_INVALIDARG;
    }
    *pbstrToolTip = nullptr;

    // Use your unified engine state machine to choose the tooltip text
    InputMode currentMode = _pOwner->GetInputMode();
    const wchar_t* pszTipText = L"Direct Input Mode (A)";

    if (currentMode == InputMode::Tcode) {
        pszTipText = L"T-Code IME Mode (漢)";
    }

    *pbstrToolTip = SysAllocString(pszTipText);
    return (*pbstrToolTip != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeModeButton::GetIcon(HICON *phIcon) {
    if (phIcon == nullptr) return E_INVALIDARG;
    *phIcon = nullptr;

    InputMode currentMode = _pOwner->GetInputMode();
    if (currentMode == InputMode::Tcode) {
        *phIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON_MODE_TCODE));
    } else {
        *phIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON_MODE_DIRECT));
    }

    return (*phIcon != nullptr) ? S_OK : S_FALSE;
}

// ITfLangBarItemButton methods
STDMETHODIMP CTCodeModeButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    if (_pOwner) {
        _pOwner->ToggleInputMode(); // Toggle the input mode on click
    }
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::InitMenu(ITfMenu* pMenu) {
    if (pMenu == nullptr) return E_INVALIDARG;
    return S_OK;
}

STDMETHODIMP CTCodeModeButton::OnMenuSelect(UINT wID) {
    return S_OK;
}

// ITfSource implementation
STDMETHODIMP CTCodeModeButton::AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie) {
    if(pUnk == nullptr || pdwCookie == nullptr) return E_INVALIDARG;
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_FAIL;
    if(_pSink != nullptr) return E_FAIL; 

    if (pUnk->QueryInterface(IID_ITfLangBarItemSink, (void**)&_pSink) == S_OK){
        *pdwCookie = 1; 
    } else {
        return E_NOINTERFACE;
    }
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

HRESULT CTCodeModeButton::UpdateIcon() {
    if (_pSink == nullptr) {
        return E_FAIL;
    }
    // Forces the OS to re-call GetText and GetStatus instantly
    return _pSink->OnUpdate(TF_LBI_TEXT | TF_LBI_STATUS);
}
