#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <glaze/glaze.hpp>
#include "EngineClient.h"

// --- Request Payloads ---

struct JsonPutRequest {
    std::string ch{}; // Maps to 'char' in Scala
};

template <>
struct glz::meta<JsonPutRequest> {
    using T = JsonPutRequest;
    static constexpr auto value = object("char", &T::ch);
};

struct JsonSelectRequest {
    int32_t n{0};
};

template <>
struct glz::meta<JsonSelectRequest> {
    using T = JsonSelectRequest;
    static constexpr auto value = object("n", &T::n);
};

struct JsonEmptyRequest {};

template <>
struct glz::meta<JsonEmptyRequest> {
    using T = JsonEmptyRequest;
    static constexpr auto value = object();
};

// --- Response Payloads ---

struct BufferStatusResponse {
    std::string outputBuffer{};
    std::string buffer{};
    std::vector<std::string> candidates{};
    std::string lastCharAsKey{};
    bool commandSucceed{false};

    // Legacy backwards compatibility interfaces for EngineClient.cpp
    bool command_succeed() const { return commandSucceed; }
    std::string buffer_text() const { return buffer; }
    explicit operator bool() const { return commandSucceed; }
};

template <>
struct glz::meta<BufferStatusResponse> {
    using T = BufferStatusResponse;
    static constexpr auto value = object(
        "outputBuffer", &T::outputBuffer,
        "buffer", &T::buffer,
        "candidates", &T::candidates,
        "lastCharAsKey", &T::lastCharAsKey,
        "commandSucceed", &T::commandSucceed
    );
};

struct CommitResponse {
    std::string output{};
};

template <>
struct glz::meta<CommitResponse> {
    using T = CommitResponse;
    static constexpr auto value = object("output", &T::output);
};
