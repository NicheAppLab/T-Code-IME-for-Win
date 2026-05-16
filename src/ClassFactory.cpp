#include "ClassFactory.h"
#include "Globals.h"
#include "TCodeIME.h"

CClassFactory::CClassFactory() : _cRef(1) {
    DllAddRef();
}

CClassFactory::~CClassFactory() {
    DllRelease();
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() {
    return InterlockedIncrement(&_cRef);
}

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    ULONG cRef = InterlockedDecrement(&_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

STDMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    CTCodeIME* pIME = new CTCodeIME();
    if (pIME == nullptr) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = pIME->QueryInterface(riid, ppvObj);
    pIME->Release();
    return hr;
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        DllAddRef();
    } else {
        DllRelease();
    }
    return S_OK;
}
