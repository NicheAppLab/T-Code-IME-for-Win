#include "EngineClient.h"
#include <windows.h>

// Constructor / Destructor – nothing to initialise
CEngineClient::CEngineClient() {}
CEngineClient::~CEngineClient() {}

// Connect is a no‑op that always succeeds (IME does not need a server)
bool CEngineClient::Connect(const std::string &address) {
    (void)address; // silence unused warning
    return true;
}

// Stub RPC‑style methods – simply return default stub objects
BufferStatusResponse CEngineClient::Put(const std::string &char_str) { (void)char_str; return BufferStatusResponse(); }
BufferStatusResponse CEngineClient::Left() { return BufferStatusResponse(); }
BufferStatusResponse CEngineClient::Right() { return BufferStatusResponse(); }
bool CEngineClient::Reset() { return true; }
BufferStatusResponse CEngineClient::Convert() { return BufferStatusResponse(); }
BufferStatusResponse CEngineClient::Select(int32_t n) { (void)n; return BufferStatusResponse(); }
CommitResponse CEngineClient::Commit() { return CommitResponse(); }
BufferStatusResponse CEngineClient::Backspace() { return BufferStatusResponse(); }

// Simplified input handling – always succeeds and returns an empty composition
bool CEngineClient::SendInput(uint32_t vkCode, std::wstring &outComposition) {
    (void)vkCode; // ignore actual key for stub
    outComposition.clear();
    return true;
}

// No actual UTF‑8 conversion needed – return empty string
std::wstring CEngineClient::Utf8ToWide(const std::string &str) { (void)str; return L""; }
