#include "absmartly/default_context_publisher.h"

#include <stdexcept>

namespace absmartly {

DefaultContextPublisher::DefaultContextPublisher(std::shared_ptr<Client> client)
    : client_(std::move(client)) {
    if (!client_) {
        throw std::invalid_argument("client must not be null");
    }
}

std::future<void> DefaultContextPublisher::publish(const PublishEvent& event) {
    return client_->publish(event);
}

} // namespace absmartly
