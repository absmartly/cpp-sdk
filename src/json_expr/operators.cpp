#include "absmartly/json_expr/operators.h"
#include "absmartly/json_expr/evaluator.h"

#include <regex>

namespace absmartly {

nlohmann::json ValueOperator::evaluate(
    const Evaluator& /*evaluator*/,
    const nlohmann::json& args,
    const nlohmann::json& /*vars*/
) const {
    return args;
}

nlohmann::json VarOperator::evaluate(
    const Evaluator& /*evaluator*/,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    std::string path;
    if (args.is_object() && args.contains("path")) {
        path = args["path"].get<std::string>();
    } else if (args.is_string()) {
        path = args.get<std::string>();
    } else {
        return nullptr;
    }
    return Evaluator::extract_var(vars, path);
}

nlohmann::json AndOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    for (const auto& expr : args) {
        auto result = evaluator.evaluate(expr, vars);
        auto b = Evaluator::to_boolean(result);
        if (!b.value_or(false)) {
            return false;
        }
    }
    return true;
}

nlohmann::json OrOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    for (const auto& expr : args) {
        auto result = evaluator.evaluate(expr, vars);
        auto b = Evaluator::to_boolean(result);
        if (b.value_or(false)) {
            return true;
        }
    }
    return args.empty();
}

nlohmann::json NotOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    auto result = evaluator.evaluate(args, vars);
    auto b = Evaluator::to_boolean(result);
    return !b.value_or(false);
}

nlohmann::json NullOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    auto result = evaluator.evaluate(args, vars);
    return result.is_null();
}

nlohmann::json EqOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto lhs = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    auto rhs = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    auto result = Evaluator::compare(lhs, rhs);
    if (result.has_value()) {
        return *result == 0;
    }
    return nullptr;
}

nlohmann::json GtOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto lhs = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (lhs.is_null()) {
        return nullptr;
    }
    auto rhs = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (rhs.is_null()) {
        return nullptr;
    }
    auto result = Evaluator::compare(lhs, rhs);
    if (result.has_value()) {
        return *result > 0;
    }
    return nullptr;
}

nlohmann::json GteOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto lhs = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (lhs.is_null()) {
        return nullptr;
    }
    auto rhs = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (rhs.is_null()) {
        return nullptr;
    }
    auto result = Evaluator::compare(lhs, rhs);
    if (result.has_value()) {
        return *result >= 0;
    }
    return nullptr;
}

nlohmann::json LtOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto lhs = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (lhs.is_null()) {
        return nullptr;
    }
    auto rhs = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (rhs.is_null()) {
        return nullptr;
    }
    auto result = Evaluator::compare(lhs, rhs);
    if (result.has_value()) {
        return *result < 0;
    }
    return nullptr;
}

nlohmann::json LteOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto lhs = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (lhs.is_null()) {
        return nullptr;
    }
    auto rhs = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (rhs.is_null()) {
        return nullptr;
    }
    auto result = Evaluator::compare(lhs, rhs);
    if (result.has_value()) {
        return *result <= 0;
    }
    return nullptr;
}

nlohmann::json InOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto needle = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (needle.is_null()) {
        return nullptr;
    }
    auto haystack = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (haystack.is_null()) {
        return nullptr;
    }

    if (haystack.is_array()) {
        for (const auto& item : haystack) {
            auto cmp = Evaluator::compare(item, needle);
            if (cmp.has_value() && *cmp == 0) {
                return true;
            }
        }
        return false;
    }

    if (haystack.is_string()) {
        auto needle_str = Evaluator::to_string_value(needle);
        if (needle_str.has_value()) {
            const auto& hs = haystack.get_ref<const std::string&>();
            return hs.find(*needle_str) != std::string::npos;
        }
        return false;
    }

    if (haystack.is_object()) {
        auto needle_str = Evaluator::to_string_value(needle);
        if (needle_str.has_value()) {
            return haystack.contains(*needle_str);
        }
        return false;
    }

    return nullptr;
}

nlohmann::json MatchOperator::evaluate(
    const Evaluator& evaluator,
    const nlohmann::json& args,
    const nlohmann::json& vars
) const {
    if (!args.is_array()) {
        return nullptr;
    }
    auto text_val = args.size() > 0 ? evaluator.evaluate(args[0], vars) : nlohmann::json(nullptr);
    if (text_val.is_null()) {
        return nullptr;
    }
    auto pattern_val = args.size() > 1 ? evaluator.evaluate(args[1], vars) : nlohmann::json(nullptr);
    if (pattern_val.is_null()) {
        return nullptr;
    }

    auto text = Evaluator::to_string_value(text_val);
    if (!text.has_value()) {
        return nullptr;
    }
    auto pattern = Evaluator::to_string_value(pattern_val);
    if (!pattern.has_value()) {
        return nullptr;
    }

    if (pattern->size() > 1024 || text->size() > 65536) {
        return nullptr;
    }

    try {
        std::regex re(*pattern);
        return std::regex_search(*text, re);
    } catch (...) {
        return nullptr;
    }
}

}
