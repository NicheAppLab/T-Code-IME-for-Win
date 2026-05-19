#include "EngineClient.h"

CEngineClient::CEngineClient() {
}

CEngineClient::~CEngineClient() {
}

bool CEngineClient::Connect(const std::string& address) {
    _channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    if (_channel) {
        _stub = TCodeService::NewStub(_channel);
        return true;
    }
    return false;
}

BufferStatusResponse CEngineClient::Put(const std::string& char_str) {
    PutRequest request;
    request.set_char_(char_str);
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Put(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Left() {
    InflexLeftRequest request;
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Left(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Right() {
    InflexRightRequest request;
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Right(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Reset() {
    ResetRequest request;
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Reset(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Convert() {
    ConvertRequest request;
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Convert(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Select(int32_t n) {
    SelectCandidateRequest request;
    request.set_n(n);
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Select(&context, request, &response);
    return response;
}

CommitResponse CEngineClient::Commit() {
    CommitRequest request;
    CommitResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Commit(&context, request, &response);
    return response;
}

BufferStatusResponse CEngineClient::Backspace() {
    BackspaceRequest request;
    BufferStatusResponse response;
    grpc::ClientContext context;
    if (_stub) _stub->Backspace(&context, request, &response);
    return response;
}

bool CEngineClient::SendInput(uint32_t vkCode, std::wstring& outComposition) {
    BufferStatusResponse resp;
    bool commandSucceed = true;
    
    if (vkCode >= 'A' && vkCode <= 'Z') {
        char c = (char)vkCode;
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) c = tolower(c);
        resp = Put(std::string(1, c));
    } else if (vkCode >= '0' && vkCode <= '9') {
        resp = Put(std::string(1, (char)vkCode));
    } else if (vkCode == VK_BACK) {
        resp = Backspace();
    } else if (vkCode == VK_ESCAPE) {
        resp = Reset();
    } else if (vkCode == VK_SPACE) {
        resp = Convert();
    } else if (vkCode == VK_RETURN) {
        Commit();
        outComposition = L"";
        return true;
    } else {
        return false;
    }

    commandSucceed = resp.command_succeed();
    if (!commandSucceed) {
        outComposition.clear();
        return false;
    }

    outComposition = Utf8ToWide(resp.buffer());
    return true;
}

std::wstring CEngineClient::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
