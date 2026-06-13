#include "PipeServer.h"
#include <iostream>

namespace tcode {

PipeServer::PipeServer(CEngineClient* engineClient) 
    : _pEngineClient(engineClient), _hListenThread(NULL), _running(false) {}

PipeServer::~PipeServer() {
    Stop();
}

bool PipeServer::Start() {
    _running = true;
    _hListenThread = CreateThread(NULL, 0, ClientThread, this, 0, NULL);
    return _hListenThread != NULL;
}

void PipeServer::Stop() {
    _running = false;
    if (_hListenThread) {
        WaitForSingleObject(_hListenThread, INFINITE);
        CloseHandle(_hListenThread);
        _hListenThread = NULL;
    }
}

DWORD WINAPI PipeServer::ClientThread(LPVOID lpParam) {
    PipeServer* pThis = static_cast<PipeServer*>(lpParam);
    
    while (pThis->_running) {
        HANDLE hPipe = CreateNamedPipe(
            pThis->_pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(IPCResponse),
            sizeof(IPCRequest),
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(100);
            continue;
        }

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            pThis->HandleClient(hPipe);
        } else {
            CloseHandle(hPipe);
        }
    }

    return 0;
}

void PipeServer::HandleClient(HANDLE hPipe) {
    IPCRequest req;
    DWORD bytesRead;
    
    while (ReadFile(hPipe, &req, sizeof(req), &bytesRead, NULL) && bytesRead != 0) {
        IPCResponse res = {};

        if (req.type == CommandType::Input) {
            std::wstring comp;
            if (_pEngineClient->SendInput(req.data, comp)) {
                res.commandSucceed = 1;
                wcscpy_s(res.buffer, comp.c_str());
            }
        } else if (req.type == CommandType::Reset) {
            if (_pEngineClient->Reset()) {
                res.commandSucceed = 1;
            }
        }

        DWORD bytesWritten;
        WriteFile(hPipe, &res, sizeof(res), &bytesWritten, NULL);
    }

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

} // namespace tcode
