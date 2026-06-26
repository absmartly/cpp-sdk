#pragma once

#include <absmartly/sdk_config.h>
#include <absmartly/client_config.h>
#include <absmartly/context.h>
#include <absmartly/context_config.h>
#include <absmartly/http_client.h>
#include <absmartly/http_mode.h>
#include <future>
#include <memory>

namespace absmartly {

class SDK {
public:
    static std::shared_ptr<SDK> create(SDKConfig config);

    static std::shared_ptr<SDK> create_simple(ClientConfig client_config,
                                                HTTPMode mode = HTTPMode::async);

    static std::shared_ptr<SDK> create_simple(ClientConfig client_config,
                                                std::shared_ptr<HTTPClient> http_client);

    std::unique_ptr<Context> create_context(const ContextConfig& config);
    std::unique_ptr<Context> create_context_with(const ContextConfig& config, ContextData data);

    std::future<ContextData> get_context_data();

private:
    explicit SDK(SDKConfig config);

    std::shared_ptr<Client> client_;
    std::shared_ptr<ContextDataProvider> context_data_provider_;
    std::shared_ptr<ContextEventLogger> context_event_handler_;
    std::shared_ptr<ContextPublisher> context_event_publisher_;
};

} // namespace absmartly
