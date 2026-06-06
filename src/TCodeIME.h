#pragma once
#include <windows.h>
#include <msctf.h>
#include "IPCClient.h"
class CTCodeModeButton;
class CTBrandButton;

class CTCodeIME : public ITfTextInputProcessorEx,
                  public ITfKeyEventSink,
                  public ITfCompositionSink
                  // ITfSource removed – brand button replaced by CTCodeModeButton
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
    void ToggleInputMode(); // Renamed from ToggleDirectInputMode

    // ITfKeyEventSink methods
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten);

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition);

private:
    LONG _cRef;
    ITfThreadMgr* _pThreadMgr;
    TfClientId _tfClientId;
    tcode::IPCClient* _pIPCClient;
    ITfComposition* _pComposition;
    BOOL _fDirectInputMode;
    CTCodeModeButton* _pModeButton;
    CTBrandButton* _pBrandButton; // Added declaration for brand button
    friend class CManageCompositionEditSession;
};

