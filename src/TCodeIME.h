#pragma once
#include <atlbase.h>
#include <windows.h>
#include <msctf.h>
#include <memory>
#include <vector>
#include <string>
#include "IPCClient.h"
#include "helper.h"
class CTCodeModeButton;
class CTCodeCandidateListUI;

enum class InputMode;
class CTCodeIME : public ITfTextInputProcessorEx,
                  public ITfKeyEventSink,
                  public ITfCompositionSink,
                  public ITfCompartmentEventSink,
                  public ITfTextLayoutSink,
                  public ITfThreadFocusSink
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

    // Open/Close compartment
    HRESULT _SetOpenClose(OpenClose state);
    HRESULT _GetOpenClose(OpenClose& outState);

    // Conversion mode compartment
    HRESULT _SetConversionMode(InputMode mode);
    HRESULT _GetConversionMode(InputMode& outMode);

    // Legacy helpers (kept for backward compat)
    HRESULT _GetCompartment(REFGUID rguid, InputMode& outMode);
    HRESULT _SetCompartment(REFGUID rguid, InputMode mode);

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition);

    // ITfCompartmentEventSink methods
    STDMETHODIMP OnChange(REFGUID rguid);

    // Candidate list management
    void UpdateCandidateList(const std::vector<std::wstring>& candidates, int selectedIndex);
    void HideCandidateList();
    void SyncCandidateListFromIPC();

    // Text layout sink
    STDMETHODIMP OnLayoutChange(ITfContext* pContext, TfLayoutCode lcode, ITfContextView* pView) override;

    // Thread Focus Sink
    STDMETHODIMP OnSetThreadFocus() override;
    STDMETHODIMP OnKillThreadFocus() override;

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
    CComPtr<CTCodeCandidateListUI> _pCandidateList;
    DWORD _dwCompartmentEventSinkOpenCloseCookie;
    DWORD _dwCompartmentEventSinkInputmodeConversionCookie;
    DWORD _dwCandidateListUIElementId;

    void CTCodeIME::MoveCandidateWindowToCaret();

    friend class CManageCompositionEditSession;
    friend class CTCodeCandidateListUI;
    friend class CTrackLayoutEditSession;

    DWORD _dwTextLayoutSinkCookie = 0;
    DWORD _dwThreadFocusSinkCookie = 0;
    void CTCodeIME::UninitContextSink(ITfContext* pContext);
    void CTCodeIME::InitContextSink(ITfContext* pContext);
    void TriggerPositionUpdate();
};


