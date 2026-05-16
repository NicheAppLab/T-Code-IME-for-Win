#pragma once

#include <windows.h>
#include <string>
#include "IPCClient.h" // Reuse the struct definitions
#include "EngineClient.h"

namespace tcode {

class PipeServer {
public:
    PipeServer(CEngineClient* engineClient);
    ~PipeServer();

    bool Start();
    void Stop();

private:
    static DWORD WINAPI ClientThread(LPVOID lpParam);
    void HandleClient(HANDLE hPipe);

    CEngineClient* _pEngineClient;
    HANDLE _hListenThread;
    bool _running;
    const std::wstring _pipeName = L"\\\\.\\pipe\\TCodeEngineProxy";
};

} // namespace tcode
