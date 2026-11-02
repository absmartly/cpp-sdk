#include "absmartly/sdk.h"
#include "absmartly/client.h"
#include "absmartly/client_config.h"
#include "absmartly/default_context_data_provider.h"
#include "absmartly/default_context_publisher.h"
#include "absmartly/default_http_client.h"
#include "absmartly/async_http_client.h"

#include <stdexcept>

namespace absmartly {

SDK::SDK(SDKConfig config)
    : client_(std::move(config.client))
    , context_data_provider_(std::move(config.context_data_provider))
    , context_event_handler_(std::move(config.context_event_handler))
    , context_event_publisher_(std::move(config.context_event_publisher)) {
    if (!context_data_provider_ || !context_event_publisher_) {
        if (!client_) {
            throw std::invalid_argument("Missing Client instance in SDKConfig");
        }

        if (!context_data_provider_) {
            context_data_provider_ = std::make_shared<DefaultContextDataProvider>(client_);
        }

        if (!context_event_publisher_) {
            context_event_publisher_ = std::make_shared<DefaultContextPublisher>(client_);
        }
    }
}

std::shared_ptr<SDK> SDK::create(SDKConfig config) {
    return std::shared_ptr<SDK>(new SDK(std::move(config)));
}

std::shared_ptr<SDK> SDK::create_simple(ClientConfig client_config, HTTPMode mode) {
    std::shared_ptr<HTTPClient> http_client;
    if (mode == HTTPMode::sync) {
        http_client = std::make_shared<DefaultHTTPClient>();
    } else {
        http_client = std::make_shared<AsyncHTTPClient>();
    }
    return create_simple(std::move(client_config), std::move(http_client));
}

std::shared_ptr<SDK> SDK::create_simple(ClientConfig client_config,
                                         std::shared_ptr<HTTPClient> http_client) {
    auto client = std::make_shared<Client>(client_config, http_client);

    SDKConfig sdk_config;
    sdk_config.client = client;

    return create(std::move(sdk_config));
}

std::unique_ptr<Context> SDK::create_context(const ContextConfig& config) {
    auto data_future = context_data_provider_->get_context_data();
    return std::make_unique<Context>(config, std::move(data_future),
                                     context_event_handler_, context_event_publisher_,
                                     context_data_provider_);
}

std::unique_ptr<Context> SDK::create_context_with(const ContextConfig& config, ContextData data) {
    return std::make_unique<Context>(config, std::move(data),
                                     context_event_handler_, context_event_publisher_,
                                     context_data_provider_);
}

std::future<ContextData> SDK::get_context_data() {
    return context_data_provider_->get_context_data();
}

} // namespace absmartly
