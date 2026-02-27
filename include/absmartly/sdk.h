#pragma once

#include <absmartly/sdk_config.h>
#include <absmartly/context.h>
#include <absmartly/context_config.h>
#include <memory>

namespace absmartly {

class SDK {
public:
    static std::shared_ptr<SDK> create(SDKConfig config);

    std::unique_ptr<Context> create_context(const ContextConfig& config);
    std::unique_ptr<Context> create_context_with(const ContextConfig& config, ContextData data);

    std::future<ContextData> get_context_data();

private:
    explicit SDK(SDKConfig config);

    std::shared_ptr<Client> client_;
    std::shared_ptr<ContextDataProvider> context_data_provider_;
    std::shared_ptr<ContextEventHandler> context_event_handler_;
    std::shared_ptr<ContextEventPublisher> context_event_publisher_;
};

} // namespace absmartly
