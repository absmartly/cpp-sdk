#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "absmartly/json_expr/evaluator.h"

namespace absmartly {

// The action of the first matching assignment rule: a fixed variant ("assign") or
// percentages to randomise the unit over ("split", as fractions summing to 1).
struct RuleAction {
    bool is_split = false;
    int variant = 0;
    std::vector<double> split;
};

class AudienceMatcher {
public:
    AudienceMatcher();

    std::optional<bool> evaluate(const std::string& audience_string, const nlohmann::json& attributes) const;

    // Evaluates parsed assignment rules ({"rules": [...]}) in order and returns the
    // action of the first one that matches, or nullopt when none does. A rule is skipped
    // when it is not an object, its type is unknown, an assign rule's variant is not an
    // integer, a split rule's percentages are invalid, its environments are present but
    // not an array, a non-empty environments array does not contain `environment` (an
    // empty environment never matches one), its conditions are present but not an
    // object, or its conditions do not evaluate to true. A rule without conditions
    // always matches. Variant bounds are the caller's responsibility.
    std::optional<RuleAction> evaluate_rules(const nlohmann::json& assignment_rules,
                                             const std::string& environment,
                                             const nlohmann::json& attributes) const;

    // Parses split percentages such as "20/30/50" into fractions. Each value is read like
    // JavaScript's parseFloat and divided by 100. Null when the input is not a string, a
    // value is not a number or is negative, or the sum is more than 0.01 from 100 after
    // rounding the gap to two decimals (the backend's own check).
    static std::optional<std::vector<double>> parse_split_percentages(const nlohmann::json& percentages);

private:
    Evaluator evaluator_;
};

}
