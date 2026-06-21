#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <iostream>
#include "resource.h"

#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001

// Global variables
DWORD g_childPid = 0;
HANDLE g_childProcessHandle = nullptr;
HANDLE g_hJob = nullptr;
HWND g_hWnd = nullptr;
NOTIFYICONDATAW g_nid = {};

void KillProcessTree(DWORD parentPid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(hSnapshot, &pe32)) {
        std::vector<DWORD> children;
        do {
            if (pe32.th32ParentProcessID == parentPid) {
                children.push_back(pe32.th32ProcessID);
            }
        } while (Process32NextW(hSnapshot, &pe32));

        for (DWORD childPid : children) {
            KillProcessTree(childPid);
        }
    }
    CloseHandle(hSnapshot);

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, parentPid);
    if (hProcess != nullptr) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
}

void KillJavaProcess() {
    if (g_childPid != 0) {
        KillProcessTree(g_childPid);
        g_childPid = 0;
    }
    if (g_childProcessHandle != nullptr) {
        CloseHandle(g_childProcessHandle);
        g_childProcessHandle = nullptr;
    }
}

std::wstring GetModuleDir() {
    wchar_t baseDir[MAX_PATH];
    GetModuleFileNameW(nullptr, baseDir, MAX_PATH);
    std::wstring path = baseDir;
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash);
    }
    return L"";
}

std::wstring GetEngineDir() {
    std::wstring path = GetModuleDir();
    while (!path.empty()) {
        std::wstring potential = path + L"\\engine";
        DWORD attrib = GetFileAttributesW(potential.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return potential;
        }
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash == std::wstring::npos) break;
        path = path.substr(0, lastSlash);
    }
    return L"";
}

HICON LoadTrayIcon(HINSTANCE hInstance) {
    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (hIcon != nullptr) {
        return hIcon;
    }

    std::wstring moduleDir = GetModuleDir();
    std::wstring possiblePaths[] = {
        moduleDir + L"\\icons\\icon.ico",
        moduleDir + L"\\icon.ico"
    };

    for (const auto& path : possiblePaths) {
        hIcon = (HICON)LoadImageW(
            nullptr,
            path.c_str(),
            IMAGE_ICON,
            16, 16,
            LR_LOADFROMFILE
        );
        if (hIcon != nullptr) {
            return hIcon;
        }
    }
    return LoadIconW(nullptr, IDI_APPLICATION);
}

void SetupJobObject() {
    g_hJob = CreateJobObjectW(nullptr, L"TCodeEngineJob");
    if (g_hJob != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        ZeroMemory(&info, sizeof(info));
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }
}

bool StartEngineProcess(const std::wstring& scriptPath, const std::wstring& workingDir, const std::wstring& engineDir) {
    std::wstring bundledJreDir = engineDir + L"\\jre";
    std::wstring jreJavaExe = bundledJreDir + L"\\bin\\java.exe";
    bool hasBundledJre = (GetFileAttributesW(jreJavaExe.c_str()) != INVALID_FILE_ATTRIBUTES);

    wchar_t serverPort[32] = L"57001";
    GetEnvironmentVariableW(L"TCODE_SERVER_PORT", serverPort, 32);

    wchar_t dictDir[MAX_PATH] = L"%APPDATA%\\tcode-server\\dictionary";
    GetEnvironmentVariableW(L"TCODE_DICT_DIR", dictDir, MAX_PATH);
    wchar_t resolvedDictDir[MAX_PATH];
    ExpandEnvironmentStringsW(dictDir, resolvedDictDir, MAX_PATH);

    SetEnvironmentVariableW(L"TCODE_SERVER_PORT", serverPort);
    SetEnvironmentVariableW(L"TCODE_DICT_DIR", resolvedDictDir);

    std::wstring workDir = workingDir.empty() ? GetModuleDir() : workingDir;
    std::wstring javaOpts = L"-Duser.home=\"" + workDir + 
                            L"\" -Dtcode-server.server.port=" + serverPort + 
                            L" -Dtcode-server.dict-dir=\"" + resolvedDictDir + L"\"";
    SetEnvironmentVariableW(L"JAVA_OPTS", javaOpts.c_str());

    if (hasBundledJre) {
        SetEnvironmentVariableW(L"BUNDLED_JVM", bundledJreDir.c_str());
        SetEnvironmentVariableW(L"JAVA_HOME", bundledJreDir.c_str());
        
        wchar_t oldPath[32767];
        GetEnvironmentVariableW(L"PATH", oldPath, 32767);
        std::wstring newPath = bundledJreDir + L"\\bin;" + oldPath;
        SetEnvironmentVariableW(L"PATH", newPath.c_str());
    }

    std::wstring cmdLine = L"cmd.exe /c \"" + scriptPath + L"\"";

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );

    if (success) {
        g_childPid = pi.dwProcessId;
        g_childProcessHandle = pi.hProcess;
        CloseHandle(pi.hThread);
        
        if (g_hJob != nullptr) {
            AssignProcessToJobObject(g_hJob, pi.hProcess);
        }
        return true;
    }
    return false;
}

void EnsureEngineRunning() {
    std::wstring engineDir = GetEngineDir();
    std::wstring scriptPath;

    if (engineDir.empty()) {
        std::wstring fallbackScript = GetModuleDir() + L"\\tcodeserver.bat";
        if (GetFileAttributesW(fallbackScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
            StartEngineProcess(fallbackScript, GetModuleDir(), L"");
        }
        return;
    }

    scriptPath = engineDir + L"\\bin\\tcodeserver.bat";
    if (GetFileAttributesW(scriptPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring altScript = engineDir + L"\\tcodeserver.bat";
        if (GetFileAttributesW(altScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
            scriptPath = altScript;
        } else {
            std::wstring fallbackScript = GetModuleDir() + L"\\tcodeserver.bat";
            if (GetFileAttributesW(fallbackScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
                scriptPath = fallbackScript;
            } else {
                return;
            }
        }
    }
    StartEngineProcess(scriptPath, engineDir, engineDir);
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    KillJavaProcess();
    return FALSE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, nullptr);
                    DestroyMenu(hMenu);
                }
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_EXIT) {
                DestroyWindow(hWnd);
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            KillJavaProcess();
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Setup Job Object
    SetupJobObject();

    // Register Exit Handlers
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Start Engine
    EnsureEngineRunning();

    // Setup Invisible Window for Tray Events
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"TCodeProxyWindowClass";
    RegisterClassExW(&wcex);

    g_hWnd = CreateWindowExW(0, L"TCodeProxyWindowClass", L"T-Code IME Proxy", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (!g_hWnd) return 1;

    // Setup Tray Icon
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadTrayIcon(hInstance);
    wcscpy_s(g_nid.szTip, L"T-Code IME Proxy");
    wcscpy_s(g_nid.szInfoTitle, L"T-Code IME Proxy");
    wcscpy_s(g_nid.szInfo, L"T-Code IME is running in the tray.");
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 2500;

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hJob != nullptr) {
        CloseHandle(g_hJob);
    }

    return (int)msg.wParam;
}
