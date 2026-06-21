#pragma once
#include "GlazeModels.hpp"
#include "EngineClient.h" // Ensures access to CommitResponse if defined there

#ifndef GLZ_USE_ASIO
#define GLZ_USE_ASIO
#endif
#include <glaze/net/http_client.hpp>
#include <stdexcept>
#include <string>

class GlazeClient {
public:
    GlazeClient(const std::string& host = "127.0.0.1", const std::string& port = "57001") 
        : server_base_url_("http://" + host + ":" + port) {}
    
    const std::unordered_map<std::string, std::string>& header = {
        {"Content-Type", "application/json"}
    };
    // Returns the fully hydrated BufferStatusResponse struct directly
    BufferStatusResponse SendPut(const std::string& target_char) {
        JsonPutRequest req{ target_char };
        
        std::string json_payload;
        auto ec_write = glz::write_json(req, json_payload);
        if (ec_write) {
            throw std::runtime_error("Glaze Serialization Error on Put request");
        }


        glz::http_client client{};
        auto response = client.post(server_base_url_ + "/v1/tcode/put", json_payload, header);
        if (!response.has_value()) {
            throw std::runtime_error("Network Failure on Put");
        }
        if (response->status_code != 200) {
            throw std::runtime_error("HTTP Status Error on Put: "+ server_base_url_ + "/v1/tcode/put" + std::to_string(response->status_code));
        }

        BufferStatusResponse res;
        auto ec_read = glz::read_json(res, response->response_body);
        if (ec_read) {
            throw std::runtime_error("Glaze Parse Failure on /put response");
        }
        return res;
    }

    BufferStatusResponse SendCommand(const std::string& action_endpoint) {
        JsonEmptyRequest req;
        std::string json_payload;
        auto ec_write = glz::write_json(req, json_payload);
        if (ec_write) {
            throw std::runtime_error("Glaze Serialization Error on Command request");
        }

        glz::http_client client{};
        auto response = client.post(server_base_url_ + "/v1/tcode/" + action_endpoint, json_payload, header);
        if (!response.has_value()) {
            throw std::runtime_error("Network Failure on Command");
        }
        if (response->status_code != 200) {
            throw std::runtime_error("HTTP Status Error on Command: " + std::to_string(response->status_code));
        }

        BufferStatusResponse res;
        auto ec_read = glz::read_json(res, response->response_body);
        if (ec_read) {
            throw std::runtime_error("Glaze Parse Failure on command response");
        }
        return res;
    }
    BufferStatusResponse SendSelect(int32_t selection_index) {
        JsonSelectRequest req{ selection_index };
        std::string json_payload;
        auto ec_write = glz::write_json(req, json_payload);
        if (ec_write) {
            throw std::runtime_error("Glaze Serialization Error on Select request");
        }
        glz::http_client client{};
        auto response = client.post(server_base_url_ + "/v1/tcode/select", json_payload, header);
        if (!response.has_value()) {
            throw std::runtime_error("Network Failure on Select");
        }
        if (response->status_code != 200) {
            throw std::runtime_error("HTTP Status Error on Select: " + std::to_string(response->status_code));
        }

        BufferStatusResponse res;
        auto ec_read = glz::read_json(res, response->response_body);
        if (ec_read) {
            throw std::runtime_error("Glaze Parse Failure on /select response");
        }
        return res;
    }
    CommitResponse SendCommit() {
        JsonEmptyRequest req;
        std::string json_payload;
        auto ec_write = glz::write_json(req, json_payload);
        if (ec_write) {
            throw std::runtime_error("Glaze Serialization Error on Commit request");
        }
        glz::http_client client{};
        auto response = client.post(server_base_url_ + "/v1/tcode/commit", json_payload, header);
        if (!response.has_value()) {
            throw std::runtime_error("Network Failure on Commit");
        }
        if (response->status_code != 200) {
            throw std::runtime_error("HTTP Status Error on Commit: " + std::to_string(response->status_code));
        }

        CommitResponse res;
        auto ec_read = glz::read_json(res, response->response_body);
        if (ec_read) {
            throw std::runtime_error("Glaze Parse Failure on /commit response");
        }
        return res;
    }

private:
    std::string server_base_url_;
};
