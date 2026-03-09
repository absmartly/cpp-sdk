#include "absmartly/default_context_data_provider.h"

namespace absmartly {

DefaultContextDataProvider::DefaultContextDataProvider(std::shared_ptr<Client> client)
    : client_(std::move(client)) {}

std::future<ContextData> DefaultContextDataProvider::get_context_data() {
    return client_->get_context_data();
}

} // namespace absmartly
