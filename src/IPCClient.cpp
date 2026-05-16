#include "IPCClient.h"
#include <iostream>

namespace tcode {

IPCClient::IPCClient() : _hPipe(INVALID_HANDLE_VALUE) {}

IPCClient::~IPCClient() {
    Disconnect();
}

bool IPCClient::Connect() {
    if (_hPipe != INVALID_HANDLE_VALUE) return true;

    _hPipe = CreateFile(
        _pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    return _hPipe != INVALID_HANDLE_VALUE;
}

void IPCClient::Disconnect() {
    if (_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(_hPipe);
        _hPipe = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::SendInput(uint32_t vkCode, std::wstring& outCommitted, std::wstring& outComposition, bool* isActive) {
    if (!Connect()) return false;

    IPCRequest req = { CommandType::Input, vkCode };
    DWORD bytesWritten;
    if (!WriteFile(_hPipe, &req, sizeof(req), &bytesWritten, NULL)) {
        Disconnect();
        return false;
    }

    IPCResponse res = {};
    DWORD bytesRead;
    if (!ReadFile(_hPipe, &res, sizeof(res), &bytesRead, NULL)) {
        Disconnect();
        return false;
    }

    if (res.success == 1) {
        if (isActive) *isActive = (res.isActive == 1);
        outCommitted = res.committed;
        outComposition = res.composition;
        return true;
    }
    return false;
}

bool IPCClient::Reset() {
    if (!Connect()) return false;

    IPCRequest req = { CommandType::Reset, 0 };
    DWORD bytesWritten;
    if (!WriteFile(_hPipe, &req, sizeof(req), &bytesWritten, NULL)) {
        Disconnect();
        return false;
    }

    IPCResponse res = {};
    DWORD bytesRead;
    if (!ReadFile(_hPipe, &res, sizeof(res), &bytesRead, NULL)) {
        Disconnect();
        return false;
    }

    return res.success;
}

} // namespace tcode
