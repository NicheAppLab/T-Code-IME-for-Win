#include "EngineClient.h"
#include "GlazeClient.hpp"
#include <windows.h>
#include <memory>
#include <fstream>
#include <chrono>

void LogToImeFile(const std::string& message) {
    // Writes directly to your user folder so Windows permissions won't block it
    std::ofstream logFile("C:\\Users\\Public\\tcode_ime_debug.log", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        // Strip the trailing newline from ctime safely
        char timeStr[26];
        ctime_s(timeStr, sizeof(timeStr), &time_t_now);
        timeStr[24] = '\0'; 

        logFile << "[" << timeStr << "] " << message << "\n";
    }
}

// Global pointer initialization tracking our Glaze client instance context
static std::unique_ptr<GlazeClient> g_glazeClient = nullptr;

CEngineClient::CEngineClient() {
    if (!g_glazeClient) {
        g_glazeClient = std::make_unique<GlazeClient>("127.0.0.1", "57001");
    }
}

CEngineClient::~CEngineClient() {}

bool CEngineClient::Connect(const std::string &address) {
    (void)address;
    return true;
}

// Convert internal UTF-8 response strings back to Windows WCHAR text streams
std::wstring CEngineClient::Utf8ToWide(const std::string &str) {
    if (str.empty()) return L"";

    // 1. Calculate required wide buffer size including the null terminator (-1 flag)
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size_needed <= 0) return L"";

    // 2. Allocate the exact string capacity
    std::wstring wstrTo(size_needed - 1, L'\0');

    // 3. Perform the conversion directly into the string's internal storage
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstrTo.data(), size_needed);

    return wstrTo;
}

// --- Live HTTP REST API Routings Straight to Pekko Server (Port 57001) ---

BufferStatusResponse CEngineClient::Put(const std::string &char_str) {
    try {
        if (g_glazeClient) {
            auto res = g_glazeClient->SendPut(char_str);
            m_lastResponse = res;
            return res;
        }
    } catch (...) {}
    m_lastResponse = BufferStatusResponse{};
    return BufferStatusResponse{};
}

BufferStatusResponse CEngineClient::Backspace() {
    return g_glazeClient ? g_glazeClient->SendCommand("backspace") : BufferStatusResponse{};
}
BufferStatusResponse CEngineClient::Left() {
    return g_glazeClient ? g_glazeClient->SendCommand("left") : BufferStatusResponse{};
}
BufferStatusResponse CEngineClient::Right() {
    return g_glazeClient ? g_glazeClient->SendCommand("right") : BufferStatusResponse{};
}
BufferStatusResponse CEngineClient::Convert() {
    return g_glazeClient ? g_glazeClient->SendCommand("convert") : BufferStatusResponse{};
}

bool CEngineClient::Reset() {
    try {
        if (g_glazeClient) {
            auto res = g_glazeClient->SendCommand("reset");
            m_lastResponse = res;
            return true;
        }
    } catch (...) {}
    m_lastResponse = BufferStatusResponse{};
    return false;
}

BufferStatusResponse CEngineClient::Select(int32_t n) {
    try {
        if (g_glazeClient) {
            auto res = g_glazeClient->SendSelect(n);
            m_lastResponse = res;
            return res;
        }
    } catch (...) {}
    m_lastResponse = BufferStatusResponse{};
    return BufferStatusResponse{};
}

CommitResponse CEngineClient::Commit() {
    try {
        if (g_glazeClient) {
            return g_glazeClient->SendCommit();
        }
    } catch (...) {}
    return CommitResponse{};
}
BufferStatusResponse CEngineClient::ExecuteCommand(EngineCommand command) {
    m_lastResponse = BufferStatusResponse{};

    switch(command) {
        case EngineCommand::Backspace:  m_lastResponse = Backspace(); break;
        case EngineCommand::Left:       m_lastResponse = Left(); break;
        case EngineCommand::Right:      m_lastResponse = Right(); break;
        case EngineCommand::Convert:    m_lastResponse = Convert(); break;
    }
    return m_lastResponse;
}
BufferStatusResponse CEngineClient::ExecutePut(const std::string& inputChar) {
    LogToImeFile(">>> ExecutePut called with char: " + inputChar);
    m_lastResponse = BufferStatusResponse{};
    
    try {
        if (!inputChar.empty() && g_glazeClient) {
            // 1. Fetch the raw response string before Glaze reads it
            // (You may need to add a quick helper method to pull response_body if tracking inside GlazeClient)
            m_lastResponse = g_glazeClient->SendPut(inputChar);
            
            LogToImeFile("=== Glaze network pass completed successfully");
            LogToImeFile("    Server outputBuffer: " + m_lastResponse.outputBuffer);
            LogToImeFile("    Server buffer: " + m_lastResponse.buffer);
            LogToImeFile("    Server commandSucceed: " + std::to_string(m_lastResponse.commandSucceed));
        }
    } 
    catch (const std::exception& e) {
        LogToImeFile("❌ EXCEPTION CAUGHT: " + std::string(e.what()));
    }
    catch (...) {
        LogToImeFile("❌ UNKNOWN EXCEPTION CAUGHT");
    }

    LogToImeFile("<<< ExecutePut returning. commandSucceed state is: " + std::to_string(m_lastResponse.command_succeed()));
    return m_lastResponse;
}

const std::vector<std::string>& CEngineClient::GetCandidates() const {
    return m_lastResponse.candidates;
}
bool CEngineClient::HasCandidates() const {
    return !m_lastResponse.candidates.empty();
}

std::wstring CEngineClient::GetBuffer() const {
    return const_cast<CEngineClient*>(this)->Utf8ToWide(m_lastResponse.buffer_text());
}
std::wstring CEngineClient::GetOutputBuffer() const {
    return const_cast<CEngineClient*>(this)->Utf8ToWide(m_lastResponse.outputBuffer);
}
std::wstring CEngineClient::GetLastKey() const {
    //Some(a)
    //0123456
    if(IsLastKeyEmpty()){
        return L"";
    } else {
        using namespace std::string_literals;
        return L"("s + (const_cast<CEngineClient*>(this)->Utf8ToWide(m_lastResponse.lastCharAsKey))[5] + L")"s;
    }
}
bool CEngineClient::IsLastKeyEmpty() const {
    return m_lastResponse.lastCharAsKey == "None";
}
bool CEngineClient::IsBufferEmpty() const {
    return m_lastResponse.buffer_text().empty();
}
bool CEngineClient::IsOutputBufferEmpty() const {
    return m_lastResponse.outputBuffer.empty();
}
bool CEngineClient::Succeed() const {
    return m_lastResponse.command_succeed();
}