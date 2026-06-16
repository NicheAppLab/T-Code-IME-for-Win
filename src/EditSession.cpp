#include <msctf.h>
#include <atlbase.h> // Required for CComPtr
#include <string>
#include "EditSession.h"
#include "TCodeIME.h"
#include "CTCodeCandidateListUI.h"
// Forward declarations to ensure visibility
class CTCodeIME;
class CTCodeCandidateListUI;

CTrackLayoutEditSession::CTrackLayoutEditSession(CTCodeIME *pIME, ITfContext *pContext)
    : _cRef(1), _pIME(pIME), _pContext(pContext)
{
    if (_pContext)
        _pContext->AddRef();
    if (reinterpret_cast<IUnknown *>(_pIME))
    {
        reinterpret_cast<IUnknown *>(_pIME)->AddRef();
    }
}

// Destructor releases the held references cleanly
CTrackLayoutEditSession::~CTrackLayoutEditSession()
{
    if (_pContext)
        _pContext->Release();
    if (reinterpret_cast<IUnknown *>(_pIME))
    {
        reinterpret_cast<IUnknown *>(_pIME)->Release();
    }
}

// --- IUnknown Interface Implementation ---
STDMETHODIMP CTrackLayoutEditSession::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj)
        return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
    {
        *ppvObj = static_cast<ITfEditSession *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CTrackLayoutEditSession::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG)
CTrackLayoutEditSession::Release()
{
    ULONG res = InterlockedDecrement(&_cRef);
    if (res == 0)
    {
        delete this;
    }
    return res;
}

// --- ITfEditSession Core Processing Loop ---
STDMETHODIMP CTrackLayoutEditSession::DoEditSession(TfEditCookie ec)
{
    // Safe validation checks to guarantee the target IME objects exist
    if (!_pIME || !_pContext)
        return S_OK;

    // Accessing variables across classes via public pointers/getters
    // Replace with your exact class member layout variable naming conventions if different
    if (!_pIME->_pComposition || !_pIME->_pCandidateList)
        return S_OK;

    CComPtr<ITfContextView> pContextView;
    if (SUCCEEDED(_pContext->GetActiveView(&pContextView)) && pContextView)
    {

        CComPtr<ITfRange> pRange;
        // Try getting the composition range first
        if (FAILED(_pIME->_pComposition->GetRange(&pRange)) || !pRange) {
            // FALLBACK: If composition fails, grab the active selection caret range
            TF_SELECTION sel;
            ULONG cFetched = 0;
            if (_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) == S_OK && cFetched > 0) {
                pRange.Attach(sel.range); // Safely attach the raw selection range
            }
        }
        // Fetch the layout bounds matching the active cursor composition string span
        if (pRange)
        {

            RECT rcText = {0};
            BOOL fClipped = FALSE;

            // Query the exact, instantaneous monitor coordinates of the typing layout line
            HRESULT hrLayout = pContextView->GetTextExt(ec, pRange, &rcText, &fClipped);

            // If layout calculations succeed, smoothly adjust the floating window location
            if (SUCCEEDED(hrLayout))
            {
                CTCodeCandidateListUI *pConcreteList = static_cast<CTCodeCandidateListUI *>(_pIME->_pCandidateList.p);
                if (pConcreteList)
                {
                    // 2. Fetch the Win32 window handle safely
                    HWND hCandidateWnd = pConcreteList->GetHWND();

                    if (hCandidateWnd && ::IsWindow(hCandidateWnd))
                    {
                        // SWP_NOCOPYBITS prevents desktop trail artifacts during heavy layout animations
                        // SWP_NOSIZE maintains whatever structural boundaries you give your UI list
                        ::SetWindowPos(hCandidateWnd, HWND_TOPMOST,
                                       rcText.left, rcText.bottom,
                                       0, 0,
                                       SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOCOPYBITS);

                        // Extra insurance check to force an absolute redrawing cycle onto your HWND
                        if (_pIME->_pCandidateList->IsShown())
                        {
                            ::InvalidateRect(hCandidateWnd, nullptr, TRUE);
                        }
                    }
                }
            }
            // Handle the case where the application layout frame composition state drops transient errors
            else if (hrLayout == TS_E_NOLAYOUT)
            {
                // Do nothing on this layout pass frame loop cycle.
                // The next drag message hook broadcast will update it safely.
            }
        }
    }
    return S_OK;
}

CManageCompositionEditSession::CManageCompositionEditSession(CTCodeIME *pIME, ITfContext *pContext, const std::wstring &committed, const std::wstring &composition)
    : _cRef(1), _pIME(pIME), _pContext(pContext), _committed(committed), _composition(composition)
{
    if (_pContext)
        _pContext->AddRef();
    if (_pIME)
        _pIME->AddRef(); // Maintain reference parity for the IME instance
}

CManageCompositionEditSession::~CManageCompositionEditSession()
{
    if (_pContext)
        _pContext->Release();
    if (_pIME)
        _pIME->Release();
}

STDMETHODIMP CManageCompositionEditSession::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj)
        return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
    {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CManageCompositionEditSession::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG)
CManageCompositionEditSession::Release()
{
    ULONG res = InterlockedDecrement(&_cRef);
    if (res == 0)
        delete this;
    return res;
}

STDMETHODIMP CManageCompositionEditSession::DoEditSession(TfEditCookie ec)
{
    // 1. Handle Committed Text
    if (!_committed.empty())
    {
        ITfRange *pRange = nullptr;
        if (_pIME->_pComposition)
        {
            _pIME->_pComposition->GetRange(&pRange);
        }
        else
        {
            TF_SELECTION sel;
            ULONG cFetched;
            if (_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) == S_OK && cFetched > 0)
            {
                pRange = sel.range;
            }
        }

        if (pRange)
        {
            pRange->SetText(ec, 0, _committed.c_str(), (ULONG)_committed.length());
            if (_pIME->_pComposition)
            {
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
    if (!_composition.empty())
    {
        if (!_pIME->_pComposition)
        {
            ITfContextComposition *pContextComposition;
            if (_pContext->QueryInterface(IID_ITfContextComposition, (void **)&pContextComposition) == S_OK)
            {
                TF_SELECTION sel;
                ULONG cFetched;
                if (_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) == S_OK && cFetched > 0)
                {
                    pContextComposition->StartComposition(ec, sel.range, _pIME, &_pIME->_pComposition);
                    sel.range->Release();
                }
                pContextComposition->Release();
            }
        }

        if (_pIME->_pComposition)
        {
            ITfRange *pRange;
            if (_pIME->_pComposition->GetRange(&pRange) == S_OK)
            {
                pRange->SetText(ec, 0, _composition.c_str(), (ULONG)_composition.length());
                pRange->Release();
            }
        }
    }
    else if (_pIME->_pComposition)
    {
        ITfRange *pRange;
        if (_pIME->_pComposition->GetRange(&pRange) == S_OK)
        {
            pRange->SetText(ec, 0, nullptr, 0);
            pRange->Release();
        }
        _pIME->_pComposition->EndComposition(ec);
        _pIME->_pComposition->Release();
        _pIME->_pComposition = nullptr;
    }

    // 3. Update Positioning Layout Controls
    if (_pIME->_pComposition && _pIME->_pCandidateList && _pIME->_dwCandidateListUIElementId != TF_INVALID_UIELEMENTID)
    {
        CComPtr<ITfContextView> pContextView;
        if (SUCCEEDED(_pContext->GetActiveView(&pContextView)) && pContextView)
        {
            ITfRange *pRange = nullptr;
            if (SUCCEEDED(_pIME->_pComposition->GetRange(&pRange)) && pRange)
            {
                RECT rcText = {0};
                BOOL fClipped = FALSE;

                if (SUCCEEDED(pContextView->GetTextExt(ec, pRange, &rcText, &fClipped)))
                {
                    HWND hCandidateWnd = _pIME->_pCandidateList->GetHWND();
                    if (hCandidateWnd)
                    {
                        // 1. Move the window to the correct location while it is still HIDDEN.
                        // Notice SWP_HIDEWINDOW is NOT used; we use standard flags without SWP_SHOWWINDOW.
                        ::SetWindowPos(hCandidateWnd, HWND_TOPMOST,
                                       rcText.left, rcText.bottom,
                                       0, 0,
                                       SWP_NOSIZE | SWP_NOACTIVATE);

                        // 2. Now that it is safely sitting under the caret, reveal it to the user.
                        // This bypasses the (0,0) rendering lifecycle entirely.
                        if (_pIME->_pCandidateList->IsShown())
                        { // Make sure TSF wants it visible
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