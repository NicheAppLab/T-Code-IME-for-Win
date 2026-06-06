#pragma once

#include <windows.h>
#include <msctf.h>

class CTCodeIME; // forward declaration of owning IME

class CTBrandButton : public ITfLangBarItemButton,
                       public ITfSource {
public:
    explicit CTBrandButton(CTCodeIME* pOwner);
    ~CTBrandButton();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfLangBarItem methods
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo);
    STDMETHODIMP GetStatus(DWORD* pdwStatus);
    STDMETHODIMP Show(BOOL fShow);
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip);
    STDMETHODIMP GetIcon(HICON* phIcon);
    STDMETHODIMP GetText(BSTR* pbstrText);

    // ITfLangBarItemButton methods
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea);
    STDMETHODIMP InitMenu(ITfMenu* pMenu);
    STDMETHODIMP OnMenuSelect(UINT wID);

    // ITfSource methods
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* pUnk, DWORD* pdwCookie);
    STDMETHODIMP UnadviseSink(DWORD dwCookie);

private:
    LONG _cRef;
    CTCodeIME* _pOwner; // not owned
    ITfLangBarItemSink* _pSink;
};