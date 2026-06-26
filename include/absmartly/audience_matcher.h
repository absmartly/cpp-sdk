#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "absmartly/json_expr/evaluator.h"

namespace absmartly {

class AudienceMatcher {
public:
    AudienceMatcher();

    std::optional<bool> evaluate(const std::string& audience_string, const nlohmann::json& attributes) const;

private:
    Evaluator evaluator_;
};

}
