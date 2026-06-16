#include "IPCClient.h"
#include <iostream>

namespace tcode {

static bool WriteAll(HANDLE hPipe, const void* buffer, DWORD size) {
    DWORD totalWritten = 0;
    const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
    while (totalWritten < size) {
        DWORD bytesWritten = 0;
        if (!WriteFile(hPipe, ptr + totalWritten, size - totalWritten, &bytesWritten, NULL) || bytesWritten == 0) {
            return false;
        }
        totalWritten += bytesWritten;
    }
    return true;
}

static bool ReadAll(HANDLE hPipe, void* buffer, DWORD size) {
    DWORD totalRead = 0;
    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    while (totalRead < size) {
        DWORD bytesRead = 0;
        if (!ReadFile(hPipe, ptr + totalRead, size - totalRead, &bytesRead, NULL) || bytesRead == 0) {
            return false;
        }
        totalRead += bytesRead;
    }
    return true;
}

IPCClient::IPCClient() : _hPipe(INVALID_HANDLE_VALUE) {
    memset(&lastResponse, 0, sizeof(lastResponse));
}

IPCClient::~IPCClient() {
    Disconnect();
}

bool IPCClient::Connect() {
    if (_hPipe != INVALID_HANDLE_VALUE) return true;

    while (true) {
        _hPipe = CreateFile(
            _pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (_hPipe != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            return false;
        }

        if (!WaitNamedPipe(_pipeName.c_str(), 1000)) {
            return false;
        }
    }

    return true;
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
    if (!WriteAll(_hPipe, &req, sizeof(req))) {
        Disconnect();
        return false;
    }

    IPCResponse res = {};
    if (!ReadAll(_hPipe, &res, sizeof(res))) {
        Disconnect();
        return false;
    }

    // If the engine committed and reset, clear lastResponse to reflect the clean state.
    // The committed text is still returned via outCommitted for the caller to apply.
    if (res.commitSucceed) {
        memset(&lastResponse, 0, sizeof(lastResponse));
    } else {
        lastResponse = res;
    }

    if (isActive) *isActive = (res.isActive == 1);
    outCommitted = res.outputBuffer;
    outComposition = std::wstring(res.buffer) + res.lastCharAsKey;
    return res.commandSucceed == 1;
}

bool IPCClient::SendInput(uint32_t vkCode, IPCResponse& outResponse) {
    if (!Connect()) return false;

    IPCRequest req = { CommandType::Input, vkCode };
    if (!WriteAll(_hPipe, &req, sizeof(req))) {
        Disconnect();
        return false;
    }

    IPCResponse res = {};
    if (!ReadAll(_hPipe, &res, sizeof(res))) {
        Disconnect();
        return false;
    }

    if (res.commitSucceed) {
        memset(&lastResponse, 0, sizeof(lastResponse));
    } else {
        lastResponse = res;
    }
    
    outResponse = res;
    
    return res.commandSucceed == 1;
}

bool IPCClient::Reset() {
    if (!Connect()) return false;

    IPCRequest req = { CommandType::Reset, 0 };
    if (!WriteAll(_hPipe, &req, sizeof(req))) {
        Disconnect();
        return false;
    }

    IPCResponse res = {};
    if (!ReadAll(_hPipe, &res, sizeof(res))) {
        Disconnect();
        return false;
    }
    lastResponse = res;

    return res.commandSucceed;
}

} // namespace tcode
