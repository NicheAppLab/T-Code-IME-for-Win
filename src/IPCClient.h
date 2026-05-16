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
    uint32_t success;
    uint32_t isActive; // 1 if engine has buffer or candidates
    wchar_t committed[256];
    wchar_t composition[256];
};
#pragma pack(pop)

class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    bool Connect();
    void Disconnect();

    bool SendInput(uint32_t vkCode, std::wstring& outCommitted, std::wstring& outComposition, bool* isActive = nullptr);
    bool Reset();

private:
    HANDLE _hPipe;
    const std::wstring _pipeName = L"\\\\.\\pipe\\TCodeEngineProxy";
};

} // namespace tcode
