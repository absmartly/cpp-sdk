#pragma once

#include <absmartly/context_data_provider.h>
#include <absmartly/client.h>
#include <memory>

namespace absmartly {

class DefaultContextDataProvider : public ContextDataProvider {
public:
    explicit DefaultContextDataProvider(std::shared_ptr<Client> client);

    std::future<ContextData> get_context_data() override;

private:
    std::shared_ptr<Client> client_;
};

} // namespace absmartly
