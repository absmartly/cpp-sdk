#include "absmartly/audience_matcher.h"

namespace absmartly {

AudienceMatcher::AudienceMatcher() = default;

std::optional<bool> AudienceMatcher::evaluate(const std::string& audience_string, const nlohmann::json& attributes) const {
    if (audience_string.empty()) {
        return std::nullopt;
    }

    nlohmann::json audience;
    try {
        audience = nlohmann::json::parse(audience_string);
    } catch (...) {
        return std::nullopt;
    }

    if (audience.is_null() || !audience.is_object()) {
        return std::nullopt;
    }

    if (!audience.contains("filter")) {
        return std::nullopt;
    }

    const auto& filter = audience["filter"];
    if (!filter.is_array() && !filter.is_object()) {
        return std::nullopt;
    }

    return evaluator_.evaluate_boolean(filter, attributes);
}

}
