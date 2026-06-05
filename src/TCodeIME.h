#pragma once
#include <windows.h>
#include <msctf.h>
#include "IPCClient.h"
class CTCodeModeButton;

class CTCodeIME : public ITfTextInputProcessorEx,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfSource
{
public:
    CTCodeIME();
    ~CTCodeIME();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor methods
    STDMETHODIMP Activate(ITfThreadMgr *ptim, TfClientId tid);
    STDMETHODIMP Deactivate();

    // ITfTextInputProcessorEx methods
    STDMETHODIMP ActivateEx(ITfThreadMgr *ptim, TfClientId tid, DWORD dwFlags);
    BOOL IsDirectInputMode() const;
    void ToggleDirectInputMode();

    // ITfKeyEventSink methods
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten);

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition);

    // ITfSource methods
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown *pUnk, DWORD *pdwCookie);
    STDMETHODIMP UnadviseSink(DWORD dwCookie);

private:
    LONG _cRef;
    ITfThreadMgr* _pThreadMgr;
    TfClientId _tfClientId;
    tcode::IPCClient* _pIPCClient;
    ITfComposition* _pComposition;
    BOOL _fDirectInputMode;
    ITfLangBarItemSink* _pLangBarItemSink;
    CTCodeModeButton* _pModeButton;
    friend class CManageCompositionEditSession;
};
