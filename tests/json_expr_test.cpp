#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "absmartly/json_expr/evaluator.h"

using namespace absmartly;
using json = nlohmann::json;

TEST_CASE("Evaluator::to_boolean", "[evaluator][boolean]") {
    SECTION("null returns nullopt") {
        REQUIRE_FALSE(Evaluator::to_boolean(nullptr).has_value());
    }

    SECTION("boolean values pass through") {
        REQUIRE(Evaluator::to_boolean(true).value() == true);
        REQUIRE(Evaluator::to_boolean(false).value() == false);
    }

    SECTION("numbers: 0 is false, non-zero is true") {
        REQUIRE(Evaluator::to_boolean(0).value() == false);
        REQUIRE(Evaluator::to_boolean(1).value() == true);
        REQUIRE(Evaluator::to_boolean(-1).value() == true);
        REQUIRE(Evaluator::to_boolean(1.5).value() == true);
        REQUIRE(Evaluator::to_boolean(2).value() == true);
    }

    SECTION("strings: empty is false, non-empty is true") {
        REQUIRE(Evaluator::to_boolean("").value() == false);
        REQUIRE(Evaluator::to_boolean("abc").value() == true);
        REQUIRE(Evaluator::to_boolean("0").value() == true);
        REQUIRE(Evaluator::to_boolean("1").value() == true);
    }

    SECTION("arrays and objects are always true") {
        REQUIRE(Evaluator::to_boolean(json::array()).value() == true);
        REQUIRE(Evaluator::to_boolean(json::object()).value() == true);
        REQUIRE(Evaluator::to_boolean(json::array({1, 2})).value() == true);
        REQUIRE(Evaluator::to_boolean(json{{"a", 1}}).value() == true);
    }
}

TEST_CASE("Evaluator::to_number", "[evaluator][number]") {
    SECTION("null returns nullopt") {
        REQUIRE_FALSE(Evaluator::to_number(nullptr).has_value());
    }

    SECTION("booleans convert to 0/1") {
        REQUIRE(Evaluator::to_number(true).value() == 1.0);
        REQUIRE(Evaluator::to_number(false).value() == 0.0);
    }

    SECTION("numbers pass through") {
        REQUIRE(Evaluator::to_number(0).value() == 0.0);
        REQUIRE(Evaluator::to_number(1).value() == 1.0);
        REQUIRE(Evaluator::to_number(1.5).value() == 1.5);
        REQUIRE(Evaluator::to_number(-1.0).value() == -1.0);
        REQUIRE(Evaluator::to_number(2.0).value() == 2.0);
        REQUIRE(Evaluator::to_number(3.0).value() == 3.0);
        REQUIRE(Evaluator::to_number(0x7fffffff).value() == 2147483647.0);
        REQUIRE(Evaluator::to_number(-0x7fffffff).value() == -2147483647.0);
    }

    SECTION("strings: numeric strings parse, empty and non-numeric are nullopt") {
        REQUIRE_FALSE(Evaluator::to_number(json("")).has_value());
        REQUIRE(Evaluator::to_number(json("0")).value() == 0.0);
        REQUIRE(Evaluator::to_number(json("1")).value() == 1.0);
        REQUIRE(Evaluator::to_number(json("1.5")).value() == 1.5);
        REQUIRE(Evaluator::to_number(json("123")).value() == 123.0);
        REQUIRE(Evaluator::to_number(json("-1")).value() == -1.0);
        REQUIRE(Evaluator::to_number(json("2")).value() == 2.0);
        REQUIRE(Evaluator::to_number(json("3.0")).value() == 3.0);
        REQUIRE_FALSE(Evaluator::to_number(json("abc")).has_value());
        REQUIRE_FALSE(Evaluator::to_number(json("x1234")).has_value());
    }

    SECTION("arrays and objects return nullopt") {
        REQUIRE_FALSE(Evaluator::to_number(json::array()).has_value());
        REQUIRE_FALSE(Evaluator::to_number(json::object()).has_value());
    }
}

TEST_CASE("Evaluator::to_string_value", "[evaluator][string]") {
    SECTION("null returns nullopt") {
        REQUIRE_FALSE(Evaluator::to_string_value(nullptr).has_value());
    }

    SECTION("booleans convert to string") {
        REQUIRE(Evaluator::to_string_value(true).value() == "true");
        REQUIRE(Evaluator::to_string_value(false).value() == "false");
    }

    SECTION("strings pass through") {
        REQUIRE(Evaluator::to_string_value(json("")).value() == "");
        REQUIRE(Evaluator::to_string_value(json("abc")).value() == "abc");
    }

    SECTION("numbers convert to string representation") {
        REQUIRE(Evaluator::to_string_value(0).value() == "0");
        REQUIRE(Evaluator::to_string_value(1).value() == "1");
        REQUIRE(Evaluator::to_string_value(-1).value() == "-1");
        REQUIRE(Evaluator::to_string_value(1.5).value() == "1.5");
        REQUIRE(Evaluator::to_string_value(2.0).value() == "2");
        REQUIRE(Evaluator::to_string_value(3.0).value() == "3");
        REQUIRE(Evaluator::to_string_value(-1.0).value() == "-1");
        REQUIRE(Evaluator::to_string_value(0.0).value() == "0");
        REQUIRE(Evaluator::to_string_value(2147483647.0).value() == "2147483647");
        REQUIRE(Evaluator::to_string_value(-2147483647.0).value() == "-2147483647");
    }

    SECTION("arrays and objects return nullopt") {
        REQUIRE_FALSE(Evaluator::to_string_value(json::array()).has_value());
        REQUIRE_FALSE(Evaluator::to_string_value(json::object()).has_value());
    }
}

TEST_CASE("Evaluator::compare", "[evaluator][compare]") {
    SECTION("null comparisons") {
        REQUIRE(Evaluator::compare(nullptr, nullptr).value() == 0);

        REQUIRE_FALSE(Evaluator::compare(nullptr, 0).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, 1).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, true).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, false).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, json("")).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, json("abc")).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, json::object()).has_value());
        REQUIRE_FALSE(Evaluator::compare(nullptr, json::array()).has_value());

        REQUIRE_FALSE(Evaluator::compare(0, nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(1, nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(true, nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(false, nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(json(""), nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(json("abc"), nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), nullptr).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), nullptr).has_value());
    }

    SECTION("object/array comparisons") {
        REQUIRE_FALSE(Evaluator::compare(json::object(), 0).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), 1).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), true).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), false).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), json("")).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), json("abc")).has_value());
        REQUIRE(Evaluator::compare(json::object(), json::object()).value() == 0);
        REQUIRE(Evaluator::compare(json{{"a", 1}}, json{{"a", 1}}).value() == 0);
        REQUIRE_FALSE(Evaluator::compare(json{{"a", 1}}, json{{"b", 2}}).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::object(), json::array()).has_value());

        REQUIRE_FALSE(Evaluator::compare(json::array(), 0).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), 1).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), true).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), false).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), json("")).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), json("abc")).has_value());
        REQUIRE_FALSE(Evaluator::compare(json::array(), json::object()).has_value());
        REQUIRE(Evaluator::compare(json::array(), json::array()).value() == 0);
        REQUIRE(Evaluator::compare(json::array({1, 2}), json::array({1, 2})).value() == 0);
        REQUIRE_FALSE(Evaluator::compare(json::array({1, 2}), json::array({3, 4})).has_value());
    }

    SECTION("boolean comparisons coerce rhs") {
        REQUIRE(Evaluator::compare(false, 0).value() == 0);
        REQUIRE(Evaluator::compare(false, 1).value() == -1);
        REQUIRE(Evaluator::compare(false, true).value() == -1);
        REQUIRE(Evaluator::compare(false, false).value() == 0);
        REQUIRE(Evaluator::compare(false, json("")).value() == 0);
        REQUIRE(Evaluator::compare(false, json("abc")).value() == -1);
        REQUIRE(Evaluator::compare(false, json::object()).value() == -1);
        REQUIRE(Evaluator::compare(false, json::array()).value() == -1);

        REQUIRE(Evaluator::compare(true, 0).value() == 1);
        REQUIRE(Evaluator::compare(true, 1).value() == 0);
        REQUIRE(Evaluator::compare(true, true).value() == 0);
        REQUIRE(Evaluator::compare(true, false).value() == 1);
        REQUIRE(Evaluator::compare(true, json("")).value() == 1);
        REQUIRE(Evaluator::compare(true, json("abc")).value() == 0);
        REQUIRE(Evaluator::compare(true, json::object()).value() == 0);
        REQUIRE(Evaluator::compare(true, json::array()).value() == 0);
    }

    SECTION("number comparisons") {
        REQUIRE(Evaluator::compare(0, 0).value() == 0);
        REQUIRE(Evaluator::compare(0, 1).value() == -1);
        REQUIRE(Evaluator::compare(0, true).value() == -1);
        REQUIRE(Evaluator::compare(0, false).value() == 0);
        REQUIRE_FALSE(Evaluator::compare(0, json("")).has_value());
        REQUIRE_FALSE(Evaluator::compare(0, json("abc")).has_value());
        REQUIRE_FALSE(Evaluator::compare(0, json::object()).has_value());
        REQUIRE_FALSE(Evaluator::compare(0, json::array()).has_value());

        REQUIRE(Evaluator::compare(1, 0).value() == 1);
        REQUIRE(Evaluator::compare(1, 1).value() == 0);
        REQUIRE(Evaluator::compare(1, true).value() == 0);
        REQUIRE(Evaluator::compare(1, false).value() == 1);
        REQUIRE_FALSE(Evaluator::compare(1, json("")).has_value());
        REQUIRE_FALSE(Evaluator::compare(1, json("abc")).has_value());
        REQUIRE_FALSE(Evaluator::compare(1, json::object()).has_value());
        REQUIRE_FALSE(Evaluator::compare(1, json::array()).has_value());

        REQUIRE(Evaluator::compare(1.0, 1).value() == 0);
        REQUIRE(Evaluator::compare(1.5, 1).value() == 1);
        REQUIRE(Evaluator::compare(2.0, 1).value() == 1);
        REQUIRE(Evaluator::compare(3.0, 1).value() == 1);

        REQUIRE(Evaluator::compare(1, 1.0).value() == 0);
        REQUIRE(Evaluator::compare(1, 1.5).value() == -1);
        REQUIRE(Evaluator::compare(1, 2.0).value() == -1);
        REQUIRE(Evaluator::compare(1, 3.0).value() == -1);
    }

    SECTION("string comparisons") {
        REQUIRE(Evaluator::compare(json(""), json("")).value() == 0);
        REQUIRE(Evaluator::compare(json("abc"), json("abc")).value() == 0);
        REQUIRE(Evaluator::compare(json("0"), 0).value() == 0);
        REQUIRE(Evaluator::compare(json("1"), 1).value() == 0);
        REQUIRE(Evaluator::compare(json("true"), true).value() == 0);
        REQUIRE(Evaluator::compare(json("false"), false).value() == 0);
        REQUIRE_FALSE(Evaluator::compare(json(""), json::object()).has_value());
        REQUIRE_FALSE(Evaluator::compare(json("abc"), json::object()).has_value());
        REQUIRE_FALSE(Evaluator::compare(json(""), json::array()).has_value());
        REQUIRE_FALSE(Evaluator::compare(json("abc"), json::array()).has_value());

        REQUIRE(Evaluator::compare(json("abc"), json("bcd")).value() == -1);
        REQUIRE(Evaluator::compare(json("bcd"), json("abc")).value() == 1);
        REQUIRE(Evaluator::compare(json("0"), json("1")).value() == -1);
        REQUIRE(Evaluator::compare(json("1"), json("0")).value() == 1);
        REQUIRE(Evaluator::compare(json("9"), json("100")).value() == 1);
        REQUIRE(Evaluator::compare(json("100"), json("9")).value() == -1);
    }
}

TEST_CASE("Evaluator::extract_var", "[evaluator][extract_var]") {
    json vars = {
        {"a", 1},
        {"b", true},
        {"c", false},
        {"d", json::array({1, 2, 3})},
        {"e", json::array({1, json{{"z", 2}}, 3})},
        {"f", json{{"y", json{{"x", 3}, {"0", 10}}}}}
    };

    SECTION("top-level access") {
        REQUIRE(Evaluator::extract_var(vars, "a") == 1);
        REQUIRE(Evaluator::extract_var(vars, "b") == true);
        REQUIRE(Evaluator::extract_var(vars, "c") == false);
        REQUIRE(Evaluator::extract_var(vars, "d") == json::array({1, 2, 3}));
        REQUIRE(Evaluator::extract_var(vars, "e") == json::array({1, json{{"z", 2}}, 3}));
        REQUIRE(Evaluator::extract_var(vars, "f") == json{{"y", json{{"x", 3}, {"0", 10}}}});
    }

    SECTION("invalid paths on non-objects") {
        REQUIRE(Evaluator::extract_var(vars, "a/0").is_null());
        REQUIRE(Evaluator::extract_var(vars, "a/b").is_null());
        REQUIRE(Evaluator::extract_var(vars, "b/0").is_null());
        REQUIRE(Evaluator::extract_var(vars, "b/e").is_null());
    }

    SECTION("array index access") {
        REQUIRE(Evaluator::extract_var(vars, "d/0") == 1);
        REQUIRE(Evaluator::extract_var(vars, "d/1") == 2);
        REQUIRE(Evaluator::extract_var(vars, "d/2") == 3);
        REQUIRE(Evaluator::extract_var(vars, "d/3").is_null());
    }

    SECTION("nested access") {
        REQUIRE(Evaluator::extract_var(vars, "e/0") == 1);
        REQUIRE(Evaluator::extract_var(vars, "e/1/z") == 2);
        REQUIRE(Evaluator::extract_var(vars, "e/2") == 3);
        REQUIRE(Evaluator::extract_var(vars, "e/1/0").is_null());

        auto fy = Evaluator::extract_var(vars, "f/y");
        REQUIRE(fy.contains("x"));
        REQUIRE(fy["x"] == 3);
        REQUIRE(Evaluator::extract_var(vars, "f/y/x") == 3);
        REQUIRE(Evaluator::extract_var(vars, "f/y/0") == 10);
    }
}

TEST_CASE("Evaluator::evaluate", "[evaluator]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns null if operator not found") {
        REQUIRE(evaluator.evaluate(json{{"not_found", true}}, vars).is_null());
    }

    SECTION("calls operator with args") {
        json expr = {{"value", json::array({1, 2, 3})}};
        auto result = evaluator.evaluate(expr, vars);
        REQUIRE(result == json::array({1, 2, 3}));
    }

    SECTION("array is treated as implicit AND") {
        json expr = json::array({json{{"value", true}}, json{{"value", true}}});
        auto result = evaluator.evaluate(expr, vars);
        REQUIRE(result == true);

        json expr2 = json::array({json{{"value", true}}, json{{"value", false}}});
        auto result2 = evaluator.evaluate(expr2, vars);
        REQUIRE(result2 == false);
    }

    SECTION("nested evaluation") {
        json expr = {{"and", json::array({
            json{{"value", true}},
            json{{"gt", json::array({json{{"value", 5}}, json{{"value", 3}}})}}
        })}};
        auto result = evaluator.evaluate(expr, vars);
        REQUIRE(result == true);
    }
}

TEST_CASE("Evaluator::evaluate_boolean", "[evaluator]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("truthy values") {
        REQUIRE(evaluator.evaluate_boolean(json{{"value", true}}, vars) == true);
        REQUIRE(evaluator.evaluate_boolean(json{{"value", 1}}, vars) == true);
        REQUIRE(evaluator.evaluate_boolean(json{{"value", 5}}, vars) == true);
    }

    SECTION("falsy values") {
        REQUIRE(evaluator.evaluate_boolean(json{{"value", false}}, vars) == false);
        REQUIRE(evaluator.evaluate_boolean(json{{"value", 0}}, vars) == false);
        REQUIRE(evaluator.evaluate_boolean(json{{"value", nullptr}}, vars) == false);
    }
}

TEST_CASE("JsonExpr integration tests", "[evaluator][integration]") {
    Evaluator evaluator;

    auto value_for = [](const json& x) -> json { return {{"value", x}}; };
    auto var_for = [](const std::string& p) -> json { return {{"var", json{{"path", p}}}}; };
    auto binary_op = [](const std::string& op, const json& a, const json& b) -> json {
        return {{op, json::array({a, b})}};
    };
    auto unary_op = [](const std::string& op, const json& arg) -> json {
        return {{op, arg}};
    };

    json john = {{"age", 20}, {"language", "en-US"}, {"returning", false}};
    json terry = {{"age", 20}, {"language", "en-GB"}, {"returning", true}};
    json kate = {{"age", 50}, {"language", "es-ES"}, {"returning", false}};
    json maria = {{"age", 52}, {"language", "pt-PT"}, {"returning", true}};

    json age_twenty_and_us = json::array({
        binary_op("eq", var_for("age"), value_for(20)),
        binary_op("eq", var_for("language"), value_for("en-US"))
    });

    json age_over_fifty = json::array({
        binary_op("gte", var_for("age"), value_for(50))
    });

    json age_twenty_and_us_or_age_over_fifty = json::array({
        json{{"or", json::array({age_twenty_and_us, age_over_fifty})}}
    });

    json returning = json::array({
        binary_op("eq", var_for("returning"), value_for(true))
    });

    json returning_and_age_twenty_and_us_or_age_over_fifty = json::array({
        returning[0],
        age_twenty_and_us_or_age_over_fifty[0]
    });

    json not_returning_and_spanish = json::array({
        unary_op("not", returning),
        binary_op("eq", var_for("language"), value_for("es-ES"))
    });

    SECTION("AgeTwentyAndUS") {
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us, john) == true);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us, terry) == false);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us, kate) == false);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us, maria) == false);
    }

    SECTION("AgeOverFifty") {
        REQUIRE(evaluator.evaluate_boolean(age_over_fifty, john) == false);
        REQUIRE(evaluator.evaluate_boolean(age_over_fifty, terry) == false);
        REQUIRE(evaluator.evaluate_boolean(age_over_fifty, kate) == true);
        REQUIRE(evaluator.evaluate_boolean(age_over_fifty, maria) == true);
    }

    SECTION("AgeTwentyAndUS_Or_AgeOverFifty") {
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us_or_age_over_fifty, john) == true);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us_or_age_over_fifty, terry) == false);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us_or_age_over_fifty, kate) == true);
        REQUIRE(evaluator.evaluate_boolean(age_twenty_and_us_or_age_over_fifty, maria) == true);
    }

    SECTION("Returning") {
        REQUIRE(evaluator.evaluate_boolean(returning, john) == false);
        REQUIRE(evaluator.evaluate_boolean(returning, terry) == true);
        REQUIRE(evaluator.evaluate_boolean(returning, kate) == false);
        REQUIRE(evaluator.evaluate_boolean(returning, maria) == true);
    }

    SECTION("Returning_And_AgeTwentyAndUS_Or_AgeOverFifty") {
        REQUIRE(evaluator.evaluate_boolean(returning_and_age_twenty_and_us_or_age_over_fifty, john) == false);
        REQUIRE(evaluator.evaluate_boolean(returning_and_age_twenty_and_us_or_age_over_fifty, terry) == false);
        REQUIRE(evaluator.evaluate_boolean(returning_and_age_twenty_and_us_or_age_over_fifty, kate) == false);
        REQUIRE(evaluator.evaluate_boolean(returning_and_age_twenty_and_us_or_age_over_fifty, maria) == true);
    }

    SECTION("NotReturning_And_Spanish") {
        REQUIRE(evaluator.evaluate_boolean(not_returning_and_spanish, john) == false);
        REQUIRE(evaluator.evaluate_boolean(not_returning_and_spanish, terry) == false);
        REQUIRE(evaluator.evaluate_boolean(not_returning_and_spanish, kate) == true);
        REQUIRE(evaluator.evaluate_boolean(not_returning_and_spanish, maria) == false);
    }
}
