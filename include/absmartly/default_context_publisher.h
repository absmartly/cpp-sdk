#pragma once

#include <absmartly/context_publisher.h>
#include <absmartly/client.h>
#include <memory>

namespace absmartly {

class DefaultContextPublisher : public ContextPublisher {
public:
    explicit DefaultContextPublisher(std::shared_ptr<Client> client);

    std::future<void> publish(const PublishEvent& event) override;

private:
    std::shared_ptr<Client> client_;
};

} // namespace absmartly
