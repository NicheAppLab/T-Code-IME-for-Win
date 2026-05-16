#include "ProcessManager.h"
#include <vector>
#include <iostream>

namespace tcode {

ProcessManager::ProcessManager(const std::wstring& jarPath) 
    : _jarPath(jarPath), _isStarted(false) {
    _pi = { 0 };
    std::filesystem::path p(jarPath);
    _workingDir = p.parent_path().wstring();
}

ProcessManager::~ProcessManager() {
    Stop();
}

bool ProcessManager::Start() {
    if (IsRunning()) return true;

    std::wstring cmdLine = L"java -jar \"" + _jarPath + L"\"";
    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    STARTUPINFOW si = { sizeof(si) };
    
    BOOL bSuccess = CreateProcessW(
        nullptr,
        cmdLineBuf.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        _workingDir.c_str(),
        &si,
        &_pi
    );

    if (bSuccess) {
        _isStarted = true;
        return true;
    }

    return false;
}

void ProcessManager::Stop() {
    if (IsRunning()) {
        TerminateProcess(_pi.hProcess, 0);
        CloseHandle(_pi.hProcess);
        CloseHandle(_pi.hThread);
        _pi = { 0 };
    }
    _isStarted = false;
}

bool ProcessManager::IsRunning() {
    if (!_isStarted) return false;

    DWORD exitCode;
    if (GetExitCodeProcess(_pi.hProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }

    return false;
}

void ProcessManager::Monitor() {
    if (!IsRunning() && _isStarted) {
        std::wcout << L"Process lost. Restarting..." << std::endl;
        Start();
    }
}

} // namespace tcode
