#pragma once
#include <windows.h>

extern HINSTANCE g_hInst;
extern LONG g_cRefDll;

extern const CLSID CLSID_TCodeIME;
extern const GUID GUID_Profile;
extern const GUID GUID_DisplayAttribute;

void DllAddRef();
void DllRelease();
