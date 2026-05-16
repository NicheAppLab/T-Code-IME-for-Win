#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "tcode_engine.grpc.pb.h"

using namespace io::github::nicheapplab::tcodeserver;

class CEngineClient {
public:
    CEngineClient();
    ~CEngineClient();

    bool Connect(const std::string& address);
    
    BufferStatusResponse Put(const std::string& char_str);
    BufferStatusResponse Left();
    BufferStatusResponse Right();
    BufferStatusResponse Reset();
    BufferStatusResponse Convert();
    BufferStatusResponse Select(int32_t n);
    CommitResponse Commit();
    BufferStatusResponse Backspace();
    
    bool SendInput(uint32_t vkCode, std::wstring& outComposition);
    
private:
    std::wstring Utf8ToWide(const std::string& str);
    std::shared_ptr<grpc::Channel> _channel;
    std::unique_ptr<TCodeService::Stub> _stub;
};
