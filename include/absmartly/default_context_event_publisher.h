#pragma once

#include <absmartly/context_event_publisher.h>
#include <absmartly/client.h>
#include <memory>

namespace absmartly {

class DefaultContextEventPublisher : public ContextEventPublisher {
public:
    explicit DefaultContextEventPublisher(std::shared_ptr<Client> client);

    std::future<void> publish(const PublishEvent& event) override;

private:
    std::shared_ptr<Client> client_;
};

} // namespace absmartly
