#include "absmartly/audience_matcher.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <string>

namespace absmartly {

namespace {

constexpr double SPLIT_PERCENTAGES_SUM_TOLERANCE = 0.01;

bool is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_js_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// JavaScript parseFloat: leading whitespace is skipped and the longest valid decimal
// prefix is parsed, so "50abc" is 50 and "abc" is NaN.
double js_parse_float(const std::string& s) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    size_t i = 0;
    const size_t n = s.size();
    while (i < n && is_js_whitespace(s[i])) {
        ++i;
    }

    const size_t start = i;
    bool negative = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) {
        negative = s[i] == '-';
        ++i;
    }

    if (s.compare(i, 8, "Infinity") == 0) {
        return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }

    bool digits = false;
    while (i < n && is_ascii_digit(s[i])) {
        ++i;
        digits = true;
    }
    if (i < n && s[i] == '.') {
        ++i;
        while (i < n && is_ascii_digit(s[i])) {
            ++i;
            digits = true;
        }
    }
    if (!digits) {
        return nan;
    }

    size_t end = i;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;
        if (j < n && (s[j] == '+' || s[j] == '-')) {
            ++j;
        }
        const size_t exp_start = j;
        while (j < n && is_ascii_digit(s[j])) {
            ++j;
        }
        if (j > exp_start) {
            end = j;
        }
    }

    // from_chars rejects a leading '+', and has no locale, unlike strtod.
    size_t from = start;
    if (s[from] == '+') {
        ++from;
    }
    double value = 0.0;
    auto [ptr, ec] = std::from_chars(s.data() + from, s.data() + end, value);
    if (ec == std::errc::result_out_of_range) {
        // Overflow parses as +/-Infinity in JS, which the sum check then rejects.
        return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    }
    if (ec != std::errc{} || ptr != s.data() + end) {
        return nan;
    }
    return value;
}

// JavaScript's parseFloat(x.toFixed(2)). Exact ties (x.xx5 in binary) only occur at
// gaps far above the tolerance, so how they round does not change the outcome.
double round_to_two_decimals(double value) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, 2);
    if (ec != std::errc{}) {
        return std::numeric_limits<double>::infinity();
    }
    double rounded = 0.0;
    std::from_chars(buf, ptr, rounded);
    return rounded;
}

// JavaScript's Number.isInteger, limited to the int range.
std::optional<int> as_integer(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        if (value.is_number_unsigned()) {
            const auto u = value.get<uint64_t>();
            if (u > static_cast<uint64_t>(std::numeric_limits<int>::max())) return std::nullopt;
            return static_cast<int>(u);
        }
        const auto i = value.get<int64_t>();
        if (i < std::numeric_limits<int>::min() || i > std::numeric_limits<int>::max()) return std::nullopt;
        return static_cast<int>(i);
    }
    if (value.is_number_float()) {
        const double d = value.get<double>();
        if (!std::isfinite(d) || d != std::floor(d) ||
            d < static_cast<double>(std::numeric_limits<int>::min()) ||
            d > static_cast<double>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        return static_cast<int>(d);
    }
    return std::nullopt;
}

std::optional<RuleAction> parse_rule_action(const nlohmann::json& rule) {
    auto type_it = rule.find("type");
    if (type_it == rule.end() || !type_it->is_string()) {
        return std::nullopt;
    }
    const auto& type = type_it->get_ref<const std::string&>();

    if (type == "assign") {
        auto variant_it = rule.find("variant");
        if (variant_it == rule.end()) return std::nullopt;
        auto variant = as_integer(*variant_it);
        if (!variant.has_value()) return std::nullopt;
        RuleAction action;
        action.variant = *variant;
        return action;
    }

    if (type == "split") {
        auto percentages_it = rule.find("percentages");
        if (percentages_it == rule.end()) return std::nullopt;
        auto split = AudienceMatcher::parse_split_percentages(*percentages_it);
        if (!split.has_value()) return std::nullopt;
        RuleAction action;
        action.is_split = true;
        action.split = std::move(*split);
        return action;
    }

    return std::nullopt;
}

} // namespace

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

std::optional<std::vector<double>> AudienceMatcher::parse_split_percentages(const nlohmann::json& percentages) {
    if (!percentages.is_string()) {
        return std::nullopt;
    }

    const auto& text = percentages.get_ref<const std::string&>();
    std::vector<double> values;
    double total = 0.0;
    size_t start = 0;
    while (true) {
        const auto end = text.find('/', start);
        const double value = js_parse_float(text.substr(start, end == std::string::npos ? end : end - start));
        if (std::isnan(value) || value < 0) {
            return std::nullopt;
        }
        values.push_back(value);
        total += value;
        if (end == std::string::npos) break;
        start = end + 1;
    }

    const double gap = std::abs(100 - total);
    if (!std::isfinite(gap) || round_to_two_decimals(gap) > SPLIT_PERCENTAGES_SUM_TOLERANCE) {
        return std::nullopt;
    }

    for (auto& value : values) {
        value /= 100;
    }
    return values;
}

std::optional<RuleAction> AudienceMatcher::evaluate_rules(const nlohmann::json& assignment_rules,
                                                          const std::string& environment,
                                                          const nlohmann::json& attributes) const {
    if (!assignment_rules.is_object()) {
        return std::nullopt;
    }
    auto rules_it = assignment_rules.find("rules");
    if (rules_it == assignment_rules.end() || !rules_it->is_array()) {
        return std::nullopt;
    }

    for (const auto& rule : *rules_it) {
        if (!rule.is_object()) continue;

        auto action = parse_rule_action(rule);
        if (!action.has_value()) continue;

        auto envs_it = rule.find("environments");
        if (envs_it != rule.end() && !envs_it->is_null()) {
            if (!envs_it->is_array()) continue;
            if (!envs_it->empty()) {
                if (environment.empty()) continue;
                bool found = false;
                for (const auto& env : *envs_it) {
                    if (env.is_string() && env.get_ref<const std::string&>() == environment) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }
        }

        auto conditions_it = rule.find("conditions");
        if (conditions_it == rule.end() || conditions_it->is_null()) {
            return action;
        }
        if (!conditions_it->is_object()) continue;

        try {
            if (evaluator_.evaluate_boolean(*conditions_it, attributes)) {
                return action;
            }
        } catch (const std::exception&) {
            // A condition that fails to evaluate skips the rule, as in the JS and Java SDKs.
        }
    }

    return std::nullopt;
}

} // namespace absmartly
