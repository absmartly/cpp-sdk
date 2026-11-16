#include "absmartly/json_expr/evaluator.h"
#include "../number_format.h"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <locale>
#include <sstream>
#include <iomanip>

namespace absmartly {

namespace {

struct Semver {
    std::vector<std::string> parts;
    std::string pre_release;
};

std::vector<std::string> split_dots(const std::string& value) {
    std::vector<std::string> result;
    size_t start = 0;
    while (true) {
        const auto end = value.find('.', start);
        result.push_back(value.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

std::optional<Semver> parse_semver(std::string v) {
    if (!v.empty() && (v.front() == 'v' || v.front() == 'V')) v.erase(0, 1);
    const auto plus = v.find('+');
    if (plus != std::string::npos) v.resize(plus);
    if (v.empty()) return std::nullopt;

    const auto dash = v.find('-');
    const std::string core = v.substr(0, dash);
    if (core.empty()) return std::nullopt;
    return Semver{split_dots(core), dash == std::string::npos ? "" : v.substr(dash + 1)};
}

bool is_numeric_identifier(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}

std::string strip_leading_zeros(const std::string& s) {
    const auto first = s.find_first_not_of('0');
    return first == std::string::npos ? "0" : s.substr(first);
}

int compare_identifiers(const std::string& a, const std::string& b) {
    const bool a_num = is_numeric_identifier(a);
    const bool b_num = is_numeric_identifier(b);
    if (a_num && b_num) {
        const auto a_norm = strip_leading_zeros(a);
        const auto b_norm = strip_leading_zeros(b);
        if (a_norm.size() != b_norm.size()) return a_norm.size() > b_norm.size() ? 1 : -1;
        return a_norm == b_norm ? 0 : (a_norm > b_norm ? 1 : -1);
    }
    if (a_num) return -1;
    if (b_num) return 1;
    return a == b ? 0 : (a > b ? 1 : -1);
}

} // namespace

Evaluator::Evaluator() {
    operators_["value"] = std::make_unique<ValueOperator>();
    operators_["var"] = std::make_unique<VarOperator>();
    operators_["and"] = std::make_unique<AndOperator>();
    operators_["or"] = std::make_unique<OrOperator>();
    operators_["not"] = std::make_unique<NotOperator>();
    operators_["null"] = std::make_unique<NullOperator>();
    operators_["eq"] = std::make_unique<EqOperator>();
    operators_["gt"] = std::make_unique<GtOperator>();
    operators_["gte"] = std::make_unique<GteOperator>();
    operators_["lt"] = std::make_unique<LtOperator>();
    operators_["lte"] = std::make_unique<LteOperator>();
    operators_["in"] = std::make_unique<InOperator>();
    operators_["contains"] = std::make_unique<InOperator>();
    operators_["match"] = std::make_unique<MatchOperator>();
    operators_["semver_eq"] = std::make_unique<SemverEqOperator>();
    operators_["semver_gt"] = std::make_unique<SemverGtOperator>();
    operators_["semver_gte"] = std::make_unique<SemverGteOperator>();
    operators_["semver_lt"] = std::make_unique<SemverLtOperator>();
    operators_["semver_lte"] = std::make_unique<SemverLteOperator>();
}

nlohmann::json Evaluator::evaluate(const nlohmann::json& expr, const nlohmann::json& vars) const {
    if (expr.is_array()) {
        return operators_.at("and")->evaluate(*this, expr, vars);
    }

    if (expr.is_object() && !expr.empty()) {
        // Expressions use one operator key; JSON objects iterate alphabetically.
        auto it = expr.begin();
        auto op_it = operators_.find(it.key());
        if (op_it != operators_.end()) {
            return op_it->second->evaluate(*this, it.value(), vars);
        }
    }

    return nullptr;
}

bool Evaluator::evaluate_boolean(const nlohmann::json& expr, const nlohmann::json& vars) const {
    auto result = evaluate(expr, vars);
    auto b = to_boolean(result);
    return b.value_or(false);
}

std::optional<bool> Evaluator::to_boolean(const nlohmann::json& value) {
    if (value.is_null()) {
        return std::nullopt;
    }
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number()) {
        return value.get<double>() != 0.0;
    }
    if (value.is_string()) {
        const auto& str = value.get_ref<const std::string&>();
        return !str.empty() && str != "false" && str != "0";
    }
    if (value.is_array() || value.is_object()) {
        return true;
    }
    return std::nullopt;
}

std::optional<double> Evaluator::to_number(const nlohmann::json& value) {
    if (value.is_null()) {
        return std::nullopt;
    }
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? 1.0 : 0.0;
    }
    if (value.is_string()) {
        const auto& str = value.get_ref<const std::string&>();
        if (str.empty()) {
            return std::nullopt;
        }
        double result = 0.0;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
        if (ec == std::errc{} && ptr == str.data() + str.size() && std::isfinite(result)) {
            return result;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> Evaluator::to_string_value(const nlohmann::json& value) {
    if (value.is_null()) {
        return std::nullopt;
    }
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? std::string("true") : std::string("false");
    }
    if (value.is_number()) {
        // JS: x.toFixed(15).replace(/\.?0{0,15}$/, "")
        double d = value.get<double>();
        if (!std::isfinite(d)) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << d;
            return oss.str();
        }
        if (std::abs(d) >= 1e21) {
            return detail::large_number_to_string(d);
        }
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::fixed << std::setprecision(15) << d;
        std::string result = oss.str();
        size_t end = result.size();
        while (end > 0 && result[end - 1] == '0') {
            --end;
        }
        if (end > 0 && result[end - 1] == '.') {
            --end;
        }
        result.resize(end);
        return result;
    }
    return std::nullopt;
}

std::optional<int> Evaluator::compare(const nlohmann::json& a, const nlohmann::json& b) {
    if (a.is_null()) {
        if (b.is_null()) {
            return 0;
        }
        return std::nullopt;
    }
    if (b.is_null()) {
        return std::nullopt;
    }

    if (a.is_number()) {
        auto rvalue = to_number(b);
        if (rvalue.has_value()) {
            double lv = a.get<double>();
            double rv = *rvalue;
            if (lv == rv) return 0;
            return lv > rv ? 1 : -1;
        }
        return std::nullopt;
    }

    if (a.is_string()) {
        auto rvalue = to_string_value(b);
        if (rvalue.has_value()) {
            const auto& lv = a.get_ref<const std::string&>();
            if (lv == *rvalue) return 0;
            return lv > *rvalue ? 1 : -1;
        }
        return std::nullopt;
    }

    if (a.is_boolean()) {
        auto rvalue = to_boolean(b);
        if (rvalue.has_value()) {
            bool lv = a.get<bool>();
            bool rv = *rvalue;
            if (lv == rv) return 0;
            return lv > rv ? 1 : -1;
        }
        return std::nullopt;
    }

    if (a == b) {
        return 0;
    }

    return std::nullopt;
}

std::optional<int> Evaluator::version_compare(const nlohmann::json& a, const nlohmann::json& b) {
    const auto a_str = to_string_value(a);
    const auto b_str = to_string_value(b);
    if (!a_str.has_value() || !b_str.has_value()) return std::nullopt;

    const auto l = parse_semver(*a_str);
    const auto r = parse_semver(*b_str);
    if (!l.has_value() || !r.has_value()) return std::nullopt;

    const size_t max_len = std::max(l->parts.size(), r->parts.size());
    for (size_t i = 0; i < max_len; ++i) {
        const std::string l_part = i < l->parts.size() ? l->parts[i] : "0";
        const std::string r_part = i < r->parts.size() ? r->parts[i] : "0";
        const int result = compare_identifiers(l_part, r_part);
        if (result != 0) return result;
    }

    if (l->pre_release.empty() && r->pre_release.empty()) return 0;
    if (l->pre_release.empty()) return 1;
    if (r->pre_release.empty()) return -1;

    const auto l_pre = split_dots(l->pre_release);
    const auto r_pre = split_dots(r->pre_release);
    const size_t pre_len = std::max(l_pre.size(), r_pre.size());
    for (size_t i = 0; i < pre_len; ++i) {
        if (i >= l_pre.size()) return -1;
        if (i >= r_pre.size()) return 1;
        const int result = compare_identifiers(l_pre[i], r_pre[i]);
        if (result != 0) return result;
    }
    return 0;
}

nlohmann::json Evaluator::extract_var(const nlohmann::json& vars, const std::string& path) {
    std::istringstream stream(path);
    std::string frag;
    const nlohmann::json* current = vars.is_null() ? nullptr : &vars;
    if (!current) {
        return nullptr;
    }

    while (std::getline(stream, frag, '/')) {
        if (current->is_object()) {
            auto it = current->find(frag);
            if (it != current->end()) {
                current = &(*it);
                continue;
            }
            return nullptr;
        }
        if (current->is_array()) {
            try {
                std::size_t pos = 0;
                int index = std::stoi(frag, &pos);
                if (pos == frag.size() && index >= 0 && static_cast<std::size_t>(index) < current->size()) {
                    current = &(*current)[index];
                    continue;
                }
            } catch (...) {
            }
            return nullptr;
        }
        return nullptr;
    }

    return *current;
}

}
