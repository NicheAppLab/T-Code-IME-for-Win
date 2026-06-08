#include "helper.h"
#include <oleauto.h> // Required for handling COM types safely

InputMode CastVariantToInputMode(const VARIANT& var) {
    // 1. Verify the VARIANT actually holds a 4-byte integer (VT_I4)
    if (var.vt != VT_I4) {
        return InputMode::Unknown;
    }

    // 2. Map the raw integer value directly to your enum constants
    switch (var.lVal) {
        case static_cast<long>(InputMode::Closed):
            return InputMode::Closed;

        case static_cast<long>(InputMode::Direct):
            return InputMode::Direct;

        case static_cast<long>(InputMode::Tcode):
            return InputMode::Tcode;

        default:
            return InputMode::Unknown; // Value is an integer, but not one of our modes
    }
}
void CastInputModeToVariant(InputMode mode, VARIANT& outVar) {
    VariantInit(&outVar);
    outVar.vt = VT_I4;
    outVar.lVal = static_cast<long>(mode);
}