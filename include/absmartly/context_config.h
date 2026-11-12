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
    // Application and environment names, used as system attributes when audiences
    // are evaluated. Populated from the client config by SDK::create_context(); set
    // them directly when constructing a Context without the SDK wrapper. Audience
    // filters that target `application`/`environment` read these, with any user-set
    // attribute of the same name taking precedence. Left empty means no key is added.
    std::string application;
    std::string environment;
};

} // namespace absmartly
