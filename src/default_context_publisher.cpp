#include "absmartly/default_context_publisher.h"

namespace absmartly {

DefaultContextPublisher::DefaultContextPublisher(std::shared_ptr<Client> client)
    : client_(std::move(client)) {}

std::future<void> DefaultContextPublisher::publish(const PublishEvent& event) {
    return client_->publish(event);
}

} // namespace absmartly
