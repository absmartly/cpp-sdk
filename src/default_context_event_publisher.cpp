#include "absmartly/default_context_event_publisher.h"

namespace absmartly {

DefaultContextEventPublisher::DefaultContextEventPublisher(std::shared_ptr<Client> client)
    : client_(std::move(client)) {}

std::future<void> DefaultContextEventPublisher::publish(const PublishEvent& event) {
    return client_->publish(event);
}

} // namespace absmartly
