#include "TCodeIME.h"
// Removed duplicate Deactivate stub (lines 140-143) - will rely on actual implementation below
#include "CTCodeModeButton.h"
#include "Globals.h" // needed for DllAddRef/DllRelease
// In ActivateEx, replace AddItem call with proper cast
#include <ctfutb.h>
#include "resource.h"
#ifndef TF_LBI_STYLE_TEXT
#define TF_LBI_STYLE_TEXT 0x00000002
#endif
class CManageCompositionEditSession : public ITfEditSession {
public:
    CManageCompositionEditSession(CTCodeIME* pIME, ITfContext* pContext, const std::wstring& committed, const std::wstring& composition) 
        : _cRef(1), _pIME(pIME), _pContext(pContext), _committed(committed), _composition(composition) {
        _pContext->AddRef();
    }
    ~CManageCompositionEditSession() { _pContext->Release(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = this; AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG res = InterlockedDecrement(&_cRef);
        if (res == 0) delete this;
        return res;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) {
        // 1. Handle Committed Text
        if (!_committed.empty()) {
            ITfRange* pRange = nullptr;
            if (_pIME->_pComposition) {
                _pIME->_pComposition->GetRange(&pRange);
            } else {
                TF_SELECTION sel;
                ULONG cFetched;
                if (_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) == S_OK && cFetched > 0) {
                    pRange = sel.range; // Use current selection if no composition
                }
            }

            if (pRange) {
                pRange->SetText(ec, 0, _committed.c_str(), (ULONG)_committed.length());
                if (_pIME->_pComposition) {
                    _pIME->_pComposition->EndComposition(ec);
                    _pIME->_pComposition->Release();
                    _pIME->_pComposition = nullptr;
                }
                
                // Move selection to end of committed text
                TF_SELECTION sel;
                sel.range = pRange;
                sel.range->Collapse(ec, TF_ANCHOR_END);
                sel.style.ase = TF_AE_NONE;
                sel.style.fInterimChar = FALSE;
                _pContext->SetSelection(ec, 1, &sel);
                pRange->Release();
            }
        }

        // 2. Handle Composition Text
        if (!_composition.empty()) {
            if (!_pIME->_pComposition) {
                ITfContextComposition* pContextComposition;
                if (_pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComposition) == S_OK) {
                    TF_SELECTION sel;
                    ULONG cFetched;
                    if (_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) == S_OK && cFetched > 0) {
                        pContextComposition->StartComposition(ec, sel.range, _pIME, &_pIME->_pComposition);
                        sel.range->Release();
                    }
                    pContextComposition->Release();
                }
            }

            if (_pIME->_pComposition) {
                ITfRange* pRange;
                if (_pIME->_pComposition->GetRange(&pRange) == S_OK) {
                    pRange->SetText(ec, 0, _composition.c_str(), (ULONG)_composition.length());
                    pRange->Release();
                }
            }
        } else if (_pIME->_pComposition) {
            // No composition text left, end it and clear the range
            ITfRange* pRange;
            if (_pIME->_pComposition->GetRange(&pRange) == S_OK) {
                pRange->SetText(ec, 0, nullptr, 0);
                pRange->Release();
            }
            _pIME->_pComposition->EndComposition(ec);
            _pIME->_pComposition->Release();
            _pIME->_pComposition = nullptr;
        }

        return S_OK;
    }

private:
    LONG _cRef;
    CTCodeIME* _pIME;
    ITfContext* _pContext;
    std::wstring _committed;
    std::wstring _composition;
};

CTCodeIME::CTCodeIME()
    : _cRef(1), _pThreadMgr(nullptr), _tfClientId(TF_CLIENTID_NULL), _pIPCClient(nullptr), _pComposition(nullptr), _fDirectInputMode(FALSE), _pLangBarItemSink(nullptr), _pModeButton(new CTCodeModeButton(this))
{
    DllAddRef();
    _pIPCClient = new tcode::IPCClient();
}

#include "CTCodeModeButton.h"

STDMETHODIMP CTCodeIME::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) || IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppvObj = static_cast<ITfSource*>(this);
    }
    if (*ppvObj) { AddRef(); return S_OK; }
}

// Destructor release resources
CTCodeIME::~CTCodeIME() {
    if (_pIPCClient) delete _pIPCClient;
    if (_pComposition) _pComposition->Release();
    if (_pModeButton) {
        _pModeButton->Release();
        _pModeButton = nullptr;
    }
    if (_pLangBarItemSink) {
        _pLangBarItemSink->Release();
        _pLangBarItemSink = nullptr;
    }
    DllRelease();
}

STDMETHODIMP_(ULONG) CTCodeIME::AddRef() { return InterlockedIncrement(&_cRef); }
STDMETHODIMP_(ULONG) CTCodeIME::Release() {
    ULONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

STDMETHODIMP CTCodeIME::Activate(ITfThreadMgr *ptim, TfClientId tid) { return ActivateEx(ptim, tid, 0); }
STDMETHODIMP CTCodeIME::ActivateEx(ITfThreadMgr *ptim, TfClientId tid, DWORD dwFlags) {
    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _tfClientId = tid;
    ITfKeystrokeMgr* pKeystrokeMgr;
    if (_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr) == S_OK) {
        pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeystrokeMgr->Release();
    }
    ITfLangBarItemMgr* pLangBarItemMgr;
    if (_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pLangBarItemMgr) == S_OK) {
        pLangBarItemMgr->AddItem(_pModeButton);
        pLangBarItemMgr->Release();
    }
    return S_OK;
}

// Deactivate: remove mode button from language bar and release resources
STDMETHODIMP CTCodeIME::Deactivate() {
    if (_pThreadMgr) {
        ITfLangBarItemMgr* pLangBarItemMgr;
        if (_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pLangBarItemMgr) == S_OK) {
            // Remove the mode button we added during activation
            if (_pModeButton) {
                pLangBarItemMgr->RemoveItem(_pModeButton);
                // Release reference held by language bar (if any)
                _pModeButton->Release();
                _pModeButton = nullptr;
            }
            pLangBarItemMgr->Release();
        }
        ITfKeystrokeMgr* pKeystrokeMgr;
        if (_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr) == S_OK) {
            pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
            pKeystrokeMgr->Release();
        }
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    _tfClientId = TF_CLIENTID_NULL;
    return S_OK;
}


STDMETHODIMP CTCodeIME::OnSetFocus(BOOL fForeground) { return S_OK; }

STDMETHODIMP CTCodeIME::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
    if (_pComposition) {
        _pComposition->Release();
        _pComposition = nullptr;
    }
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    bool isCtrlSlash = isCtrl && !isAlt && !isWin && (wParam == VK_OEM_2);

    if (_fDirectInputMode) {
        if (isCtrlSlash) {
            *pfEaten = TRUE;
            return S_OK;
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    if (isCtrlSlash) {
        *pfEaten = TRUE;
        return S_OK;
    }

    // Do not capture other Windows shortcuts with modifier keys (Ctrl/Alt/Win)
    if (isCtrl || isAlt || isWin) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Intercept Numbers (0x30-0x39), A-Z (0x41-0x5A), and Punctuation (0xBA-0xDF)
    if ((wParam >= 0x30 && wParam <= 0x39) || 
        (wParam >= 0x41 && wParam <= 0x5A) || 
        (wParam >= 0xBA && wParam <= 0xDF)) { 
        *pfEaten = TRUE; return S_OK; 
    }
    
    if (wParam == VK_SPACE || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_RETURN) {
        *pfEaten = TRUE; return S_OK;
    }
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    bool isCtrlSlash = isCtrl && !isAlt && !isWin && (wParam == VK_OEM_2);

    if (isCtrlSlash) {
        _fDirectInputMode = !_fDirectInputMode;
        if (_pLangBarItemSink) {
            _pLangBarItemSink->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT);
        }
        if (_fDirectInputMode) {
            // Cancel composition when switching to direct input mode
            if (_pComposition) {
                CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, L"", L"");
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
            }
            _pIPCClient->Reset();
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    if (_fDirectInputMode) {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_LWIN || wParam == VK_RWIN) {
        *pfEaten = FALSE; return S_OK;
    }

    // Do not capture other Windows shortcuts with modifier keys (Ctrl/Alt/Win)
    if (isCtrl || isAlt || isWin) {
        *pfEaten = FALSE;
        return S_OK;
    }
    
    std::wstring committed, composition;
    bool isActive = false;
    bool inputConsumed = _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);

    if (!inputConsumed) {
        // If the engine isn't active and nothing was committed, don't eat the key.
        if (committed.empty() && !isActive && (wParam == VK_SPACE || wParam == VK_BACK || wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_RETURN)) {
            *pfEaten = FALSE;
            return S_OK;
        }
    }

    if (inputConsumed || !committed.empty() || isActive) {
        *pfEaten = TRUE;
        CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
        HRESULT hr;
        pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        pEditSession->Release();
    } else {
        *pfEaten = FALSE;
    }
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) { *pfEaten = FALSE; return S_OK; }
STDMETHODIMP CTCodeIME::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) { *pfEaten = FALSE; return S_OK; }
STDMETHODIMP CTCodeIME::OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten) { *pfEaten = FALSE; return S_OK; }

// Removed old ITfLangBarItemButton methods from CTCodeIME; functionality now provided by CTCodeModeButton.

// ITfSource methods
STDMETHODIMP CTCodeIME::AdviseSink(REFIID riid, IUnknown *pUnk, DWORD *pdwCookie) {
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_NOINTERFACE;
    if (_pLangBarItemSink != nullptr) return E_FAIL;
    if (pUnk->QueryInterface(IID_ITfLangBarItemSink, (void**)&_pLangBarItemSink) != S_OK) {
        return E_NOINTERFACE;
    }
    *pdwCookie = 1;
    return S_OK;
}

// Helper methods for Direct Input mode
BOOL CTCodeIME::IsDirectInputMode() const {
    return _fDirectInputMode;
}

void CTCodeIME::ToggleDirectInputMode() {
    _fDirectInputMode = !_fDirectInputMode;
    if (_pLangBarItemSink) {
        _pLangBarItemSink->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT);
    }
}



STDMETHODIMP CTCodeIME::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != 1) return E_INVALIDARG;
    if (_pLangBarItemSink) {
        _pLangBarItemSink->Release();
        _pLangBarItemSink = nullptr;
    }
    return S_OK;
}
