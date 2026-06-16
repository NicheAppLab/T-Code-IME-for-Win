#pragma once
#include <msctf.h>
#include <atlbase.h> // Required for CComPtr
#include <string>
#include "TCodeIME.h"
// Forward declarations to ensure visibility


class CTrackLayoutEditSession : public ITfEditSession {
public:
    CTrackLayoutEditSession(CTCodeIME* pIME, ITfContext* pContext);
    virtual ~CTrackLayoutEditSession();

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    LONG _cRef;
    CTCodeIME* _pIME;
    ITfContext* _pContext;
};


class CManageCompositionEditSession : public ITfEditSession {
public:
    CManageCompositionEditSession(CTCodeIME* pIME, ITfContext* pContext, const std::wstring& committed, const std::wstring& composition);
    ~CManageCompositionEditSession();
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
    LONG _cRef;
    CTCodeIME* _pIME;
    ITfContext* _pContext;
    std::wstring _committed;
    std::wstring _composition;
};
