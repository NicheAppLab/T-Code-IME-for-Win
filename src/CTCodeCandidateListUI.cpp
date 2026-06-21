#include "CTCodeCandidateListUI.h"
#include "TCodeIME.h"
#include "Globals.h"
#include <atlbase.h>

// A safe, file-level static variable to ensure we don't double-register
static BOOL g_bClassRegistered = FALSE;

CTCodeCandidateListUI::CTCodeCandidateListUI(CTCodeIME *pIME)
    : _cRef(1), _pIME(pIME), _selectedIndex(-1), _isShown(false), _updatedFlags(0), _currentPage(0), _hCandidateWnd(NULL)
{
    // Fix: Only register if it hasn't been registered yet this session
    if (!g_bClassRegistered)
    {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = CTCodeCandidateListUI::CandidateWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"TCodeCandidateWindowClassName";
        wc.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH); // Pure white background

        if (RegisterClassExW(&wc))
        {
            g_bClassRegistered = TRUE;
        }
    }

    DllAddRef(); // Keep your original COM tracking intact!
}

CTCodeCandidateListUI::~CTCodeCandidateListUI()
{
    if (_hCandidateWnd && ::IsWindow(_hCandidateWnd))
    {
        ::DestroyWindow(_hCandidateWnd);
        _hCandidateWnd = NULL;
    }

    // CRUCIAL FIX: DO NOT call UnregisterClassW here.
    // Leaving the template class alive in memory prevents the 2-minute freeze,
    // and removing it from here prevents the DLL from crashing.

    DllRelease(); // Keep your original COM tracking intact!
}

void CTCodeCandidateListUI::CreateCandidateWindow(HWND hParentWnd)
{
    _hCandidateWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, // CRUCIAL: Do not activate!
        L"TCodeCandidateWindowClassName",
        L"",
        WS_POPUP | WS_BORDER, // Borderless popup window
        0, 0, 200, 300,       // Initial position and size (change this dynamically later)
        hParentWnd,
        NULL,
        GetModuleHandle(NULL),
        this // Pass 'this' so WndProc can access candidates
    );
}
LRESULT CALLBACK CTCodeCandidateListUI::CandidateWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Retrieve our class pointer from the window memory slot
    CTCodeCandidateListUI *pThis = nullptr;
    if (uMsg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = reinterpret_cast<CTCodeCandidateListUI *>(lpcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CTCodeCandidateListUI *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (pThis)
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int width = rcClient.right - rcClient.left;
            int height = rcClient.bottom - rcClient.top;

            // 1. Double-Buffering: Create an off-screen surface to prevent flicker
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

            // 2. Draw modern background and crisp border
            HBRUSH hBgBrush = CreateSolidBrush(RGB(250, 250, 250)); // Off-white flat color
            FillRect(hdcMem, &rcClient, hBgBrush);
            DeleteObject(hBgBrush);

            HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(218, 220, 224)); // Subtle gray border
            HPEN hOldPen = (HPEN)SelectObject(hdcMem, hBorderPen);
            MoveToEx(hdcMem, 0, 0, NULL);
            LineTo(hdcMem, width - 1, 0);
            LineTo(hdcMem, width - 1, height - 1);
            LineTo(hdcMem, 0, height - 1);
            LineTo(hdcMem, 0, 0);
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hBorderPen);

            // 3. Configure elegant typography
            // Use Segoe UI or system UI font instead of raw raster font
            HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

            // Render configuration
            SetBkMode(hdcMem, TRANSPARENT);
            int itemHeight = 32;                         // Taller rows provide a cleaner, touch-friendly UI
            int verticalPadding = 6;                     // Alignment margins
            size_t startIndex = pThis->_currentPage * 9; // 9 Candidates Per Page
            size_t endIndex = startIndex + 9;

            if (endIndex > pThis->_candidates.size())
            {
                endIndex = pThis->_candidates.size();
            }

            // 4. Render the candidate list rows
            for (size_t i = startIndex; i < endIndex; ++i)
            {
                size_t displayIndex = (i - startIndex) + 1;
                std::wstring indexStr = std::to_wstring(displayIndex) + L" ";
                std::wstring candidateStr = pThis->_candidates[i];

                // Define bounding box for the entire row item
                RECT rcRow = {1, verticalPadding, width - 1, verticalPadding + itemHeight};

                if (static_cast<int>(i) == pThis->_selectedIndex)
                {
                    // Highlighted Item: Modern deep blue layout with crisp white text
                    HBRUSH hSelBrush = CreateSolidBrush(RGB(0, 103, 192));
                    FillRect(hdcMem, &rcRow, hSelBrush);
                    DeleteObject(hSelBrush);

                    SetTextColor(hdcMem, RGB(255, 255, 255));
                }
                else
                {
                    // Alternating row background for enhanced readability
                    if ((i - startIndex) % 2 == 1)
                    {
                        HBRUSH hAltBrush = CreateSolidBrush(RGB(243, 243, 243));
                        FillRect(hdcMem, &rcRow, hAltBrush);
                        DeleteObject(hAltBrush);
                    }
                    SetTextColor(hdcMem, RGB(32, 33, 36)); // Modern dark gray text
                }

                // Draw index token (e.g., "1 ")
                RECT rcIndex = {rcRow.left + 12, rcRow.top, rcRow.left + 35, rcRow.bottom};
                DrawTextW(hdcMem, indexStr.c_str(), -1, &rcIndex, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // Draw candidate character string
                RECT rcText = {rcIndex.right, rcRow.top, rcRow.right - 12, rcRow.bottom};
                DrawTextW(hdcMem, candidateStr.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                verticalPadding += itemHeight;
            }

            // 5. Commit off-screen frame buffer back to screen
            BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);

            // Cleanup resources
            SelectObject(hdcMem, hOldFont);
            DeleteObject(hFont);
            SelectObject(hdcMem, hOldBm);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// IUnknown
STDMETHODIMP CTCodeCandidateListUI::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj)
        return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfUIElement) ||
        IsEqualIID(riid, IID_ITfCandidateListUIElement))
    {
        *ppvObj = static_cast<ITfCandidateListUIElement *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG)
CTCodeCandidateListUI::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG)
CTCodeCandidateListUI::Release()
{
    ULONG res = InterlockedDecrement(&_cRef);
    if (res == 0)
        delete this;
    return res;
}

// ITfUIElement
STDMETHODIMP CTCodeCandidateListUI::GetDescription(BSTR *pbstrDescription)
{
    if (!pbstrDescription)
        return E_INVALIDARG;
    *pbstrDescription = SysAllocString(L"T-Code Candidate List");
    return (*pbstrDescription) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeCandidateListUI::GetGUID(GUID *pguid)
{
    if (!pguid)
        return E_INVALIDARG;
    static const GUID GUID_TCODE_CANDIDATE_LIST =
        {0x3b8a3b8a, 0x1b8a, 0x4b8a, {0x8a, 0x3b, 0x8a, 0x3b, 0x8a, 0x3b, 0x8a, 0x3b}};
    *pguid = GUID_TCODE_CANDIDATE_LIST;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::IsShown(BOOL *pbShow)
{
    if (!pbShow)
        return E_INVALIDARG;
    *pbShow = _isShown ? TRUE : FALSE;
    return S_OK;
}
// Add to your CTCodeCandidateListUI class declaration/implementation:
BOOL CTCodeCandidateListUI::IsShown() const { return _isShown; }

STDMETHODIMP CTCodeCandidateListUI::Show(BOOL bShow)
{
    _isShown = (bShow != FALSE);

    if (_isShown)
    {
        // LAZY INITIALIZATION: Build the window only when it's time to display it
        if (!_hCandidateWnd || !::IsWindow(_hCandidateWnd))
        {

            // FIX 1: Explicitly declare the variable first!
            HWND hActiveAppWnd = NULL;

            // 1. Fetch the absolute active application window handle via TSF
            if (_pIME && _pIME->_pThreadMgr)
            {
                CComPtr<ITfDocumentMgr> pDocMgr;
                if (SUCCEEDED(_pIME->_pThreadMgr->GetFocus(&pDocMgr)) && pDocMgr)
                {
                    CComPtr<ITfContext> pContext;
                    if (SUCCEEDED(pDocMgr->GetTop(&pContext)) && pContext)
                    {
                        CComPtr<ITfContextView> pContextView;
                        if (SUCCEEDED(pContext->GetActiveView(&pContextView)) && pContextView)
                        {
                            pContextView->GetWnd(&hActiveAppWnd); // Get Notepad's HWND
                        }
                    }
                }
            }

            // 2. Fallback if TSF is still initializing its focus map
            if (!hActiveAppWnd)
            {
                hActiveAppWnd = ::GetForegroundWindow();
            }

            // 3. Create the window with a solid, verified parent handle
            CreateCandidateWindow(hActiveAppWnd);

            if (_pIME)
            {
                _pIME->TriggerPositionUpdate();
            }
        }

        // Display the window safely
        if (_hCandidateWnd)
        {
            // FIX 2: Remove the coordinate (0,0) gatekeeper check completely!
            // We force a standard, focus-safe display pass right out of the box.
            ::ShowWindow(_hCandidateWnd, SW_SHOWNA);
            ::InvalidateRect(_hCandidateWnd, NULL, TRUE);
            ::UpdateWindow(_hCandidateWnd);
        }
    }
    else
    {
        if (_hCandidateWnd && ::IsWindow(_hCandidateWnd))
        {
            ::ShowWindow(_hCandidateWnd, SW_HIDE);
        }
    }
    return S_OK;
}

// ITfCandidateListUIElement
STDMETHODIMP CTCodeCandidateListUI::GetDocumentMgr(ITfDocumentMgr **ppdim)
{
    if (!ppdim)
        return E_INVALIDARG;
    *ppdim = nullptr;

    if (_pCachedDocMgr)
    {
        *ppdim = _pCachedDocMgr;
        (*ppdim)->AddRef(); // Must increment COM ref count when handing out pointers
        return S_OK;
    }

    // Fallback just in case, but we want to avoid relying on this
    if (_pIME && _pIME->_pThreadMgr)
    {
        return _pIME->_pThreadMgr->GetFocus(ppdim);
    }

    return E_FAIL; // Tell TSF clearly if no context exists yet
}
void CTCodeCandidateListUI::SetDocumentMgr(ITfDocumentMgr *pDocMgr)
{
    if (_pCachedDocMgr)
    {
        _pCachedDocMgr->Release();
    }
    _pCachedDocMgr = pDocMgr;
    if (_pCachedDocMgr)
    {
        _pCachedDocMgr->AddRef();
    }
}
STDMETHODIMP CTCodeCandidateListUI::GetCount(UINT *puCount)
{
    if (!puCount)
        return E_INVALIDARG;
    *puCount = (UINT)_candidates.size();
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetSelection(UINT *puIndex)
{
    if (!puIndex)
        return E_INVALIDARG;
    *puIndex = (_selectedIndex >= 0) ? (UINT)_selectedIndex : 0;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetString(UINT uIndex, BSTR *pbstr)
{
    if (!pbstr)
        return E_INVALIDARG;
    *pbstr = nullptr;

    if (uIndex >= (UINT)_candidates.size())
        return E_INVALIDARG;

    *pbstr = SysAllocString(_candidates[uIndex].c_str());
    return (*pbstr) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeCandidateListUI::GetPageIndex(UINT *pPageIndex, UINT uPageSize, UINT *puPageCnt)
{
    if (!puPageCnt)
        return E_INVALIDARG;

    UINT pageCount = (UINT)_pageIndex.size();
    *puPageCnt = pageCount;

    if (pPageIndex && uPageSize >= pageCount)
    {
        for (UINT i = 0; i < pageCount; i++)
        {
            pPageIndex[i] = _pageIndex[i];
        }
    }
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::SetPageIndex(UINT *pPageIndex, UINT uPageCnt)
{
    if (!pPageIndex)
        return E_INVALIDARG;
    _pageIndex.assign(pPageIndex, pPageIndex + uPageCnt);
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetCurrentPage(UINT *puPage)
{
    if (!puPage)
        return E_INVALIDARG;
    *puPage = _currentPage;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetUpdatedFlags(DWORD *pdwFlags)
{
    if (!pdwFlags)
        return E_INVALIDARG;
    *pdwFlags = _updatedFlags;
    _updatedFlags = 0; // Clear after reading
    return S_OK;
}

// Public helpers
void CTCodeCandidateListUI::SetCandidates(const std::vector<std::wstring> &candidates, int selectedIndex)
{
    _candidates = candidates;
    _selectedIndex = selectedIndex;

    // Rebuild page index
    _pageIndex.clear();
    for (UINT i = 0; i < (UINT)_candidates.size(); i += CANDIDATES_PER_PAGE)
    {
        _pageIndex.push_back(i);
    }

    // Calculate current page
    if (_selectedIndex >= 0)
    {
        _currentPage = (UINT)_selectedIndex / CANDIDATES_PER_PAGE;
    }

    // Set update flags
    _updatedFlags = TF_CLUIE_COUNT | TF_CLUIE_STRING | TF_CLUIE_SELECTION;
    if (!_pageIndex.empty())
    {
        _updatedFlags |= TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    }
}

void CTCodeCandidateListUI::ClearCandidates()
{
    _candidates.clear();
    _selectedIndex = -1;
    _pageIndex.clear();
    _currentPage = 0;
    _updatedFlags = TF_CLUIE_COUNT;
}

HWND CTCodeCandidateListUI::GetHWND() const
{
    return _hCandidateWnd;
}

HINSTANCE GetCurrentModuleInstance()
{
    HINSTANCE hInst = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetCurrentModuleInstance,
        &hInst);
    return hInst;
}

void RegisterGlobalCandidateClass()
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    // The compiler knows exactly what this is now because it's inside this file!
    wc.lpfnWndProc = CTCodeCandidateListUI::CandidateWndProc;
    wc.hInstance = GetCurrentModuleInstance();
    wc.lpszClassName = L"TCodeCandidateWindowClassName";
    wc.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH);

    RegisterClassExW(&wc);
}

void UnregisterGlobalCandidateClass()
{
    UnregisterClassW(L"TCodeCandidateWindowClassName", GetCurrentModuleInstance());
}