#pragma once

#include <absmartly/client_config.h>
#include <absmartly/http_client.h>
#include <absmartly/models.h>

#include <future>
#include <memory>

namespace absmartly {

class Client {
public:
    Client(const ClientConfig& config, std::shared_ptr<HTTPClient> http_client);

    std::future<ContextData> get_context_data();
    std::future<void> publish(const PublishEvent& event);

private:
    std::string url_;
    std::map<std::string, std::string> query_;
    std::map<std::string, std::string> headers_;
    std::shared_ptr<HTTPClient> http_client_;
};

} // namespace absmartly
