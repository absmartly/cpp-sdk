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

    // The application and environment names from the client config. Audiences and
    // assignment-rule conditions read these (the collector also attaches both
    // server-side), so the SDK injects them into the evaluation attribute map.
    const std::string& application() const noexcept { return application_; }
    const std::string& environment() const noexcept { return environment_; }

private:
    std::string url_;
    std::map<std::string, std::string> query_;
    std::map<std::string, std::string> headers_;
    std::string application_;
    std::string environment_;
    std::shared_ptr<HTTPClient> http_client_;
};

} // namespace absmartly
