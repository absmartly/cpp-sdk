#pragma once

#include <string>

namespace absmartly {

struct ClientConfig {
    std::string endpoint;
    std::string api_key;
    std::string application;
    std::string environment;
};

} // namespace absmartly
