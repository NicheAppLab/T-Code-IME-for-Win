#include "CTCodeCandidateListUI.h"
#include "TCodeIME.h"
#include "Globals.h"
#include <atlbase.h>

CTCodeCandidateListUI::CTCodeCandidateListUI(CTCodeIME* pIME)
    : _cRef(1)
    , _pIME(pIME)
    , _selectedIndex(-1)
    , _isShown(false)
    , _updatedFlags(0)
    , _currentPage(0)
    , _hCandidateWnd(NULL)
{
    // Register the window class once during instantiation
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CTCodeCandidateListUI::CandidateWndProc;
    wc.hInstance = GetModuleHandle(NULL); // Or your TIP dll instance
    wc.lpszClassName = L"TCodeCandidateWindowClassName";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassExW(&wc);
    
    // Create the actual window handle
    CreateCandidateWindow();
    DllAddRef();
}

CTCodeCandidateListUI::~CTCodeCandidateListUI()
{
    if (_hCandidateWnd) {
        DestroyWindow(_hCandidateWnd);
    }
    UnregisterClassW(L"TCodeCandidateWindowClassName", GetModuleHandle(NULL));
    DllRelease();
}

void CTCodeCandidateListUI::CreateCandidateWindow(HWND hParentWnd) {
    _hCandidateWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, // CRUCIAL: Do not activate!
        L"TCodeCandidateWindowClassName",
        L"",
        WS_POPUP | WS_BORDER, // Borderless popup window
        0, 0, 200, 300,       // Initial position and size (change this dynamically later)
        hParentWnd,
        NULL,
        GetModuleHandle(NULL),
        this                  // Pass 'this' so WndProc can access candidates
    );
}
LRESULT CALLBACK CTCodeCandidateListUI::CandidateWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Retrieve our class pointer from the window memory slot
    CTCodeCandidateListUI* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        pThis = reinterpret_cast<CTCodeCandidateListUI*>(lpcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<CTCodeCandidateListUI*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (pThis) {
                int yOffset = 5;
                // Render each string stored in your candidate vector onto this HWND surface
                for (size_t i = 0; i < pThis->_candidates.size(); ++i) {
                    std::wstring line = std::to_wstring(i + 1) + L". " + pThis->_candidates[i];
                    
                    // Highlight the item matching the active selection index
                    if (static_cast<int>(i) == pThis->_selectedIndex) {
                        SetTextColor(hdc, RGB(255, 255, 255));
                        SetBkColor(hdc, RGB(0, 120, 215)); // Highlight Blue
                    } else {
                        SetTextColor(hdc, RGB(0, 0, 0));
                        SetBkColor(hdc, RGB(255, 255, 255));
                    }
                    
                    TextOutW(hdc, 5, yOffset, line.c_str(), static_cast<int>(line.length()));
                    yOffset += 20; // Move down for next candidate row
                }
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// IUnknown
STDMETHODIMP CTCodeCandidateListUI::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfUIElement) ||
        IsEqualIID(riid, IID_ITfCandidateListUIElement))
    {
        *ppvObj = static_cast<ITfCandidateListUIElement*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTCodeCandidateListUI::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CTCodeCandidateListUI::Release()
{
    ULONG res = InterlockedDecrement(&_cRef);
    if (res == 0) delete this;
    return res;
}

// ITfUIElement
STDMETHODIMP CTCodeCandidateListUI::GetDescription(BSTR* pbstrDescription)
{
    if (!pbstrDescription) return E_INVALIDARG;
    *pbstrDescription = SysAllocString(L"T-Code Candidate List");
    return (*pbstrDescription) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeCandidateListUI::GetGUID(GUID* pguid)
{
    if (!pguid) return E_INVALIDARG;
    static const GUID GUID_TCODE_CANDIDATE_LIST =
        { 0x3b8a3b8a, 0x1b8a, 0x4b8a, { 0x8a, 0x3b, 0x8a, 0x3b, 0x8a, 0x3b, 0x8a, 0x3b } };
    *pguid = GUID_TCODE_CANDIDATE_LIST;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::IsShown(BOOL* pbShow)
{
    if (!pbShow) return E_INVALIDARG;
    *pbShow = _isShown ? TRUE : FALSE;
    return S_OK;
}
// Add to your CTCodeCandidateListUI class declaration/implementation:
BOOL CTCodeCandidateListUI::IsShown() const { return _isShown; }

STDMETHODIMP CTCodeCandidateListUI::Show(BOOL bShow) { 
    _isShown = (bShow != FALSE); 

    // Assuming you store your native candidate window handle in _hCandidateWnd
    if (_hCandidateWnd) {
        if (_isShown) {
            RECT rcWnd;
            GetWindowRect(_hCandidateWnd, &rcWnd);
            if(rcWnd.left != 0 && rcWnd.top != 0){
                // CRUCIAL: Use SW_SHOWNA so your candidate window doesn't steal keyboard focus.
                // If it steals focus, the active app loses focus, and TSF kills the candidate list instantly.
                ::ShowWindow(_hCandidateWnd, SW_SHOWNA);
                
                // Force a window repaint so your candidates get painted via WM_PAINT
                ::InvalidateRect(_hCandidateWnd, NULL, TRUE);
                ::UpdateWindow(_hCandidateWnd);
            }
        } else {
            ::ShowWindow(_hCandidateWnd, SW_HIDE);
        }
    }
    return S_OK; 
}

// ITfCandidateListUIElement
STDMETHODIMP CTCodeCandidateListUI::GetDocumentMgr(ITfDocumentMgr** ppdim)
{
    if (!ppdim) return E_INVALIDARG;
    *ppdim = nullptr;
    // Return the current document manager from the thread manager
    if (_pIME && _pIME->_pThreadMgr)
    {
        return _pIME->_pThreadMgr->GetFocus(ppdim);
    }
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetCount(UINT* puCount)
{
    if (!puCount) return E_INVALIDARG;
    *puCount = (UINT)_candidates.size();
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetSelection(UINT* puIndex)
{
    if (!puIndex) return E_INVALIDARG;
    *puIndex = (_selectedIndex >= 0) ? (UINT)_selectedIndex : 0;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetString(UINT uIndex, BSTR* pbstr)
{
    if (!pbstr) return E_INVALIDARG;
    *pbstr = nullptr;

    if (uIndex >= (UINT)_candidates.size())
        return E_INVALIDARG;

    *pbstr = SysAllocString(_candidates[uIndex].c_str());
    return (*pbstr) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeCandidateListUI::GetPageIndex(UINT* pPageIndex, UINT uPageSize, UINT* puPageCnt)
{
    if (!puPageCnt) return E_INVALIDARG;

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

STDMETHODIMP CTCodeCandidateListUI::SetPageIndex(UINT* pPageIndex, UINT uPageCnt)
{
    if (!pPageIndex) return E_INVALIDARG;
    _pageIndex.assign(pPageIndex, pPageIndex + uPageCnt);
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetCurrentPage(UINT* puPage)
{
    if (!puPage) return E_INVALIDARG;
    *puPage = _currentPage;
    return S_OK;
}

STDMETHODIMP CTCodeCandidateListUI::GetUpdatedFlags(DWORD* pdwFlags)
{
    if (!pdwFlags) return E_INVALIDARG;
    *pdwFlags = _updatedFlags;
    _updatedFlags = 0; // Clear after reading
    return S_OK;
}

// Public helpers
void CTCodeCandidateListUI::SetCandidates(const std::vector<std::wstring>& candidates, int selectedIndex)
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

HWND CTCodeCandidateListUI::GetHWND() const {
    return _hCandidateWnd;
}