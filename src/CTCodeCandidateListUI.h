#pragma once

#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>

// TF_CLUIE_* flags for ITfCandidateListUIElement::GetUpdatedFlags
// These may not be defined in older Windows SDK headers.
#ifndef TF_CLUIE_DOCUMENTMGR
#define TF_CLUIE_DOCUMENTMGR   0x00000001
#define TF_CLUIE_COUNT         0x00000002
#define TF_CLUIE_SELECTION     0x00000004
#define TF_CLUIE_STRING        0x00000008
#define TF_CLUIE_PAGEINDEX     0x00000010
#define TF_CLUIE_CURRENTPAGE   0x00000020
#endif

// Forward declaration
class CTCodeIME;

class CTCodeCandidateListUI : public ITfCandidateListUIElement
{
public:
    CTCodeCandidateListUI(CTCodeIME* pIME);
    ~CTCodeCandidateListUI();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfUIElement
    STDMETHODIMP GetDescription(BSTR* pbstrDescription);
    STDMETHODIMP GetGUID(GUID* pguid);
    STDMETHODIMP IsShown(BOOL* pbShow);
    // Add to your CTCodeCandidateListUI class declaration/implementation:
    BOOL IsShown() const;
    STDMETHODIMP Show(BOOL bShow);

    // ITfCandidateListUIElement
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr** ppdim);
    STDMETHODIMP GetCount(UINT* puCount);
    STDMETHODIMP GetSelection(UINT* puIndex);
    STDMETHODIMP GetString(UINT uIndex, BSTR* pbstr);
    STDMETHODIMP GetPageIndex(UINT* pPageIndex, UINT uPageSize, UINT* puPageCnt);
    STDMETHODIMP SetPageIndex(UINT* pPageIndex, UINT uPageCnt);
    STDMETHODIMP GetCurrentPage(UINT* puPage);
    STDMETHODIMP GetUpdatedFlags(DWORD* pdwFlags);

    // Public helpers for CTCodeIME
    void SetCandidates(const std::vector<std::wstring>& candidates, int selectedIndex);
    void ClearCandidates();
    bool HasCandidates() const { return !_candidates.empty(); }

    HWND GetHWND() const;

    // Helper to initialize the window
    void CreateCandidateWindow(HWND hParentWnd = NULL);
    void SetDocumentMgr(ITfDocumentMgr* pDocMgr);

    // A static Window Procedure to handle messages for this window
    static LRESULT CALLBACK CandidateWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    LONG _cRef;
    CTCodeIME* _pIME;
    std::vector<std::wstring> _candidates;
    int _selectedIndex;
    bool _isShown;
    DWORD _updatedFlags;
    UINT _currentPage;
    std::vector<UINT> _pageIndex;
    HWND _hCandidateWnd;

    static const UINT CANDIDATES_PER_PAGE = 9;
    ITfDocumentMgr* _pCachedDocMgr = nullptr;
};