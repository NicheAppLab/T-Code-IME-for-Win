#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>
#include "PipeServer.h"
#include "ProcessManager.h"
#include "EngineClient.h"

int main() {
    std::wcout << L"T-Code IME Proxy Service Starting..." << std::endl;

    // 1. Resolve Engine Path
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
    std::filesystem::path exePath(szPath);
    
    // In dev: project_root/build/Debug/TCodeProxy.exe -> project_root/engine/
    std::filesystem::path engineDir = exePath.parent_path().parent_path().parent_path() / L"engine";
    std::filesystem::path jarPath = engineDir / L"tcodeserver_3.jar";

    if (!std::filesystem::exists(jarPath)) {
        std::wcerr << L"Error: Could not find " << jarPath << std::endl;
        return 1;
    }

    // 2. Initialize Components
    tcode::ProcessManager pm(jarPath.wstring());
    CEngineClient engineClient;
    if (!engineClient.Connect("localhost:8080")) {
        std::wcerr << L"Error: Failed to connect to gRPC server." << std::endl;
        return 1;
    }
    tcode::PipeServer pipeServer(&engineClient);

    // 3. Start Everything
    if (!pm.Start()) {
        std::wcerr << L"Error: Failed to start Java engine." << std::endl;
        return 1;
    }

    if (!pipeServer.Start()) {
        std::wcerr << L"Error: Failed to start Pipe Server." << std::endl;
        return 1;
    }

    std::wcout << L"Proxy Service is running. Press Ctrl+C to stop." << std::endl;

    // 4. Main Loop (Service heartbeat)
    while (true) {
        pm.Monitor();
        Sleep(5000);
    }

    return 0;
}
