// CTCodeModeButton.cpp
#include "CTCodeModeButton.h"
#include "TCodeIME.h"
#include "resource.h"
#include <windows.h>
#include <ctfutb.h>
#include "Globals.h"
#include "helper.h"
#include <string>

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
    pInfo->guidItem = GUID_LBI_INPUTMODE;
    pInfo->ulSort = 0;
    // Use TF_LBI_STYLE_BTN_BUTTON (not TOGGLE) — Windows may drop toggle buttons it doesn't recognize
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;

    // Description should be static - the dynamic text is returned by GetText()
    wcscpy_s(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"T-Code IME Input Mode");
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
        pszModeChar = L"漢"; // Kanji representation for T-Code mode
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
    // Show() is called BY the language bar to tell us to show/hide.
    // We just acknowledge it. The language bar will call GetInfo/GetIcon/GetText
    // as needed after this.
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
    int iconId = (currentMode == InputMode::Tcode) ? IDI_ICON_MODE_TCODE : IDI_ICON_MODE_DIRECT;
    *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCE(iconId), IMAGE_ICON,
                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    // Return S_OK even if icon loading failed — S_FALSE may cause the system tray to skip this item entirely.
    // The caller (language bar / system tray) owns the returned icon handle and must destroy it via DestroyIcon.
    return S_OK;
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

void CTCodeModeButton::Refresh() {
    UpdateIcon();
}

HRESULT CTCodeModeButton::UpdateIcon() {
    if (_pSink == nullptr) {
        return E_FAIL;
    }
    // Forces the OS to re-call GetIcon, GetText, GetTooltipString and GetStatus
    return _pSink->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP | TF_LBI_STATUS);
}
