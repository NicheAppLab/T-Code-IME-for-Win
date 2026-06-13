#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace tcode {

enum class CommandType : uint32_t {
    Input = 1,
    Reset = 2,
    GetComposition = 3
};

#pragma pack(push, 1)
struct IPCRequest {
    CommandType type;
    uint32_t data; // e.g. Virtual Key Code
};

struct IPCResponse {
    uint32_t commandSucceed;     // gRPC: commandSucceed
    uint32_t isActive;           // 1 if engine has buffer or candidates
    uint32_t commitSucceed;      // 1 if CommitAsync was called and engine reset
    uint32_t selectedIndex;      // Index of the selected candidate
    wchar_t outputBuffer[256];   // gRPC: outputBuffer (committed text)
    wchar_t buffer[256];         // gRPC: buffer (composition text)
    wchar_t candidates[512];     // gRPC: candidates (null-separated list)
    wchar_t lastCharAsKey[64];   // gRPC: lastCharAsKey ("Some(x)" or "None")
};
#pragma pack(pop)

class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    bool Connect();
    void Disconnect();

    bool SendInput(uint32_t vkCode, std::wstring& outCommitted, std::wstring& outComposition, bool* isActive = nullptr);
    bool SendInput(uint32_t vkCode, IPCResponse& outResponse);
    bool Reset();

    IPCResponse lastResponse;

private:
    HANDLE _hPipe;
    const std::wstring _pipeName = L"\\\\.\\pipe\\TCodeEngineProxy";
};

} // namespace tcode
