#include "absmartly/client.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace absmartly {

Client::Client(const ClientConfig& config, std::shared_ptr<HTTPClient> http_client)
    : http_client_(std::move(http_client)) {
    if (config.endpoint.empty()) {
        throw std::invalid_argument("Missing endpoint configuration");
    }

    if (config.api_key.empty()) {
        throw std::invalid_argument("Missing api_key configuration");
    }

    if (config.application.empty()) {
        throw std::invalid_argument("Missing application configuration");
    }

    if (config.environment.empty()) {
        throw std::invalid_argument("Missing environment configuration");
    }

    url_ = config.endpoint + "/v1/context";

    headers_["X-API-Key"] = config.api_key;
    headers_["X-Application"] = config.application;
    headers_["X-Environment"] = config.environment;
    headers_["X-Application-Version"] = "0";
    headers_["X-Agent"] = "absmartly-cpp-sdk";
    headers_["Content-Type"] = "application/json";

    query_["application"] = config.application;
    query_["environment"] = config.environment;
}

std::future<ContextData> Client::get_context_data() {
    auto future_response = http_client_->get(url_, query_, headers_);

    return std::async(std::launch::async, [resp_future = std::move(future_response)]() mutable -> ContextData {
        auto response = resp_future.get();

        if (response.status_code / 100 != 2) {
            throw std::runtime_error("HTTP error: " + std::to_string(response.status_code) +
                                     " " + response.status_message);
        }

        if (response.content.empty()) {
            throw std::runtime_error("Empty response body from context data endpoint");
        }

        std::string body(response.content.begin(), response.content.end());
        auto j = nlohmann::json::parse(body);
        return j.get<ContextData>();
    });
}

std::future<void> Client::publish(const PublishEvent& event) {
    nlohmann::json j = event;
    std::string body_str = j.dump();
    std::vector<uint8_t> body(body_str.begin(), body_str.end());

    auto future_response = http_client_->put(url_, {}, headers_, body);

    return std::async(std::launch::async, [resp_future = std::move(future_response)]() mutable {
        auto response = resp_future.get();

        if (response.status_code / 100 != 2) {
            throw std::runtime_error("HTTP error: " + std::to_string(response.status_code) +
                                     " " + response.status_message);
        }
    });
}

} // namespace absmartly
