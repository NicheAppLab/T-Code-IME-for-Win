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
#include "EditSession.h"


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
    } else if (IsEqualIID(riid, IID_ITfTextLayoutSink)) {
        *ppvObj = static_cast<ITfTextLayoutSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadFocusSink)) {
        *ppvObj = static_cast<ITfThreadFocusSink*>(this);
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

    CComPtr<ITfSource> pSource;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource) {
        pSource->AdviseSink(IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this), &_dwThreadFocusSinkCookie);
    }

    // Run an initial manual setup block for the startup window state
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr) {
        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext) {
            InitContextSink(pContext);
        }
    }

    SetInputMode(InputMode::Direct);

    // Return S_OK as long as the keystroke sink was installed successfully.
    // Language bar items are optional UI and should not block activation.
    return fRetKeyMgr ? S_OK : E_FAIL;
}

// Deactivate: remove mode button and brand button from language bar and release resources
STDMETHODIMP CTCodeIME::Deactivate() {
    if (_dwThreadFocusSinkCookie != 0 && _pThreadMgr) {
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource) {
            pSource->UnadviseSink(_dwThreadFocusSinkCookie);
            _dwThreadFocusSinkCookie = 0;
        }
    }

    // Clean up your layout sink on the current focused text field
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (_pThreadMgr && SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr) {
        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext) {
            UninitContextSink(pContext);
        }
    }
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

    CComPtr<ITfDocumentMgr> pDocMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr) return;

    // ==========================================
    // CRUCIAL CHANGE: GetBase instead of GetTop!
    // ==========================================
    CComPtr<ITfContext> pContext;
    if (FAILED(pDocMgr->GetTop(&pContext)) || !pContext) return;

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

void CTCodeIME::HideCandidateList() {
    // 1. Physically hide your custom Win32 window frame
    if (_pCandidateList) {
        _pCandidateList->Show(FALSE); 
    }
    // 2. Clear out the TSF UI Element lifecycle sequence
    if (_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID) {
        CComPtr<ITfUIElementMgr> pUIElementMgr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pUIElementMgr))) && pUIElementMgr) {
            pUIElementMgr->EndUIElement(_dwCandidateListUIElementId);
        }
        _dwCandidateListUIElementId = TF_INVALID_UIELEMENTID; // Reset the token identifier
    }

    if (_pCandidateList) {
        _pCandidateList->SetDocumentMgr(nullptr); 
    }
}

// layout sink
STDMETHODIMP CTCodeIME::OnLayoutChange(ITfContext* pContext, TfLayoutCode lcode, ITfContextView* pView) {
    ::OutputDebugStringW(L"--> TSF Layout Change Fired!");
    // We only care if our candidate list is active and currently visible
    if (_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID && _pCandidateList && _pCandidateList->IsShown()) {
        
        // TF_LC_CHANGE indicates the window layout, position, or view boundary shifted
        if (lcode == TF_LC_CHANGE) {
            TriggerPositionUpdate(); 
        }
    }
    return S_OK;
}

void CTCodeIME::InitContextSink(ITfContext* pContext) {
    if (!pContext) return;
    if (_dwTextLayoutSinkCookie != 0) return; // Guard against double registration

    // 1. Explicitly fetch the Context View from the current active context
    CComPtr<ITfContextView> pContextView;
    if (SUCCEEDED(pContext->GetActiveView(&pContextView)) && pContextView) {
        
        // 2. Query ITfSource specifically from the VIEW, or directly from the strict Context interface.
        // To be absolutely certain we are bypassing the parent Document Manager wrapper, 
        // cast the context explicitly to an ITfSource:
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(pContext->QueryInterface(IID_ITfSource, (void**)&pSource)) && pSource) {
            // ============================================================
            // THE FIX: Break the multiple-inheritance pointer ambiguity.
            // Explicitly extract the layout-specific vtable offset 
            // and convert it into a generic IUnknown* before passing it out.
            // ============================================================
            ITfTextLayoutSink* pLayoutSink = static_cast<ITfTextLayoutSink*>(this);
            IUnknown* pUnkSink = static_cast<IUnknown*>(pLayoutSink);
                // 3. Advise the layout sink
            HRESULT hr = pSource->AdviseSink(IID_ITfTextLayoutSink, pUnkSink, &_dwTextLayoutSinkCookie);
            
            if (SUCCEEDED(hr)) {
                ::OutputDebugStringW(L"--> InitContextSink SUCCESS: Registered ITfTextLayoutSink!\n");
            } else {
                wchar_t szLog[128];
                swprintf_s(szLog, L"--> InitContextSink FAILED: AdviseSink returned 0x%08X\n", hr);
                ::OutputDebugStringW(szLog);
            }
        }
    }
}
void CTCodeIME::UninitContextSink(ITfContext* pContext) {
    if (_dwTextLayoutSinkCookie != 0 && pContext) {
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(pContext->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource) {
            
            HRESULT hr = pSource->UnadviseSink(_dwTextLayoutSinkCookie);
            
            if (SUCCEEDED(hr)) {
                ::OutputDebugStringW(L"--> UninitContextSink SUCCESS: Unregistered Cookie\n");
            }
        }
        _dwTextLayoutSinkCookie = 0; // Explicitly reset to zero so it can be reused later
    }
}

void CTCodeIME::TriggerPositionUpdate() {
    if (!_pThreadMgr || !_pCandidateList) return;

    // 1. Get the current active document context manager
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr) return;

    // 2. Extract the top-most active text context layer
    CComPtr<ITfContext> pContext;
    if (FAILED(pDocMgr->GetTop(&pContext)) || !pContext) return;

    // 3. Use our lightweight, layout-only session class
    CComPtr<CTrackLayoutEditSession> pLayoutSession;
    pLayoutSession.Attach(new CTrackLayoutEditSession(this, pContext));
    
    if (pLayoutSession) {
        HRESULT hrSession = S_OK;
        
        // 4. FIXED: Request the lock using 'pLayoutSession' instead of '_pEditSession'
        pContext->RequestEditSession(_tfClientId, pLayoutSession, TF_ES_READ | TF_ES_SYNC, &hrSession);
    }
}

STDMETHODIMP CTCodeIME::OnSetThreadFocus() {
    if (!_pThreadMgr) return S_OK;

    // 1. Fetch the document manager that just stabilized focus
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr) {
        
        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext) {
            
            // 2. Clear any stale trackers from old windows
            UninitContextSink(pContext);
            
            // 3. Bind your layout tracker directly to the fresh context layer
            InitContextSink(pContext);
        }
    }
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnKillThreadFocus() {
    // No specific action required, focus handoff cleans itself up
    return S_OK;
}
