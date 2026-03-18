#pragma once

#include <absmartly/models.h>
#include <future>

namespace absmartly {

class ContextPublisher {
public:
    virtual ~ContextPublisher() = default;
    virtual std::future<void> publish(const PublishEvent& event) = 0;
};

} // namespace absmartly
