#pragma once

#include <memory>
#include <string>
#include <cstring>
#include <cstdint>

// Simple stub response structs to replace protobuf generated types
struct BufferStatusResponse {
    bool command_succeed() const { return true; }
    std::string buffer() const { return ""; }
    explicit operator bool() const { return command_succeed(); }
};
struct CommitResponse {};

class CEngineClient {
public:
    CEngineClient();
    ~CEngineClient();

    bool Connect(const std::string& address);
    BufferStatusResponse Put(const std::string& char_str);
    BufferStatusResponse Left();
    BufferStatusResponse Right();
    bool Reset();
    BufferStatusResponse Convert();
    BufferStatusResponse Select(int32_t n);
    CommitResponse Commit();
    BufferStatusResponse Backspace();
    bool SendInput(uint32_t vkCode, std::wstring& outComposition);

private:
    std::wstring Utf8ToWide(const std::string& str);
};
