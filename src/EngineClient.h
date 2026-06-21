#pragma once

#include <memory>
#include <string>
#include <cstring>
#include <cstdint>

#include "GlazeModels.hpp"
enum class EngineCommand : uint8_t {
    Backspace,
    Left,
    Right,
    Convert
};
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
    BufferStatusResponse ExecuteCommand(EngineCommand command);
    BufferStatusResponse ExecutePut(const std::string& inputChar);
    std::wstring Utf8ToWide(const std::string& str);

    // getters
    const std::vector<std::string>& GetCandidates() const;
    bool HasCandidates() const;
    std::wstring GetBuffer() const;
    std::wstring GetOutputBuffer() const;
    std::wstring GetLastKey() const;
    bool IsLastKeyEmpty() const;
    bool IsBufferEmpty() const;
    bool IsOutputBufferEmpty() const;
    int32_t GetSelectedIndex() const;
    bool Succeed() const;
private:
    BufferStatusResponse m_lastResponse{};
};
