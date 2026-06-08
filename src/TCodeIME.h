#pragma once
#include <atlbase.h>
#include <windows.h>
#include <msctf.h>
#include <memory>
#include "IPCClient.h"
#include "helper.h"
class CTCodeModeButton;

enum class InputMode;
class CTCodeIME : public ITfTextInputProcessorEx,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfCompartmentEventSink
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
    BOOL IsDirectInputMode();
    void ToggleInputMode(); // Renamed from ToggleDirectInputMode

    // ITfKeyEventSink methods
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten);
    HRESULT SetInputMode(InputMode mode);
    InputMode GetInputMode();
    BOOL _IsKeyboardDisabled();
    HRESULT _GetCompartment(REFGUID rguid, InputMode& outMode);
    HRESULT _SetCompartment(REFGUID rguid, InputMode mode);

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition);

    // ITfCompartmentEventSink methods
    STDMETHODIMP OnChange(REFGUID rguid);

    // Internal helpers
    BOOL _InitCompartmentEventSink();
    void _UninitCompartmentEventSink();
    void _KeyboardOpenCloseChanged();
    void _KeyboardInputConversionChanged();

private:
    LONG _cRef;
    ITfThreadMgr* _pThreadMgr;
    TfClientId _tfClientId;
    tcode::IPCClient* _pIPCClient;
    ITfComposition* _pComposition;
    CComPtr<CTCodeModeButton> _pModeButton;
    DWORD _dwCompartmentEventSinkOpenCloseCookie;
    DWORD _dwCompartmentEventSinkInputmodeConversionCookie;
    friend class CManageCompositionEditSession;

};


