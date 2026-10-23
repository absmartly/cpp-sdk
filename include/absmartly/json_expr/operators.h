#pragma once

#include <memory>
#include <optional>
#include <regex>
#include <string>

#include <nlohmann/json.hpp>

namespace absmartly {

class Evaluator;

class Operator {
public:
    virtual ~Operator() = default;
    virtual nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const = 0;
};

class ValueOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class VarOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class AndOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class OrOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class NotOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class NullOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class EqOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class GtOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class GteOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class LtOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class LteOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class InOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

class MatchOperator : public Operator {
public:
    nlohmann::json evaluate(
        const Evaluator& evaluator,
        const nlohmann::json& args,
        const nlohmann::json& vars
    ) const override;
};

}
