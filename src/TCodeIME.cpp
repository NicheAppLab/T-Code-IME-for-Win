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
    : _cRef(1), _pThreadMgr(nullptr), _tfClientId(TF_CLIENTID_NULL), m_pCEngineClient(nullptr), _pComposition(nullptr),
      _dwCandidateListUIElementId(TF_INVALID_UIELEMENTID)
{
    // Attach takes ownership of the initial refcount=1 without adding another reference
    _pModeButton.Attach(new CTCodeModeButton(this));
    _pCandidateList.Attach(new CTCodeCandidateListUI(this));
    DllAddRef();
    m_pCEngineClient = new CEngineClient();
}

STDMETHODIMP CTCodeIME::QueryInterface(REFIID riid, void **ppvObj)
{
    if (ppvObj == nullptr)
        return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) || IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    {
        *ppvObj = static_cast<ITfTextInputProcessorEx *>(this);
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    {
        *ppvObj = static_cast<ITfKeyEventSink *>(this);
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink))
    {
        *ppvObj = static_cast<ITfCompositionSink *>(this);
    }
    else if (IsEqualIID(riid, IID_ITfCompartmentEventSink))
    {
        *ppvObj = static_cast<ITfCompartmentEventSink *>(this);
    }
    else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    {
        *ppvObj = static_cast<ITfTextLayoutSink *>(this);
    }
    else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    {
        *ppvObj = static_cast<ITfThreadFocusSink *>(this);
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

// Destructor release resources
CTCodeIME::~CTCodeIME()
{
    if (m_pCEngineClient)
        delete m_pCEngineClient;
    if (_pComposition)
        _pComposition->Release();
    // CComPtr destructors automatically Release() _pModeButton
    DllRelease();
}

STDMETHODIMP_(ULONG)
CTCodeIME::AddRef() { return InterlockedIncrement(&_cRef); }
STDMETHODIMP_(ULONG)
CTCodeIME::Release()
{
    ULONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0)
        delete this;
    return cRef;
}

STDMETHODIMP CTCodeIME::Activate(ITfThreadMgr *ptim, TfClientId tid) { return ActivateEx(ptim, tid, 0); }
STDMETHODIMP CTCodeIME::ActivateEx(ITfThreadMgr *ptim, TfClientId tid, DWORD dwFlags)
{
    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _tfClientId = tid;

    BOOL fRetKeyMgr = FALSE;
    BOOL fRetBarItem = FALSE;
    ITfKeystrokeMgr *pKeystrokeMgr;
    if (_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) == S_OK)
    {
        fRetKeyMgr = TRUE;
        pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, static_cast<ITfKeyEventSink *>(this), TRUE);
        pKeystrokeMgr->Release();
    }
    // Recreate buttons if they were released (defensive, e.g. if Deactivate nulled them)
    if (!_pModeButton)
    {
        _pModeButton.Attach(new CTCodeModeButton(this));
    }
    CComPtr<ITfLangBarItemMgr> pLangBarItemMgr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pLangBarItemMgr))) && pLangBarItemMgr)
    {
        if (_pModeButton)
        {
            // Register the mode button into the Language Bar
            HRESULT hr = pLangBarItemMgr->AddItem(_pModeButton);
            if (SUCCEEDED(hr))
            {
                fRetBarItem = TRUE;
            }
        }
    }

    // Initialize compartment event sinks to track open/close and conversion mode changes
    _InitCompartmentEventSink();

    CComPtr<ITfSource> pSource;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
    {
        pSource->AdviseSink(IID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink *>(this), &_dwThreadFocusSinkCookie);
    }

    // Run an initial manual setup block for the startup window state
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr)
    {
        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext)
        {
            InitContextSink(pContext);
        }
    }

    SetInputMode(InputMode::Direct);

    // Return S_OK as long as the keystroke sink was installed successfully.
    // Language bar items are optional UI and should not block activation.
    return fRetKeyMgr ? S_OK : E_FAIL;
}

// Deactivate: remove mode button and brand button from language bar and release resources
STDMETHODIMP CTCodeIME::Deactivate()
{
    if (_dwThreadFocusSinkCookie != 0 && _pThreadMgr)
    {
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
        {
            pSource->UnadviseSink(_dwThreadFocusSinkCookie);
            _dwThreadFocusSinkCookie = 0;
        }
    }

    // Clean up your layout sink on the current focused text field
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (_pThreadMgr && SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr)
    {
        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext)
        {
            UninitContextSink(pContext);
        }
    }
    if (_pThreadMgr)
    {
        // Unadvise compartment event sinks
        _UninitCompartmentEventSink();

        CComPtr<ITfLangBarItemMgr> pLangBarItemMgr;
        if (_pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void **)&pLangBarItemMgr) == S_OK)
        {
            // Remove the mode button from language bar (but keep the object alive via CComPtr)
            if (_pModeButton)
            {
                pLangBarItemMgr->RemoveItem(_pModeButton);
            }
        }

        // Hide candidate list on deactivation
        HideCandidateList();

        // Release buttons so they get recreated fresh in ActivateEx.
        // Keeping old button objects across deactivate/reactivate cycles can cause
        // the language bar to fail re-registration because _pSink may still be set.
        _pModeButton = nullptr;

        ITfKeystrokeMgr *pKeystrokeMgr;
        if (_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr) == S_OK)
        {
            pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
            pKeystrokeMgr->Release();
        }
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    _tfClientId = TF_CLIENTID_NULL;
    return S_OK;
}
void CTCodeIME::_KeyboardOpenCloseChanged()
{
    // When the open/close state changes, update the mode button
    if (_pModeButton)
    {
        _pModeButton->UpdateIcon();
    }
}

void CTCodeIME::_KeyboardInputConversionChanged()
{
    // When the conversion mode changes, update the mode button
    if (_pModeButton)
    {
        _pModeButton->UpdateIcon();
    }
}

STDMETHODIMP CTCodeIME::OnSetFocus(BOOL fForeground) { return S_OK; }

STDMETHODIMP CTCodeIME::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition)
{
    if (_pComposition)
    {
        _pComposition->Release();
        _pComposition = nullptr;
    }
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    bool isCtrlSlash = isCtrl && !isAlt && !isWin && (wParam == VK_OEM_2);

    if (!m_pCEngineClient)
    {
        *pfEaten = FALSE;
        return S_OK;
    }
    // Ctrl+/ always toggles mode regardless of current state
    if (isCtrlSlash)
    {
        *pfEaten = TRUE;
        return S_OK;
    }

    InputMode inputmode = GetInputMode();

    // In Direct mode, only eat Ctrl+/
    if (inputmode == InputMode::Direct)
    {
        *pfEaten = FALSE;
        return S_OK;
    }

    // In Tcode mode, don't capture modifier keys
    if (isCtrl || isAlt || isWin)
    {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Intercept Numbers (0x30-0x39), A-Z (0x41-0x5A), and Punctuation (0xBA-0xDF)
    if ((wParam >= 0x30 && wParam <= 0x39) ||
        (wParam >= 0x41 && wParam <= 0x5A) ||
        (wParam >= 0xBA && wParam <= 0xDF))
    {
        *pfEaten = TRUE;
        return S_OK;
    }

    if (wParam == VK_SPACE)
    {
        if (m_pCEngineClient->GetBuffer().empty())
        {
            *pfEaten = FALSE;
            return S_OK;
        }
        *pfEaten = TRUE;
        return S_OK;
    }
    if (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN)
    {
        if (m_pCEngineClient->HasCandidates())
        {
            *pfEaten = TRUE;
            return S_OK;
        }
        else if (!m_pCEngineClient->IsBufferEmpty())
        {
            if (wParam == VK_LEFT || wParam == VK_RIGHT)
            {
                *pfEaten = TRUE;
                return S_OK;
            }
        }
        *pfEaten = FALSE;
        return S_OK;
    }
    if (wParam == VK_RETURN)
    {
        // Eat Return only if there are candidates to select
        if (m_pCEngineClient && m_pCEngineClient->HasCandidates())
        {
            *pfEaten = TRUE;
            return S_OK;
        }
        *pfEaten = FALSE;
        return S_OK;
    }
    if (wParam == VK_BACK)
    {
        if (m_pCEngineClient->IsLastKeyEmpty() &&
            m_pCEngineClient->IsBufferEmpty() &&
            m_pCEngineClient->IsOutputBufferEmpty())
        {
            *pfEaten = FALSE;
            return S_OK;
        }
        *pfEaten = TRUE;
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}
STDMETHODIMP CTCodeIME::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (!pfEaten)
        return E_INVALIDARG;

    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool isWin = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;
    bool isCtrlSlash = isCtrl && !isAlt && !isWin && (wParam == VK_OEM_2);

    // Read the current state of the engine before executing changes
    InputMode currentMode = GetInputMode();
    if (!m_pCEngineClient)
    {
        *pfEaten = FALSE;
        return S_OK;
    }
    if (isCtrlSlash)
    {
        *pfEaten = TRUE;

        // FIX 1: Compute the explicit target mode beforehand to avoid racing else-if blocks
        InputMode targetMode = (currentMode == InputMode::Tcode) ? InputMode::Direct : InputMode::Tcode;

        // Execute the single synchronized atomic change
        SetInputMode(targetMode);

        if (_pModeButton)
        {
            // Proactively invoke the Language Bar UI update
            _pModeButton->UpdateIcon();
        }

        // FIX 2: Check targetMode instead of currentMode to correctly handle Direct Input entry
        if (targetMode == InputMode::Direct)
        {
            // Cancel active composition when switching back to direct layout
            if (_pComposition)
            {
                CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, L"", L"");
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
            }
            m_pCEngineClient->Reset();
            HideCandidateList();
        }

        return S_OK;
    }

    // Use currentMode for structural processing throughout the remainder of the loop
    if (currentMode == InputMode::Direct)
    {
        *pfEaten = FALSE;
        return S_OK;
    }

    if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_LWIN || wParam == VK_RWIN)
    {
        *pfEaten = FALSE;
        return S_OK;
    }

    // Do not capture other Windows shortcuts with modifier keys (Ctrl/Alt/Win)
    if (isCtrl || isAlt || isWin)
    {
        *pfEaten = FALSE;
        return S_OK;
    }
    if (wParam == VK_BACK)
    {
        // 1. Check cached state to see if the engine currently holds an active typing sequence
        if (m_pCEngineClient->IsLastKeyEmpty() &&
            m_pCEngineClient->IsBufferEmpty() &&
            m_pCEngineClient->IsOutputBufferEmpty())
        {
            *pfEaten = FALSE;
            return S_OK;
        }
        *pfEaten = TRUE;
        m_pCEngineClient->ExecuteCommand(EngineCommand::Backspace);
        std::wstring outputBuffer = m_pCEngineClient->GetOutputBuffer();
        std::wstring buffer = m_pCEngineClient->GetBuffer() + m_pCEngineClient->GetLastKey();
        CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, outputBuffer, buffer);
        if (pEditSession)
        {
            HRESULT hr;
            pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
            pEditSession->Release();
        }
        SyncCandidateList();

        // --- CRITICAL FIX ---
        // If your backend backspace logic just emptied out the composition target completely,
        // do NOT eat the key. Let Windows pass VK_BACK to the app so the document deletes backward.
        if (buffer.empty() && outputBuffer.empty())
        {
            *pfEaten = FALSE;
        }
        else
        {
            *pfEaten = TRUE; // Swallowed only if we are still actively rendering a pre-edit string
        }
        return S_OK;
    }

    // Numbers, A-Z, Punctuation: map hardware keys locally and forward via ExecutePut
    if ((wParam >= 0x30 && wParam <= 0x39) || // '0'-'9'
        (wParam >= 0x41 && wParam <= 0x5A) || // 'A'-'Z'
        (wParam >= 0xBA && wParam <= 0xDF))   // OEM Punctuation block (;,./ etc.)
    {
        *pfEaten = TRUE;
        std::string inputChar = "";

        // 1. Direct local structural translation mapping table
        if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9'))
        {
            inputChar = std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(wParam))));
        }
        else
        {
            switch (wParam)
            {
            case VK_OEM_1:
                inputChar = ";";
                break;
            case VK_OEM_COMMA:
                inputChar = ",";
                break;
            case VK_OEM_PERIOD:
                inputChar = ".";
                break;
            case VK_OEM_2:
                inputChar = "/";
                break;
            default:
                // If it is an unmapped OEM key outside our T-Code layout target, don't eat it
                *pfEaten = FALSE;
                return S_OK;
            }
        }

        if (!inputChar.empty())
        {
            std::wstring committed = L"";
            std::wstring composition = L"";
            BufferStatusResponse res = m_pCEngineClient->ExecutePut(inputChar);
            if (!m_pCEngineClient->IsOutputBufferEmpty())
            {
                CommitResponse commitRes = m_pCEngineClient->Commit();
                committed = m_pCEngineClient->Utf8ToWide(commitRes.output);
            }
            composition = m_pCEngineClient->GetBuffer() + m_pCEngineClient->GetLastKey();
            CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
            if (pEditSession)
            {
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
            }
        }
        // 4. Instantly refresh the menu layout bounds tracking loops
        SyncCandidateList();
        return S_OK;
    }

    // Space: send to Engine only if engine has active buffer characters (to convert/select candidate)
    if (wParam == VK_SPACE)
    {
        // 💡 FIX: Negate the check so it runs when the buffer HAS text (!IsBufferEmpty())
        if (!m_pCEngineClient->IsBufferEmpty())
        {
            *pfEaten = TRUE;
            m_pCEngineClient->ExecuteCommand(EngineCommand::Convert);
            std::wstring committed = m_pCEngineClient->GetOutputBuffer();
            std::wstring composition = m_pCEngineClient->GetBuffer() + m_pCEngineClient->GetLastKey();

            CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
            if (pEditSession)
            {
                HRESULT hr;
                pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                pEditSession->Release();
            }
        }
        else
        {
            // Engine is completely idle — pass Space through so the host app types a normal space character
            *pfEaten = FALSE;
        }

        // 4. Update the candidate layout UI using our clean vector abstraction
        SyncCandidateList();
        return S_OK;
    }

    // Left/Right: send to IPC only if engine has buffer (to change inflex position)
    if (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN)
    {
        if (m_pCEngineClient->HasCandidates())
        {
            *pfEaten = TRUE;
            if (wParam == VK_DOWN || wParam == VK_RIGHT)
            {
                m_selectedIndex++;
            }
            else
            {
                m_selectedIndex--;
            }
            SyncCandidateList();
            return S_OK;
        }
        else if (!m_pCEngineClient->IsBufferEmpty())
        {
            if (wParam == VK_LEFT || wParam == VK_RIGHT)
            {
                *pfEaten = TRUE;

                EngineCommand command = (wParam == VK_LEFT) ? EngineCommand::Left : EngineCommand::Right;
                m_pCEngineClient->ExecuteCommand(command);
                std::wstring composition = m_pCEngineClient->GetBuffer();
                std::wstring committed = m_pCEngineClient->GetOutputBuffer() + m_pCEngineClient->GetLastKey();

                // Update the text view container layout under active TSF context ranges
                CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
                if (pEditSession)
                {
                    HRESULT hr;
                    pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                    pEditSession->Release();
                }
                return S_OK;
            }
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // Return: send Select(index) to fetch selection, then call Commit() to flush output to the app
    if (wParam == VK_RETURN)
    {
        if (m_pCEngineClient && m_pCEngineClient->HasCandidates())
        {
            *pfEaten = TRUE;

            // 1. Send the highlighted item index to the engine
            BufferStatusResponse selectRes = m_pCEngineClient->Select(m_selectedIndex);

            if (selectRes.command_succeed())
            {
                // 💡 2. Execute the Commit transaction directly
                // CommitResponse contains the finalized Kanji text inside its output field
                CommitResponse commitRes = m_pCEngineClient->Commit();

                // 💡 3. Extract the target string using your wide translation helper
                std::wstring committed = m_pCEngineClient->Utf8ToWide(commitRes.output);
                std::wstring composition = L"";

                // 4. Dispatch text updates directly to your active TSF Edit Session context
                // Passing our extracted 'committed' string flushes it straight to the target app document
                CManageCompositionEditSession *pEditSession = new CManageCompositionEditSession(this, pic, committed, composition);
                if (pEditSession)
                {
                    HRESULT hr;
                    pic->RequestEditSession(_tfClientId, pEditSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
                    pEditSession->Release();
                }
            }

            // 5. Reset local visual selection tracking parameters back to zero
            m_selectedIndex = 0;
        }
        else
        {
            // No candidates are active — let Return pass through to process a normal newline event
            *pfEaten = FALSE;
        }

        // 6. Instantly update or hide the candidate suggestion box panel layout
        SyncCandidateList();
        return S_OK;
    }

    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (!pfEaten)
        return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
{
    if (!pfEaten)
        return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten)
{
    if (!pfEaten)
        return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

// IsDirectInputMode: return the current input mode
BOOL CTCodeIME::IsDirectInputMode()
{
    InputMode im = GetInputMode();
    return (im == InputMode::Direct);
}

// ToggleInputMode: toggle the input mode and update the button
void CTCodeIME::ToggleInputMode()
{
    InputMode im = GetInputMode();
    if (im == InputMode::Tcode)
    {
        SetInputMode(InputMode::Direct);
    }
    else
    {
        SetInputMode(InputMode::Tcode);
    }
    if (_pModeButton)
    {
        _pModeButton->UpdateIcon();
    }
}

// UpdateCandidateList: update the candidate list UI element via ITfUIElementMgr
void CTCodeIME::UpdateCandidateList(const std::vector<std::wstring> &candidates, int selectedIndex)
{
    if (!_pThreadMgr || !_pCandidateList)
        return;

    CComPtr<ITfDocumentMgr> pDocMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr)
        return;

    // ==========================================
    // CRUCIAL CHANGE: GetBase instead of GetTop!
    // ==========================================
    CComPtr<ITfContext> pContext;
    if (FAILED(pDocMgr->GetTop(&pContext)) || !pContext)
        return;

    CComPtr<ITfUIElementMgr> pUIElementMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pUIElementMgr))) || !pUIElementMgr)
        return;

    // 1. Update the internal string and selection states first
    _pCandidateList->SetCandidates(candidates, selectedIndex);

    // =========================================================================
    // WINDOW SIZING OPTIMIZATION PLACE HERE
    // =========================================================================
    HWND hCandidateWnd = _pCandidateList->GetHWND();

    if (hCandidateWnd && ::IsWindow(hCandidateWnd))
    {
        // Find out how many items are actually visible on the CURRENT page
        UINT currentPage = 0;
        _pCandidateList->GetCurrentPage(&currentPage);

        size_t startIndex = currentPage * 9; // 9 candidates per page
        size_t totalCandidates = candidates.size();
        int itemCountOnThisPage = 0;

        if (totalCandidates > startIndex)
        {
            itemCountOnThisPage = (int)(totalCandidates - startIndex);
            if (itemCountOnThisPage > 9)
            {
                itemCountOnThisPage = 9; // Clamp to the page maximum
            }
        }

        // Calculate layout size to match the 32px height rows + padding from the custom design
        int finalWindowWidth = 240;
        int finalWindowHeight = (itemCountOnThisPage * 32) + 12; // 32px per row + 12px absolute margin padding

        // Fetch caret tracking position out of TSF so the container sits right beneath it
        CComPtr<ITfContextView> pContextView;
        if (SUCCEEDED(pContext->GetActiveView(&pContextView)) && pContextView)
        {
            ITfRange *pRange = nullptr;
            if (_pComposition && SUCCEEDED(_pComposition->GetRange(&pRange)) && pRange)
            {
                RECT rcText = {0};
                BOOL fClipped = FALSE;

                // Get the exact screen coordinates of your active composition underline
                if (SUCCEEDED(pContextView->GetTextExt(0, pRange, &rcText, &fClipped)))
                {
                    // Update layout metrics: Position underneath caret, plus apply our new optimized dimensions
                    ::SetWindowPos(hCandidateWnd, HWND_TOPMOST,
                                   rcText.left, rcText.bottom,
                                   finalWindowWidth, finalWindowHeight,
                                   SWP_NOACTIVATE);
                }
                pRange->Release();
            }
        }
    }

    if (_dwCandidateListUIElementId == TF_INVALID_UIELEMENTID)
    {
        // First time showing the candidate list — begin the UI element
        BOOL bShow = TRUE;
        HRESULT hr = pUIElementMgr->BeginUIElement(_pCandidateList, &bShow, &_dwCandidateListUIElementId);

        if (SUCCEEDED(hr))
        {
            // 2. CRUCIAL: Only show your physical window if TSF allows it!
            if (bShow)
            {
                _pCandidateList->Show(TRUE);
            }
            else
            {
                _pCandidateList->Show(FALSE); // App is UI-less; hide window, let app pull data
            }

            // 3. Push the initial dataset out to TSF listeners right after creation
            pUIElementMgr->UpdateUIElement(_dwCandidateListUIElementId);
        }
    }
    else
    {
        // Update existing UI element data for both native window and UI-less listeners
        pUIElementMgr->UpdateUIElement(_dwCandidateListUIElementId);

        HWND hCandidateWnd = _pCandidateList->GetHWND();
        if (hCandidateWnd && ::IsWindow(hCandidateWnd))
        {
            ::InvalidateRect(hCandidateWnd, NULL, TRUE); // Clear the stale layout lines
            ::UpdateWindow(hCandidateWnd);               // Force WndProc WM_PAINT to fire immediately
        }
    }
}

void CTCodeIME::SyncCandidateList()
{
    if (!m_pCEngineClient)
        return;

    std::vector<std::wstring> wideCandidates;
    const auto &utf8Candidates = m_pCEngineClient->GetCandidates();

    for (const auto &utf8Str : utf8Candidates)
    {
        if (!utf8Str.empty())
        {
            wideCandidates.push_back(m_pCEngineClient->Utf8ToWide(utf8Str));
        }
    }

    if (wideCandidates.empty())
    {
        HideCandidateList();
    }
    else
    {
        UpdateCandidateList(wideCandidates, m_selectedIndex);
    }
}

void CTCodeIME::HideCandidateList()
{
    // 1. Physically hide your custom Win32 window frame
    if (_pCandidateList)
    {
        _pCandidateList->Show(FALSE);
    }
    // 2. Clear out the TSF UI Element lifecycle sequence
    if (_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID)
    {
        CComPtr<ITfUIElementMgr> pUIElementMgr;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pUIElementMgr))) && pUIElementMgr)
        {
            pUIElementMgr->EndUIElement(_dwCandidateListUIElementId);
        }
        _dwCandidateListUIElementId = TF_INVALID_UIELEMENTID; // Reset the token identifier
    }

    if (_pCandidateList)
    {
        _pCandidateList->SetDocumentMgr(nullptr);
    }
}

// layout sink
STDMETHODIMP CTCodeIME::OnLayoutChange(ITfContext *pContext, TfLayoutCode lcode, ITfContextView *pView)
{
    ::OutputDebugStringW(L"--> TSF Layout Change Fired!");
    // We only care if our candidate list is active and currently visible
    if (_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID && _pCandidateList && _pCandidateList->IsShown())
    {

        // TF_LC_CHANGE indicates the window layout, position, or view boundary shifted
        if (lcode == TF_LC_CHANGE)
        {
            TriggerPositionUpdate();
        }
    }
    return S_OK;
}

void CTCodeIME::InitContextSink(ITfContext *pContext)
{
    if (!pContext)
        return;
    if (_dwTextLayoutSinkCookie != 0)
        return; // Guard against double registration

    // 1. Explicitly fetch the Context View from the current active context
    CComPtr<ITfContextView> pContextView;
    if (SUCCEEDED(pContext->GetActiveView(&pContextView)) && pContextView)
    {

        // 2. Query ITfSource specifically from the VIEW, or directly from the strict Context interface.
        // To be absolutely certain we are bypassing the parent Document Manager wrapper,
        // cast the context explicitly to an ITfSource:
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(pContext->QueryInterface(IID_ITfSource, (void **)&pSource)) && pSource)
        {
            // ============================================================
            // THE FIX: Break the multiple-inheritance pointer ambiguity.
            // Explicitly extract the layout-specific vtable offset
            // and convert it into a generic IUnknown* before passing it out.
            // ============================================================
            ITfTextLayoutSink *pLayoutSink = static_cast<ITfTextLayoutSink *>(this);
            IUnknown *pUnkSink = static_cast<IUnknown *>(pLayoutSink);
            // 3. Advise the layout sink
            HRESULT hr = pSource->AdviseSink(IID_ITfTextLayoutSink, pUnkSink, &_dwTextLayoutSinkCookie);

            if (SUCCEEDED(hr))
            {
                ::OutputDebugStringW(L"--> InitContextSink SUCCESS: Registered ITfTextLayoutSink!\n");
            }
            else
            {
                wchar_t szLog[128];
                swprintf_s(szLog, L"--> InitContextSink FAILED: AdviseSink returned 0x%08X\n", hr);
                ::OutputDebugStringW(szLog);
            }
        }
    }
}
void CTCodeIME::UninitContextSink(ITfContext *pContext)
{
    if (_dwTextLayoutSinkCookie != 0 && pContext)
    {
        CComPtr<ITfSource> pSource;
        if (SUCCEEDED(pContext->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
        {

            HRESULT hr = pSource->UnadviseSink(_dwTextLayoutSinkCookie);

            if (SUCCEEDED(hr))
            {
                ::OutputDebugStringW(L"--> UninitContextSink SUCCESS: Unregistered Cookie\n");
            }
        }
        _dwTextLayoutSinkCookie = 0; // Explicitly reset to zero so it can be reused later
    }
}

void CTCodeIME::TriggerPositionUpdate()
{
    if (!_pThreadMgr || !_pCandidateList)
        return;

    // 1. Get the current active document context manager
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocMgr)) || !pDocMgr)
        return;

    // 2. Extract the top-most active text context layer
    CComPtr<ITfContext> pContext;
    if (FAILED(pDocMgr->GetTop(&pContext)) || !pContext)
        return;

    // 3. Use our lightweight, layout-only session class
    CComPtr<CTrackLayoutEditSession> pLayoutSession;
    pLayoutSession.Attach(new CTrackLayoutEditSession(this, pContext));

    if (pLayoutSession)
    {
        HRESULT hrSession = S_OK;

        // 4. FIXED: Request the lock using 'pLayoutSession' instead of '_pEditSession'
        pContext->RequestEditSession(_tfClientId, pLayoutSession, TF_ES_READ | TF_ES_SYNC, &hrSession);
    }
}

STDMETHODIMP CTCodeIME::OnSetThreadFocus()
{
    if (!_pThreadMgr)
        return S_OK;

    // 1. Fetch the document manager that just stabilized focus
    CComPtr<ITfDocumentMgr> pDocMgr;
    if (SUCCEEDED(_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr)
    {

        CComPtr<ITfContext> pContext;
        if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext)
        {

            // 2. Clear any stale trackers from old windows
            UninitContextSink(pContext);

            // 3. Bind your layout tracker directly to the fresh context layer
            InitContextSink(pContext);
        }
    }
    return S_OK;
}

STDMETHODIMP CTCodeIME::OnKillThreadFocus()
{
    // No specific action required, focus handoff cleans itself up
    return S_OK;
}
