#include "TCodeIME.h"
#include <atlbase.h>

STDAPI CTCodeIME::OnChange(REFGUID rguid)
{
	if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE))
	{
		_KeyboardOpenCloseChanged();
	}
	else if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION))
	{
		_KeyboardInputConversionChanged();
	}

	return S_OK;
}

BOOL CTCodeIME::_InitCompartmentEventSink()
{
	if (_pThreadMgr == nullptr) return FALSE;

	CComPtr<ITfCompartmentMgr> pCompartmentMgr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) && pCompartmentMgr)
	{
		{
			CComPtr<ITfCompartment> pCompartment;
			if (SUCCEEDED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pCompartment)) && pCompartment)
			{
				CComPtr<ITfSource> pSource;
				if (SUCCEEDED(pCompartment->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
				{
					if (FAILED(pSource->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &_dwCompartmentEventSinkOpenCloseCookie)))
					{
						_dwCompartmentEventSinkOpenCloseCookie = TF_INVALID_COOKIE;
					}
				}
			}
		}
		{
			CComPtr<ITfCompartment> pCompartment;
			if (SUCCEEDED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &pCompartment)) && pCompartment)
			{
				CComPtr<ITfSource> pSource;
				if (SUCCEEDED(pCompartment->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
				{
					if (FAILED(pSource->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &_dwCompartmentEventSinkInputmodeConversionCookie)))
					{
						_dwCompartmentEventSinkInputmodeConversionCookie = TF_INVALID_COOKIE;
					}
				}
			}
		}
	}

	return (_dwCompartmentEventSinkOpenCloseCookie != TF_INVALID_COOKIE && _dwCompartmentEventSinkInputmodeConversionCookie != TF_INVALID_COOKIE);
}

void CTCodeIME::_UninitCompartmentEventSink()
{
	if (_pThreadMgr == nullptr) return;

	CComPtr<ITfCompartmentMgr> pCompartmentMgr;
	if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_PPV_ARGS(&pCompartmentMgr))) && pCompartmentMgr)
	{
		{
			CComPtr<ITfCompartment> pCompartment;
			if (SUCCEEDED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pCompartment)) && pCompartment)
			{
				CComPtr<ITfSource> pSource;
				if (SUCCEEDED(pCompartment->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
				{
					pSource->UnadviseSink(_dwCompartmentEventSinkOpenCloseCookie);
				}
			}
		}
		{
			CComPtr<ITfCompartment> pCompartment;
			if (SUCCEEDED(pCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &pCompartment)) && pCompartment)
			{
				CComPtr<ITfSource> pSource;
				if (SUCCEEDED(pCompartment->QueryInterface(IID_PPV_ARGS(&pSource))) && pSource)
				{
					pSource->UnadviseSink(_dwCompartmentEventSinkInputmodeConversionCookie);
				}
			}
		}
	}
}