#include "CTCodeBrandingButton.h"
#include "TCodeIME.h"
#include "resource.h"
#include "Globals.h"
#include <ctfutb.h>

#ifndef TF_LBI_STYLE_SHOWNINTRAY
#define TF_LBI_STYLE_SHOWNINTRAY   0x00000002
#endif
#ifndef TF_LBI_STYLE_SHOWINDESC
#define TF_LBI_STYLE_SHOWINDESC    0x00000010
#endif
#ifndef TF_LBI_STYLE_BTN_BUTTON
#define TF_LBI_STYLE_BTN_BUTTON    0x00000000
#endif

CTCodeBrandingButton::CTCodeBrandingButton(CTCodeIME* pOwner)
    : _cRef(1), _pOwner(pOwner), _pSink(nullptr)
{
    if (_pOwner) {
        _pOwner->AddRef();
    }
}

CTCodeBrandingButton::~CTCodeBrandingButton()
{
    if (_pOwner) {
        _pOwner->Release();
    }
    if (_pSink) {
        _pSink->Release();
    }
}

// IUnknown
STDMETHODIMP CTCodeBrandingButton::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfLangBarItem)) {
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

STDMETHODIMP_(ULONG) CTCodeBrandingButton::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CTCodeBrandingButton::Release()
{
    ULONG ref = InterlockedDecrement(&_cRef);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ITfLangBarItem
STDMETHODIMP CTCodeBrandingButton::GetInfo(TF_LANGBARITEMINFO* pInfo)
{
    if (pInfo == nullptr) return E_INVALIDARG;

    pInfo->clsidService = CLSID_TCodeIME;
    pInfo->guidItem = GUID_LBI_Branding;
    pInfo->ulSort = 0;
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON;

    wcscpy_s(pInfo->szDescription, ARRAYSIZE(pInfo->szDescription), L"T-Code IME");
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::GetStatus(DWORD* pdwStatus)
{
    if (pdwStatus == nullptr) return E_INVALIDARG;
    *pdwStatus = 0;
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::Show(BOOL fShow)
{
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::GetTooltipString(BSTR* pbstrToolTip)
{
    if (pbstrToolTip == nullptr) return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"T-Code IME");
    return (*pbstrToolTip != nullptr) ? S_OK : E_OUTOFMEMORY;
}

// ITfLangBarItemButton
STDMETHODIMP CTCodeBrandingButton::GetIcon(HICON* phIcon)
{
    if (phIcon == nullptr) return E_INVALIDARG;
    *phIcon = LoadIconW(g_hInst, MAKEINTRESOURCE(IDI_ICON_BRANDING));
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::GetText(BSTR* pbstrText)
{
    if (pbstrText == nullptr) return E_INVALIDARG;
    *pbstrText = SysAllocString(L"T-Code");
    return (*pbstrText != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CTCodeBrandingButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea)
{
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::InitMenu(ITfMenu* pMenu)
{
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::OnMenuSelect(UINT wID)
{
    return S_OK;
}

// ITfSource
STDMETHODIMP CTCodeBrandingButton::AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie)
{
    if (pUnk == nullptr || pdwCookie == nullptr) return E_INVALIDARG;
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_FAIL;
    if (_pSink != nullptr) return E_FAIL;

    if (pUnk->QueryInterface(IID_ITfLangBarItemSink, (void**)&_pSink) == S_OK) {
        *pdwCookie = 1;
    }
    else {
        return E_NOINTERFACE;
    }
    return S_OK;
}

STDMETHODIMP CTCodeBrandingButton::UnadviseSink(DWORD dwCookie)
{
    if (dwCookie != 1) return E_INVALIDARG;
    if (_pSink) {
        _pSink->Release();
        _pSink = nullptr;
    }
    return S_OK;
}