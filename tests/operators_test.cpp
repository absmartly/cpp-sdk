#include <catch2/catch_test_macros.hpp>

#include "absmartly/json_expr/evaluator.h"
#include "absmartly/json_expr/operators.h"

using namespace absmartly;
using json = nlohmann::json;

TEST_CASE("ValueOperator", "[operators][value]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns args directly without evaluating") {
        json expr = {{"value", 0}};
        REQUIRE(evaluator.evaluate(expr, vars) == 0);

        expr = {{"value", 1}};
        REQUIRE(evaluator.evaluate(expr, vars) == 1);

        expr = {{"value", true}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"value", false}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"value", ""}};
        REQUIRE(evaluator.evaluate(expr, vars) == "");

        expr = {{"value", nullptr}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());

        expr = {{"value", json::object()}};
        REQUIRE(evaluator.evaluate(expr, vars) == json::object());

        expr = {{"value", json::array()}};
        REQUIRE(evaluator.evaluate(expr, vars) == json::array());
    }
}

TEST_CASE("VarOperator", "[operators][var]") {
    Evaluator evaluator;

    SECTION("extracts variable using path string") {
        json vars = {{"a", 1}, {"b", json{{"c", 2}}}};
        json expr = {{"var", "a"}};
        REQUIRE(evaluator.evaluate(expr, vars) == 1);

        expr = {{"var", "b/c"}};
        REQUIRE(evaluator.evaluate(expr, vars) == 2);
    }

    SECTION("extracts variable using path object") {
        json vars = {{"a", json{{"b", json{{"c", "abc"}}}}}};
        json expr = {{"var", json{{"path", "a/b/c"}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == "abc");
    }

    SECTION("returns null for missing paths") {
        json vars = {{"a", 1}};
        json expr = {{"var", "x"}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("AndOperator", "[operators][and]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true if all arguments are truthy") {
        json expr = {{"and", json::array({json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"and", json::array({json{{"value", true}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"and", json::array({json{{"value", true}}, json{{"value", true}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false if any argument is falsy") {
        json expr = {{"and", json::array({json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"and", json::array({json{{"value", true}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"and", json::array({json{{"value", false}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"and", json::array({json{{"value", false}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"and", json::array({json{{"value", false}}, json{{"value", false}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns false if any argument evaluates to null") {
        json expr = {{"and", json::array({json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("empty array returns true") {
        json expr = {{"and", json::array()}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }
}

TEST_CASE("OrOperator", "[operators][or]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true if any argument is truthy") {
        json expr = {{"or", json::array({json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"or", json::array({json{{"value", true}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"or", json::array({json{{"value", true}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"or", json::array({json{{"value", false}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false if all arguments are falsy") {
        json expr = {{"or", json::array({json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"or", json::array({json{{"value", false}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"or", json::array({json{{"value", false}}, json{{"value", false}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns false if argument is null") {
        json expr = {{"or", json::array({json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("empty array returns true") {
        json expr = {{"or", json::array()}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }
}

TEST_CASE("NotOperator", "[operators][not]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("negates truthy to false") {
        json expr = {{"not", json{{"value", true}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("negates falsy to true") {
        json expr = {{"not", json{{"value", false}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("negates null to true") {
        json expr = {{"not", json{{"value", nullptr}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }
}

TEST_CASE("NullOperator", "[operators][null]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true if argument is null") {
        json expr = {{"null", json{{"value", nullptr}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false if argument is not null") {
        json expr = {{"null", json{{"value", true}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"null", json{{"value", false}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"null", json{{"value", 0}}}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }
}

TEST_CASE("EqOperator", "[operators][eq]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true when arguments are equal") {
        json expr = {{"eq", json::array({json{{"value", 0}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"eq", json::array({json{{"value", 1}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"eq", json::array({json{{"value", "abc"}}, json{{"value", "abc"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"eq", json::array({json{{"value", true}}, json{{"value", true}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"eq", json::array({json{{"value", false}}, json{{"value", false}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false when arguments are not equal") {
        json expr = {{"eq", json::array({json{{"value", 1}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"eq", json::array({json{{"value", 0}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null == null is true via compare") {
        REQUIRE(Evaluator::compare(nullptr, nullptr).has_value());
        REQUIRE(Evaluator::compare(nullptr, nullptr).value() == 0);
    }

    SECTION("equal arrays") {
        json expr = {{"eq", json::array({
            json{{"value", json::array({1, 2})}},
            json{{"value", json::array({1, 2})}}
        })}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("unequal arrays return null") {
        json expr = {{"eq", json::array({
            json{{"value", json::array({1, 2})}},
            json{{"value", json::array({3, 4})}}
        })}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("equal objects") {
        json expr = {{"eq", json::array({
            json{{"value", json{{"a", 1}, {"b", 2}}}},
            json{{"value", json{{"a", 1}, {"b", 2}}}}
        })}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("unequal objects return null") {
        json expr = {{"eq", json::array({
            json{{"value", json{{"a", 1}, {"b", 2}}}},
            json{{"value", json{{"a", 3}, {"b", 4}}}}
        })}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("GtOperator", "[operators][gt]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns false when equal") {
        json expr = {{"gt", json::array({json{{"value", 0}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns true when greater") {
        json expr = {{"gt", json::array({json{{"value", 1}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false when less") {
        json expr = {{"gt", json::array({json{{"value", 0}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null arguments return null") {
        json expr = {{"gt", json::array({json{{"value", nullptr}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("GteOperator", "[operators][gte]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true when equal") {
        json expr = {{"gte", json::array({json{{"value", 0}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns true when greater") {
        json expr = {{"gte", json::array({json{{"value", 1}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false when less") {
        json expr = {{"gte", json::array({json{{"value", 0}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null arguments return null") {
        json expr = {{"gte", json::array({json{{"value", nullptr}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("LtOperator", "[operators][lt]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns false when equal") {
        json expr = {{"lt", json::array({json{{"value", 0}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns false when greater") {
        json expr = {{"lt", json::array({json{{"value", 1}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns true when less") {
        json expr = {{"lt", json::array({json{{"value", 0}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("null arguments return null") {
        json expr = {{"lt", json::array({json{{"value", nullptr}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("LteOperator", "[operators][lte]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("returns true when equal") {
        json expr = {{"lte", json::array({json{{"value", 0}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("returns false when greater") {
        json expr = {{"lte", json::array({json{{"value", 1}}, json{{"value", 0}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("returns true when less") {
        json expr = {{"lte", json::array({json{{"value", 0}}, json{{"value", 1}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("null arguments return null") {
        json expr = {{"lte", json::array({json{{"value", nullptr}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}

TEST_CASE("InOperator", "[operators][in]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("string containment") {
        json expr = {{"in", json::array({json{{"value", "abc"}}, json{{"value", "abcdefghijk"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"in", json::array({json{{"value", "def"}}, json{{"value", "abcdefghijk"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"in", json::array({json{{"value", "xxx"}}, json{{"value", "abcdefghijk"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null needle or haystack returns null") {
        json expr = {{"in", json::array({json{{"value", nullptr}}, json{{"value", "abcdefghijk"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());

        expr = {{"in", json::array({json{{"value", "abc"}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("empty array returns false") {
        json expr = {{"in", json::array({json{{"value", 1}}, json{{"value", json::array()}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", "1"}}, json{{"value", json::array()}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", true}}, json{{"value", json::array()}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", false}}, json{{"value", json::array()}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null needle with array returns null") {
        json expr = {{"in", json::array({json{{"value", nullptr}}, json{{"value", json::array()}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("array element search") {
        json haystack01 = json::array({0, 1});
        json haystack12 = json::array({1, 2});

        json expr = {{"in", json::array({json{{"value", 2}}, json{{"value", haystack01}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", 0}}, json{{"value", haystack12}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", 1}}, json{{"value", haystack12}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"in", json::array({json{{"value", 2}}, json{{"value", haystack12}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }

    SECTION("object key search") {
        json haystackab = json{{"a", 1}, {"b", 2}};
        json haystackbc = json{{"b", 2}, {"c", 3}, {"0", 100}};

        json expr = {{"in", json::array({json{{"value", "c"}}, json{{"value", haystackab}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", "a"}}, json{{"value", haystackbc}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);

        expr = {{"in", json::array({json{{"value", "b"}}, json{{"value", haystackbc}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"in", json::array({json{{"value", "c"}}, json{{"value", haystackbc}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"in", json::array({json{{"value", 0}}, json{{"value", haystackbc}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);
    }
}

TEST_CASE("MatchOperator", "[operators][match]") {
    Evaluator evaluator;
    json vars = {};

    SECTION("regex matching") {
        json expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", ""}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "abc"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "ijk"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "^abc"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "ijk$"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "def"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "b.*j"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == true);

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", "xyz"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars) == false);
    }

    SECTION("null arguments return null") {
        json expr = {{"match", json::array({json{{"value", nullptr}}, json{{"value", "abc"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());

        expr = {{"match", json::array({json{{"value", "abcdefghijk"}}, json{{"value", nullptr}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("invalid regex returns null") {
        json expr = {{"match", json::array({json{{"value", "abc"}}, json{{"value", "[invalid"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("Fix: overly long pattern returns null (ReDoS protection)") {
        std::string long_pattern(1025, 'a');
        json expr = {{"match", json::array({json{{"value", "abc"}}, json{{"value", long_pattern}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }

    SECTION("Fix: overly long text returns null (ReDoS protection)") {
        std::string long_text(65537, 'a');
        json expr = {{"match", json::array({json{{"value", long_text}}, json{{"value", "a+"}}})}};
        REQUIRE(evaluator.evaluate(expr, vars).is_null());
    }
}
