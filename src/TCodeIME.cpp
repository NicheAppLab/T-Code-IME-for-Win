#include "TCodeIME.h"
// Removed duplicate Deactivate stub (lines 140-143) - will rely on actual implementation below
#include "CTCodeModeButton.h"
#include "CTCodeCandidateListUI.h"
#include "Globals.h" // needed for DllAddRef/DllRelease
// In ActivateEx, replace AddItem call with proper cast
#include <ctfutb.h>
#include "resource.h"
#include "helper.h"
#ifndef TF_LBI_STYLE_TEXT
#define TF_LBI_STYLE_TEXT 0x00000002
#endif
#ifndef TF_INVALID_UIELEMENTID
#define TF_INVALID_UIELEMENTID 0xFFFFFFFF
#endif
#include <msctf.h>
#include <atlbase.h> // Required for CComPtr
#include <string>

class CManageCompositionEditSession : public ITfEditSession {
public:
    CManageCompositionEditSession(CTCodeIME* pIME, ITfContext* pContext, const std::wstring& committed, const std::wstring& composition) 
        : _cRef(1), _pIME(pIME), _pContext(pContext), _committed(committed), _composition(composition) 
    {
        if (_pContext) _pContext->AddRef();
        if (_pIME) _pIME->AddRef(); // Maintain reference parity for the IME instance
    }

    ~CManageCompositionEditSession() {
        if (_pContext) _pContext->Release();
        if (_pIME) _pIME->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { 
        return InterlockedIncrement(&_cRef); 
    }

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
                    pRange = sel.range; 
                }
            }

            if (pRange) {
                pRange->SetText(ec, 0, _committed.c_str(), (ULONG)_committed.length());
                if (_pIME->_pComposition) {
                    _pIME->_pComposition->EndComposition(ec);
                    _pIME->_pComposition->Release();
                    _pIME->_pComposition = nullptr;
                }
                
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
            ITfRange* pRange;
            if (_pIME->_pComposition->GetRange(&pRange) == S_OK) {
                pRange->SetText(ec, 0, nullptr, 0);
                pRange->Release();
            }
            _pIME->_pComposition->EndComposition(ec);
            _pIME->_pComposition->Release();
            _pIME->_pComposition = nullptr;
        }

        // 3. Update Positioning Layout Controls
        if (_pIME->_pComposition && _pIME->_pCandidateList && _pIME->_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID) {
            CComPtr<ITfContextView> pContextView;
            if (SUCCEEDED(_pContext->GetActiveView(&pContextView)) && pContextView) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(_pIME->_pComposition->GetRange(&pRange)) && pRange) {
                    RECT rcText = {0};
                    BOOL fClipped = FALSE;
                    
                    if (SUCCEEDED(pContextView->GetTextExt(ec, pRange, &rcText, &fClipped))) {
                        HWND hCandidateWnd = _pIME->_pCandidateList->GetHWND();
                        if (hCandidateWnd) {
                            // 1. Move the window to the correct location while it is still HIDDEN.
                            // Notice SWP_HIDEWINDOW is NOT used; we use standard flags without SWP_SHOWWINDOW.
                            ::SetWindowPos(hCandidateWnd, HWND_TOPMOST, 
                                        rcText.left, rcText.bottom, 
                                        0, 0, 
                                        SWP_NOSIZE | SWP_NOACTIVATE);

                            // 2. Now that it is safely sitting under the caret, reveal it to the user.
                            // This bypasses the (0,0) rendering lifecycle entirely.
                            if (_pIME->_pCandidateList->IsShown()) { // Make sure TSF wants it visible
                                ::ShowWindow(hCandidateWnd, SW_SHOWNA);
                                ::InvalidateRect(hCandidateWnd, NULL, TRUE);
                            }
                        }
                    }
                    pRange->Release();
                }
            }
        }

        return S_OK; // <-- FIXED: Safely placed outside all blocks so it always runs
    }

private:
    LONG _cRef;
    CTCodeIME* _pIME;
    ITfContext* _pContext;
    std::wstring _committed;
    std::wstring _composition;
};

CTCodeIME::CTCodeIME()
    : _cRef(1), _pThreadMgr(nullptr), _tfClientId(TF_CLIENTID_NULL), _pIPCClient(nullptr), _pComposition(nullptr),
      _dwCandidateListUIElementId(TF_INVALID_UIELEMENTID)
{
    // Attach takes ownership of the initial refcount=1 without adding another reference
    _pModeButton.Attach(new CTCodeModeButton(this));
    _pCandidateList.Attach(new CTCodeCandidateListUI(this));
    DllAddRef();
    _pIPCClient = new tcode::IPCClient();
}

STDMETHODIMP CTCodeIME::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) || IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
        *ppvObj = static_cast<ITfCompartmentEventSink*>(this);
    }
    if (*ppvObj) { AddRef(); return S_OK; }

    return E_NOINTERFACE;
}

// Destructor release resources
CTCodeIME::~CTCodeIME() {
    if (_pIPCClient) delete _pIPCClient;
    if (_pComposition) _pComposition->Release();
    // CComPtr destructors automatically Release() _pModeButton
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

    BOOL fRetKeyMgr = FALSE;
    BOOL fRetBarItem = FALSE;
    ITfKeystrokeMgr* pKeystrokeMgr;
    if (_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr) == S_OK) {
        fRetKeyMgr = TRUE;
        pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, static_cast<ITfKeyEventSink*>(this), TRUE);
        pKeystrokeMgr->Release();
    }
    // Recreate buttons if they were released (defensive, e.g. if Deactivate nulled them)
    if (!_pModeButton) {
        _pModeButton.Attach(new CTCodeModeButton(this));
    }
    CComPtr<ITfLangBarItemMgr> pLangBarItemMgr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pLangBarItemMgr))) && pLangBarItemMgr) {
        if (_pModeButton) {
            // Register the mode button into the Language Bar
            HRESULT hr = pLangBarItemMgr->AddItem(_pModeButton);
            if (SUCCEEDED(hr)) {
                fRetBarItem = TRUE;
            }
        }
    }

    // Initialize compartment event sinks to track open/close and conversion mode changes
    _InitCompartmentEventSink();

    SetInputMode(InputMode::Direct);

    // Return S_OK as long as the keystroke sink was installed successfully.
    // Language bar items are optional UI and should not block activation.
    return fRetKeyMgr ? S_OK : E_FAIL;
}

// Deactivate: remove mode button and brand button from language bar and release resources
STDMETHODIMP CTCodeIME::Deactivate() { 
    if (_pThreadMgr) { 
        // Unadvise compartment event sinks
        _UninitCompartmentEventSink();

        CComPtr<ITfLangBarItemMgr> pLangBarItemMgr; 
        if (_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pLangBarItemMgr) == S_OK) { 
            // Remove the mode button from language bar (but keep the object alive via CComPtr)
            if (_pModeButton) { 
                pLangBarItemMgr->RemoveItem(_pModeButton); 
            } 
        } 

        // Hide candidate list on deactivation
        HideCandidateList();

        // Release buttons so they get recreated fresh in ActivateEx.
        // Keeping old button objects across deactivate/reactivate cycles can cause
        // the language bar to fail re-registration because _pSink may still be set.
        _pModeButton = nullptr;

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
void CTCodeIME::_KeyboardOpenCloseChanged() {
    // When the open/close state changes, update the mode button
    if (_pModeButton) {
        _pModeButton->UpdateIcon();
    }
}

void CTCodeIME::_KeyboardInputConversionChanged() {
    // When the conversion mode changes, update the mode button
    if (_pModeButton) {
        _pModeButton->UpdateIcon();
    }
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

    // Ctrl+/ always toggles mode regardless of current state
    if (isCtrlSlash) {
        *pfEaten = TRUE;
        return S_OK;
    }

    InputMode inputmode = GetInputMode();

    // In Direct mode, only eat Ctrl+/
    if (inputmode == InputMode::Direct) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // In Tcode mode, don't capture modifier keys
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
    
    if(wParam == VK_SPACE){
        if (_pIPCClient->lastResponse.buffer[0] == '\0'){
            *pfEaten = FALSE;
            return S_OK;
        }
        *pfEaten = TRUE;
        return S_OK;
    }
    if(wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN){
        if (_pIPCClient->lastResponse.buffer[0] != '\0'){
            *pfEaten = TRUE;
            return S_OK;
        }
        *pfEaten = FALSE;
        return S_OK;
    }
    if(wParam == VK_RETURN){
        // Eat Return only if there are candidates to select
        if(_pIPCClient->lastResponse.candidates[0] != '\0'){
            *pfEaten = TRUE;
            return S_OK;
        }
        *pfEaten = FALSE;
        return S_OK;
    }
    if (wParam == VK_BACK) {
        if(
            _pIPCClient->lastResponse.lastCharAsKey[0] == '\0' &&
            _pIPCClient->lastResponse.buffer[0] == '\0' &&
            _pIPCClient->lastResponse.outputBuffer[0] == '\0'
        ){
            *pfEaten = FALSE;
            return S_OK;
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}
STDMETHODIMP CTCodeIME::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;

    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    bool isCtrlSlash = isCtrl && !isAlt && !isWin && (wParam == VK_OEM_2);

    // Read the current state of the engine before executing changes
    InputMode currentMode = GetInputMode();

    if (isCtrlSlash) {
        *pfEaten = TRUE;

        // FIX 1: Compute the explicit target mode beforehand to avoid racing else-if blocks
        InputMode targetMode = (currentMode == InputMode::Tcode) ? InputMode::Direct : InputMode::Tcode;
        
        // Execute the single synchronized atomic change
        SetInputMode(targetMode);

        if (_pModeButton) {
            // Proactively invoke the Language Bar UI update
            _pModeButton->UpdateIcon();
        }

        // FIX 2: Check targetMode instead of currentMode to correctly handle Direct Input entry
        if (targetMode == InputMode::Direct) {
            // Cancel active composition when switching back to direct layout
            if (_pComposition) {
                CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, L"", L"");
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
            }
            _pIPCClient->Reset();
            HideCandidateList();
        }

        return S_OK;
    }

    // Use currentMode for structural processing throughout the remainder of the loop
    if (currentMode == InputMode::Direct) {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_LWIN || wParam == VK_RWIN) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Do not capture other Windows shortcuts with modifier keys (Ctrl/Alt/Win)
    if (isCtrl || isAlt || isWin) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // For Backspace, use the cached engine state to decide without making an IPC call.
    // If the engine has buffered content, we eat the key and send Backspace to the engine.
    // If the engine is idle, we pass Backspace through to the app.
    if (wParam == VK_BACK) {
        if (
            _pIPCClient->lastResponse.lastCharAsKey[0] != L'\0' ||
            _pIPCClient->lastResponse.buffer[0] != L'\0' ||
            _pIPCClient->lastResponse.outputBuffer[0] != L'\0'
        ) {
            *pfEaten = TRUE;
            std::wstring committed, composition;
            bool isActive = false;
            bool cmdSucceed = _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);
            if (cmdSucceed) {
                CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
                if (!committed.empty()) {
                    _pIPCClient->Reset();
                }
            } else {
                // Engine had nothing to delete — pass Backspace through to the application
                *pfEaten = FALSE;
            }
        } else {
            *pfEaten = FALSE;
        }
        SyncCandidateListFromIPC();
        return S_OK;
    }

    // Numbers, A-Z, Punctuation: always send to IPC and update composition
    if ((wParam >= 0x30 && wParam <= 0x39) || 
        (wParam >= 0x41 && wParam <= 0x5A) || 
        (wParam >= 0xBA && wParam <= 0xDF)) {
        *pfEaten = TRUE;
        std::wstring committed, composition;
        bool isActive = false;
        _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);
        CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
        HRESULT hr;
        pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        pEditSession->Release();
        // After committed text has been sent to the application, reset the IPC client
        // so that lastResponse reflects the cleared engine state. This ensures subsequent
        // Backspace checks in OnTestKeyDown/OnKeyDown work correctly.
        if (!committed.empty()) {
            _pIPCClient->Reset();
        }
        SyncCandidateListFromIPC();
        return S_OK;
    }

    // Space: send to IPC only if engine has buffer (to select candidate)
    if (wParam == VK_SPACE) {
        if (_pIPCClient->lastResponse.buffer[0] != L'\0') {
            *pfEaten = TRUE;
            std::wstring committed, composition;
            bool isActive = false;
            _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);
            CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
            HRESULT hr;
            pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
            pEditSession->Release();

            if(_pIPCClient->lastResponse.candidates[0] != L'\0'){
                SyncCandidateListFromIPC();
            }
        }else {
            *pfEaten = FALSE;
        }
        SyncCandidateListFromIPC();
        return S_OK;
    }

    // Left/Right: send to IPC only if engine has buffer (to change inflex position)
    if (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN) {
        if (_pIPCClient->lastResponse.buffer[0] != L'\0') {
            *pfEaten = TRUE;
            std::wstring committed, composition;
            bool isActive = false;
            _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);
            CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
            HRESULT hr;
            pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
            pEditSession->Release();
        } else {
            *pfEaten = FALSE;
        }
        SyncCandidateListFromIPC();
        return S_OK;
    }

    // Return: send to IPC only if candidates exist (to select), then commit outputBuffer
    if (wParam == VK_RETURN) {
        if (_pIPCClient->lastResponse.candidates[0] != L'\0') {
            *pfEaten = TRUE;
            std::wstring committed, composition;
            bool isActive = false;
            _pIPCClient->SendInput((uint32_t)wParam, committed, composition, &isActive);
            CManageCompositionEditSession* pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
            HRESULT hr;
            pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
            pEditSession->Release();
        } else {
            *pfEaten = FALSE;
        }
        SyncCandidateListFromIPC();
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

// IsDirectInputMode: return the current input mode
BOOL CTCodeIME::IsDirectInputMode() {
    InputMode im = GetInputMode();
    return (im == InputMode::Direct);
}

// ToggleInputMode: toggle the input mode and update the button
void CTCodeIME::ToggleInputMode() {
    InputMode im = GetInputMode();
    if (im == InputMode::Tcode) {
        SetInputMode(InputMode::Direct);
    } else {
        SetInputMode(InputMode::Tcode);
    }
    if (_pModeButton) {
        _pModeButton->UpdateIcon();
    }
}

// UpdateCandidateList: update the candidate list UI element via ITfUIElementMgr
void CTCodeIME::UpdateCandidateList(const std::vector<std::wstring>& candidates, int selectedIndex) {
    if (!_pThreadMgr || !_pCandidateList) return;

    CComPtr<ITfUIElementMgr> pUIElementMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pUIElementMgr))) || !pUIElementMgr) return;

    // 1. Update the internal string and selection states first
    _pCandidateList->SetCandidates(candidates, selectedIndex);

    if (_dwCandidateListUIElementId == TF_INVALID_UIELEMENTID) {
        // First time showing the candidate list — begin the UI element
        BOOL bShow = TRUE; 
        HRESULT hr = pUIElementMgr->BeginUIElement(_pCandidateList, &bShow, &_dwCandidateListUIElementId);
        
        if (SUCCEEDED(hr)) {
            // 2. CRUCIAL: Only show your physical window if TSF allows it!
            if (bShow) {
                _pCandidateList->Show(TRUE);
            } else {
                _pCandidateList->Show(FALSE); // App is UI-less; hide window, let app pull data
            }
            
            // 3. Push the initial dataset out to TSF listeners right after creation
            pUIElementMgr->UpdateUIElement(_dwCandidateListUIElementId);
        }
    } else {
        // Update existing UI element data for both native window and UI-less listeners
        pUIElementMgr->UpdateUIElement(_dwCandidateListUIElementId);

        // If your custom UI window is active, trigger its native redraw/repaint here
        // e.g., PostMessage(_hCandidateWnd, WM_PAINT, ...); or internal paint method
    }
}

// SyncCandidateListFromIPC: parse candidates from lastResponse and update the candidate list UI
void CTCodeIME::SyncCandidateListFromIPC() {
    std::vector<std::wstring> candidates;
    const wchar_t* p = _pIPCClient->lastResponse.candidates;
    while (*p != L'\0') {
        std::wstring s(p);
        if (!s.empty()) {
            candidates.push_back(s);
        }
        p += s.length() + 1; // skip past null terminator
    }

    if (candidates.empty()) {
        HideCandidateList();
    } else {
        // Use the selectedIndex returned from proxy via IPCResponse
        UpdateCandidateList(candidates, _pIPCClient->lastResponse.selectedIndex);
    }
}

// HideCandidateList: hide and end the candidate list UI element
void CTCodeIME::HideCandidateList() {
    _pCandidateList->Show(FALSE); // Close window frame
    
    if (_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID) {
        CComPtr<ITfUIElementMgr> pUIElementMgr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pUIElementMgr))) && pUIElementMgr) {
            // Tell TSF this UI element sequence is dead
            pUIElementMgr->EndUIElement(_dwCandidateListUIElementId);
        }
        _dwCandidateListUIElementId = TF_INVALID_UIELEMENTID; // Reset token
    }
}