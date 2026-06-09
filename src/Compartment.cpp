#include "TCodeIME.h"
#include <windows.h>
#include <combaseapi.h>
#include <atlbase.h>
#include "helper.h"

// --- Open/Close compartment helpers ---

HRESULT CTCodeIME::_SetOpenClose(OpenClose state) {
    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;
    var.lVal = (state == OpenClose::Open) ? 1 : 0;

    return pCompartment->SetValue(_tfClientId, &var);
}

HRESULT CTCodeIME::_GetOpenClose(OpenClose& outState) {
    outState = OpenClose::Closed;

    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    CComVariant var;
    if (FAILED(pCompartment->GetValue(&var))) {
        return E_FAIL;
    }

    if (V_VT(&var) != VT_I4) {
        return E_INVALIDARG;
    }

    outState = (V_I4(&var) != 0) ? OpenClose::Open : OpenClose::Closed;
    return S_OK;
}

// --- Conversion mode compartment helpers ---

HRESULT CTCodeIME::_SetConversionMode(InputMode mode) {
    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;

    if (mode == InputMode::Tcode) {
        var.lVal = TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_FULLSHAPE | TF_CONVERSIONMODE_ROMAN;
    } else {
        var.lVal = TF_CONVERSIONMODE_ALPHANUMERIC;
    }

    return pCompartment->SetValue(_tfClientId, &var);
}

HRESULT CTCodeIME::_GetConversionMode(InputMode& outMode) {
    outMode = InputMode::Direct;

    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    CComVariant var;
    if (FAILED(pCompartment->GetValue(&var))) {
        return E_FAIL;
    }

    if (V_VT(&var) != VT_I4) {
        return E_INVALIDARG;
    }

    long val = V_I4(&var);
    outMode = (val & TF_CONVERSIONMODE_NATIVE) ? InputMode::Tcode : InputMode::Direct;
    return S_OK;
}

// --- Legacy _SetCompartment / _GetCompartment (kept for backward compat, delegates to new methods) ---

HRESULT CTCodeIME::_SetCompartment(REFGUID rguid, InputMode mode) {
    if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
        return _SetOpenClose((mode == InputMode::Tcode) ? OpenClose::Open : OpenClose::Open);
    }
    else if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
        return _SetConversionMode(mode);
    }
    return E_INVALIDARG;
}

HRESULT CTCodeIME::_GetCompartment(REFGUID rguid, InputMode& outMode) {
    if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
        OpenClose state;
        HRESULT hr = _GetOpenClose(state);
        outMode = (state == OpenClose::Open) ? InputMode::Direct : InputMode::Direct; // both map to Direct
        return hr;
    }
    else if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
        return _GetConversionMode(outMode);
    }
    return E_INVALIDARG;
}

// Helper to quickly extract a boolean status from a context compartment
BOOL IsCompartmentTrue(ITfCompartmentMgr* pCompartmentMgr, REFGUID rguid) {
    CComPtr<ITfCompartment> pCompartment;
    if (SUCCEEDED(pCompartmentMgr->GetCompartment(rguid, &pCompartment)) && pCompartment) {
        CComVariant var;
        if (SUCCEEDED(pCompartment->GetValue(&var)) && V_VT(&var) == VT_I4) {
            return (V_I4(&var) != 0) ? TRUE : FALSE;
        }
    }
    return FALSE;
}

BOOL CTCodeIME::_IsKeyboardDisabled() {
    if (_pThreadMgr == nullptr) return TRUE;

    CComPtr<ITfDocumentMgr> pDocumentMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocumentMgr)) || !pDocumentMgr) {
        return TRUE;
    }

    CComPtr<ITfContext> pContext;
    if (FAILED(pDocumentMgr->GetTop(&pContext)) || !pContext) {
        return TRUE;
    }

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(pContext->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return TRUE;
    }

    if (IsCompartmentTrue(pCompartmentMgr, GUID_COMPARTMENT_KEYBOARD_DISABLED)) {
        return TRUE;
    }

    if (IsCompartmentTrue(pCompartmentMgr, GUID_COMPARTMENT_EMPTYCONTEXT)) {
        return TRUE;
    }

    return FALSE;
}

InputMode CTCodeIME::GetInputMode() {
    InputMode mode = InputMode::Direct;
    _GetConversionMode(mode);
    return mode;
}

HRESULT CTCodeIME::SetInputMode(InputMode mode) {
    HRESULT hr = _SetOpenClose(OpenClose::Open);
    if (FAILED(hr)) return hr;
    return _SetConversionMode(mode);
}