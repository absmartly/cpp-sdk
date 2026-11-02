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
    static nlohmann::json extract_var(const nlohmann::json& vars, const std::string& path);

private:
    std::map<std::string, std::unique_ptr<Operator>> operators_;
};

}
