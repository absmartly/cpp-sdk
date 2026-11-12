#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "absmartly/json_expr/operators.h"

namespace absmartly {

class Evaluator {
public:
    Evaluator();

    nlohmann::json evaluate(const nlohmann::json& expr, const nlohmann::json& vars) const;
    bool evaluate_boolean(const nlohmann::json& expr, const nlohmann::json& vars) const;

    static std::optional<bool> to_boolean(const nlohmann::json& value);
    static std::optional<double> to_number(const nlohmann::json& value);
    static std::optional<std::string> to_string_value(const nlohmann::json& value);
    static std::optional<int> compare(const nlohmann::json& a, const nlohmann::json& b);
    // Semantic version comparison used by the semver_* operators, matching the JS and
    // Java SDKs: a leading v/V and build metadata are ignored, missing core parts count
    // as 0, numeric identifiers compare numerically and rank below alphanumeric ones, and
    // a release ranks above its pre-releases. Null when either side is not convertible to
    // a string or has an empty version core. Parsing is deliberately as lenient as the
    // other SDKs': "1.0.0-" and "1.0.0+" read as "1.0.0", and a non-numeric core part
    // (as in "1.x") compares as an alphanumeric identifier.
    static std::optional<int> version_compare(const nlohmann::json& a, const nlohmann::json& b);
    static nlohmann::json extract_var(const nlohmann::json& vars, const std::string& path);

private:
    std::map<std::string, std::unique_ptr<Operator>> operators_;
};

}
