#include "TCodeIME.h"
#include <windows.h>
#include <combaseapi.h>
#include <atlbase.h>
#include "helper.h"

HRESULT CTCodeIME::_SetCompartment(REFGUID rguid, InputMode mode) {
    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(rguid, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    VARIANT var;
    VariantInit(&var);
    var.vt = VT_I4;

    // --- FIX: Map the high-level enum to exact low-level Windows TSF expectations ---
    if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
        // 0 = Closed/Disabled. 1 = Open/Enabled.
        // Critical: Never pass any other state number here.
        var.lVal = (mode == InputMode::Closed) ? 0 : 1;
    } 
    else if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
        if (mode == InputMode::Tcode) {
            // Restore your exact multi-flag composition layout mask
            var.lVal = TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_FULLSHAPE | TF_CONVERSIONMODE_ROMAN;
        } else {
            // Standard pass-through alphanumeric mask
            var.lVal = TF_CONVERSIONMODE_ALPHANUMERIC; 
        }
    } 
    else {
        // Fallback for any other custom compartments
        var.lVal = static_cast<long>(mode);
    }

    return pCompartment->SetValue(_tfClientId, &var);
}

HRESULT CTCodeIME::_GetCompartment(REFGUID rguid, InputMode& outMode) {
    // Set a safe fallback value immediately in case of failure
    outMode = InputMode::Unknown; 

    if (_pThreadMgr == nullptr) return E_UNEXPECTED;

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return E_FAIL;
    }

    CComPtr<ITfCompartment> pCompartment;
    if (FAILED(pCompartmentMgr->GetCompartment(rguid, &pCompartment)) || !pCompartment) {
        return E_FAIL;
    }

    CComVariant var;
    if (FAILED(pCompartment->GetValue(&var))) {
        return E_FAIL;
    }

    // Verify the variant holds a 4-byte integer before casting
    if (V_VT(&var) != VT_I4) {
        return E_INVALIDARG; 
    }

    long val = V_I4(&var);

    // --- FIX: Map according to what Windows actually stores in each specific GUID ---
    if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
        // 0 means Closed. Anything else means it is Open (mapped here as Direct)
        outMode = (val == 0) ? InputMode::Closed : InputMode::Direct;
    } 
    else if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
        // T-Code sets TF_CONVERSIONMODE_NATIVE. Check if that bit is turned on.
        if ((val & TF_CONVERSIONMODE_NATIVE) != 0) {
            outMode = InputMode::Tcode;
        } else {
            outMode = InputMode::Direct;
        }
    }
    else {
        // Fallback for custom, non-system compartments that use your explicit enum numbers
        switch (val) {
            case 0: outMode = InputMode::Closed; break;
            case 1: outMode = InputMode::Direct; break;
            case 2: outMode = InputMode::Tcode;  break;
            default: outMode = InputMode::Unknown; break;
        }
    }

    return S_OK;
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
    return FALSE; // Default to false if compartment doesn't exist or is 0
}
BOOL CTCodeIME::_IsKeyboardDisabled() {
    if (_pThreadMgr == nullptr) return TRUE;

    // Rule 1: If there is no active document manager, input is disabled
    CComPtr<ITfDocumentMgr> pDocumentMgr;
    if (FAILED(_pThreadMgr->GetFocus(&pDocumentMgr)) || !pDocumentMgr) {
        return TRUE;
    }

    // Rule 2: If there is no active text context, input is disabled
    CComPtr<ITfContext> pContext;
    if (FAILED(pDocumentMgr->GetTop(&pContext)) || !pContext) {
        return TRUE;
    }

    CComPtr<ITfCompartmentMgr> pCompartmentMgr;
    if (FAILED(pContext->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) || !pCompartmentMgr) {
        return TRUE;
    }

    // Rule 3: Check if the keyboard is explicitly disabled by the system/app
    if (IsCompartmentTrue(pCompartmentMgr, GUID_COMPARTMENT_KEYBOARD_DISABLED)) {
        return TRUE;
    }

    // Rule 4: Check if the context is completely empty (e.g., password fields or uneditable fields)
    if (IsCompartmentTrue(pCompartmentMgr, GUID_COMPARTMENT_EMPTYCONTEXT)) {
        return TRUE;
    }

    // If all checks pass, the keyboard is enabled and ready to accept input
    return FALSE;
}

InputMode CTCodeIME::GetInputMode() {
    InputMode openCloseMode = InputMode::Closed;

    // Step 1: Check if the IME is open or closed
    if (FAILED(_GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, openCloseMode))) {
        return InputMode::Closed; // Safe fallback if the compartment doesn't exist
    }

    // If it's closed, we can return immediately
    if (openCloseMode == InputMode::Closed) {
        return InputMode::Closed;
    }

    // Step 2: The IME is open, so query the conversion mode compartment
    InputMode conversionMode = InputMode::Direct;
    if (SUCCEEDED(_GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, conversionMode))) {
        // Return the exact active typing mode (Direct or Tcode)
        if (conversionMode == InputMode::Tcode || conversionMode == InputMode::Direct) {
            return conversionMode;
        }
    }

    // Default safety fallback if open but conversion flags are uninitialized
    return InputMode::Direct; 
}

HRESULT CTCodeIME::SetInputMode(InputMode mode) {
    if (mode == InputMode::Unknown) {
        return E_INVALIDARG;
    }

    // Update the open/close state status cleanly
    HRESULT hr = _SetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, mode);
    if (FAILED(hr)) return hr;

    // Update and sync the conversion configuration masks securely
    hr = _SetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, mode);
    
    return hr;
}