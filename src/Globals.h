#pragma once
#include <windows.h>

extern HINSTANCE g_hInst;
extern LONG g_cRefDll;

extern const CLSID CLSID_TCodeIME;
extern const GUID GUID_Profile;

DEFINE_GUID(GUID_LBI_INPUTMODE, 0x6be4fa07, 0x5dd6, 0x4d51, 0x91, 0x5d, 0x2b, 0x73, 0x46, 0x14, 0x3d, 0x8d);

void DllAddRef();
void DllRelease();
