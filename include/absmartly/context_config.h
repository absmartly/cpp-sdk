#pragma once
#include <map>
#include <string>

namespace absmartly {

struct ContextConfig {
    int publish_delay = -1;
    int refresh_period = 0;
    std::map<std::string, std::string> units;
    std::map<std::string, int> overrides;
    std::map<std::string, int> custom_assignments;
};

} // namespace absmartly
