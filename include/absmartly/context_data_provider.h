#pragma once

#include <absmartly/models.h>
#include <future>

namespace absmartly {

class ContextDataProvider {
public:
    virtual ~ContextDataProvider() = default;
    virtual std::future<ContextData> get_context_data() = 0;
};

} // namespace absmartly
