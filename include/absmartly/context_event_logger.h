#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace absmartly {

class Context;

class ContextEventLogger {
public:
    virtual ~ContextEventLogger() = default;
    virtual void handle_event(Context& context, const std::string& event_type, const nlohmann::json& data) = 0;
};

} // namespace absmartly
