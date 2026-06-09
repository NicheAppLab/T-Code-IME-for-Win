#pragma once

#include <windows.h>
#include <msctf.h>

class CTCodeIME; // forward declaration

class CTCodeBrandingButton : public ITfLangBarItemButton,
                             public ITfSource
{
public:
    explicit CTCodeBrandingButton(CTCodeIME* pOwner);
    ~CTCodeBrandingButton();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo);
    STDMETHODIMP GetStatus(DWORD* pdwStatus);
    STDMETHODIMP Show(BOOL fShow);
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip);

    // ITfLangBarItemButton
    STDMETHODIMP GetIcon(HICON* phIcon);
    STDMETHODIMP GetText(BSTR* pbstrText);
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea);
    STDMETHODIMP InitMenu(ITfMenu* pMenu);
    STDMETHODIMP OnMenuSelect(UINT wID);

    // ITfSource
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie);
    STDMETHODIMP UnadviseSink(DWORD dwCookie);

private:
    LONG _cRef;
    CTCodeIME* _pOwner;
    ITfLangBarItemSink* _pSink;
};