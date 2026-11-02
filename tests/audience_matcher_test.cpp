#include <catch2/catch_test_macros.hpp>

#include "absmartly/audience_matcher.h"

using namespace absmartly;
using json = nlohmann::json;

TEST_CASE("AudienceMatcher", "[audience_matcher]") {
    AudienceMatcher matcher;

    SECTION("returns nullopt on empty audience string") {
        REQUIRE_FALSE(matcher.evaluate("", nullptr).has_value());
    }

    SECTION("returns nullopt on empty object string") {
        REQUIRE_FALSE(matcher.evaluate("{}", nullptr).has_value());
    }

    SECTION("returns nullopt on null JSON string") {
        REQUIRE_FALSE(matcher.evaluate("null", nullptr).has_value());
    }

    SECTION("returns nullopt if filter is not object or array") {
        REQUIRE_FALSE(matcher.evaluate(R"({"filter":null})", nullptr).has_value());
        REQUIRE_FALSE(matcher.evaluate(R"({"filter":false})", nullptr).has_value());
        REQUIRE_FALSE(matcher.evaluate(R"({"filter":5})", nullptr).has_value());
        REQUIRE_FALSE(matcher.evaluate(R"({"filter":"a"})", nullptr).has_value());
    }

    SECTION("returns boolean for value expressions") {
        REQUIRE(matcher.evaluate(R"({"filter":[{"value":5}]})", nullptr).value() == true);
        REQUIRE(matcher.evaluate(R"({"filter":[{"value":true}]})", nullptr).value() == true);
        REQUIRE(matcher.evaluate(R"({"filter":[{"value":1}]})", nullptr).value() == true);
        REQUIRE(matcher.evaluate(R"({"filter":[{"value":null}]})", nullptr).value() == false);
        REQUIRE(matcher.evaluate(R"({"filter":[{"value":0}]})", nullptr).value() == false);
    }

    SECTION("evaluates with variables") {
        json returning_true = {{"returning", true}};
        json returning_false = {{"returning", false}};

        auto result1 = matcher.evaluate(R"({"filter":[{"not":{"var":"returning"}}]})", returning_true);
        REQUIRE(result1.has_value());
        REQUIRE(result1.value() == false);

        auto result2 = matcher.evaluate(R"({"filter":[{"not":{"var":"returning"}}]})", returning_false);
        REQUIRE(result2.has_value());
        REQUIRE(result2.value() == true);
    }
}

TEST_CASE("AudienceMatcher filter as object", "[audience_matcher]") {
    AudienceMatcher matcher;

    SECTION("filter can be an object expression") {
        auto result = matcher.evaluate(R"({"filter":{"value":true}})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);

        result = matcher.evaluate(R"({"filter":{"value":false}})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == false);
    }
}

TEST_CASE("AudienceMatcher complex expressions", "[audience_matcher]") {
    AudienceMatcher matcher;

    SECTION("not expression") {
        auto result = matcher.evaluate(R"({"filter":[{"not":{"value":true}}]})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == false);
    }

    SECTION("and expression") {
        auto result = matcher.evaluate(R"({"filter":[{"and":[{"value":true},{"value":true}]}]})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);

        result = matcher.evaluate(R"({"filter":[{"and":[{"value":true},{"value":false}]}]})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == false);
    }

    SECTION("or expression") {
        auto result = matcher.evaluate(R"({"filter":[{"or":[{"value":false},{"value":true}]}]})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);

        result = matcher.evaluate(R"({"filter":[{"or":[{"value":false},{"value":false}]}]})", nullptr);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == false);
    }
}
