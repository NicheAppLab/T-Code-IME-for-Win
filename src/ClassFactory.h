#pragma once
#include <windows.h>
#include <unknwn.h>

class CClassFactory : public IClassFactory {
public:
    CClassFactory();
    ~CClassFactory();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IClassFactory methods
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj);
    STDMETHODIMP LockServer(BOOL fLock);

private:
    LONG _cRef;
};
