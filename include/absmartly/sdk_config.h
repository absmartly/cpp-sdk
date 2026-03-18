#pragma once

#include <absmartly/client.h>
#include <absmartly/context_data_provider.h>
#include <absmartly/context_event_logger.h>
#include <absmartly/context_publisher.h>
#include <memory>

namespace absmartly {

struct SDKConfig {
    std::shared_ptr<Client> client;
    std::shared_ptr<ContextDataProvider> context_data_provider;
    std::shared_ptr<ContextEventLogger> context_event_handler;
    std::shared_ptr<ContextPublisher> context_event_publisher;
};

} // namespace absmartly
