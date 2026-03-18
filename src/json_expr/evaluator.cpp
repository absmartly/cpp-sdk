#include "absmartly/json_expr/evaluator.h"

#include <cmath>
#include <charconv>
#include <locale>
#include <sstream>
#include <iomanip>

namespace absmartly {

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
    operators_["match"] = std::make_unique<MatchOperator>();
}

nlohmann::json Evaluator::evaluate(const nlohmann::json& expr, const nlohmann::json& vars) const {
    if (expr.is_array()) {
        auto it = operators_.find("and");
        if (it != operators_.end()) {
            return it->second->evaluate(*this, expr, vars);
        }
        return nullptr;
    }

    if (expr.is_object()) {
        for (auto it = expr.begin(); it != expr.end(); ++it) {
            auto op_it = operators_.find(it.key());
            if (op_it != operators_.end()) {
                return op_it->second->evaluate(*this, it.value(), vars);
            }
            break;
        }
        return nullptr;
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
        return !value.get_ref<const std::string&>().empty();
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
        double d = value.get<double>();
        double int_part;
        if (std::modf(d, &int_part) == 0.0 && std::abs(d) < 1e15) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::fixed << std::setprecision(0) << d;
            return oss.str();
        }
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::setprecision(15) << d;
        std::string result = oss.str();
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

nlohmann::json Evaluator::extract_var(const nlohmann::json& vars, const std::string& path) {
    std::vector<std::string> fragments;
    std::istringstream stream(path);
    std::string fragment;
    while (std::getline(stream, fragment, '/')) {
        fragments.push_back(fragment);
    }

    const nlohmann::json* current = vars.is_null() ? nullptr : &vars;
    if (!current) {
        return nullptr;
    }

    for (const auto& frag : fragments) {
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
