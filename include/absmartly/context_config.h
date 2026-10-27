#pragma once
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace absmartly {

struct ContextConfig {
    int publish_delay = -1;
    int refresh_period = 0;
    // Unit uids are strings or numbers; the type given is kept.
    std::map<std::string, nlohmann::json> units;
    std::map<std::string, int> overrides;
    std::map<std::string, int> custom_assignments;
};

} // namespace absmartly
