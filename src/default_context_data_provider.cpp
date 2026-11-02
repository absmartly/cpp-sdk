#include "absmartly/default_context_data_provider.h"

#include <stdexcept>

namespace absmartly {

DefaultContextDataProvider::DefaultContextDataProvider(std::shared_ptr<Client> client)
    : client_(std::move(client)) {
    if (!client_) {
        throw std::invalid_argument("client must not be null");
    }
}

std::future<ContextData> DefaultContextDataProvider::get_context_data() {
    return client_->get_context_data();
}

} // namespace absmartly
