#include <catch2/catch_test_macros.hpp>
#include "absmartly/hashing.h"
#include <limits>
#include <string>
#include <vector>

using namespace absmartly;

TEST_CASE("hash_unit matches JS SDK known hashes", "[utils]") {
    REQUIRE(hash_unit("4a42766ca6313d26f49985e799ff4f3790fb86efa0fce46edb3ea8fbf1ea3408") == "H2jvj6o9YcAgNdhKqEbtWw");
    REQUIRE(hash_unit("bleh@absmarty.com") == "DRgslOje35bZMmpaohQjkA");
    REQUIRE(hash_unit("testy") == "K5I_V6RgP8c6sYKz-TVn8g");
}

TEST_CASE("hash_unit with numeric string", "[utils]") {
    REQUIRE(hash_unit("123456778999") == "K4uy4bTeCy34W97lmceVRg");
}

TEST_CASE("hash_unit of a number rounds like JS toFixed(0)", "[utils]") {
    using nlohmann::json;
    REQUIRE(unit_to_string(json(125.75)) == "126");
    REQUIRE(unit_to_string(json(125.5)) == "126");
    REQUIRE(unit_to_string(json(-1.5)) == "-2");
    REQUIRE(unit_to_string(json(125)) == "125");
    REQUIRE(unit_to_string(json(125.0)) == "125");
    REQUIRE(unit_to_string(json("125.75")) == "125.75");
    REQUIRE(hash_unit(json(125.75)) == hash_unit("126"));
    REQUIRE(hash_unit(json(123456778999)) == "K4uy4bTeCy34W97lmceVRg");
    // JS: Number(1000000000000000129).toFixed(0) === "1000000000000000128"
    REQUIRE(unit_to_string(json(1000000000000000129LL)) == "1000000000000000128");
    REQUIRE(unit_to_string(json(1000000000000000129ULL)) == "1000000000000000128");
    REQUIRE(unit_to_string(json(1000000000000000128.0)) == "1000000000000000128");
    REQUIRE(unit_to_string(json(std::numeric_limits<double>::max())) == "1.7976931348623157e+308");
}

TEST_CASE("choose_variant with [0.0, 1.0] split", "[utils]") {
    REQUIRE(choose_variant({0.0, 1.0}, 0.0) == 1);
    REQUIRE(choose_variant({0.0, 1.0}, 0.5) == 1);
    REQUIRE(choose_variant({0.0, 1.0}, 1.0) == 1);
}

TEST_CASE("choose_variant with [1.0, 0.0] split", "[utils]") {
    REQUIRE(choose_variant({1.0, 0.0}, 0.0) == 0);
    REQUIRE(choose_variant({1.0, 0.0}, 0.5) == 0);
    REQUIRE(choose_variant({1.0, 0.0}, 1.0) == 1);
}

TEST_CASE("choose_variant with [0.5, 0.5] split", "[utils]") {
    REQUIRE(choose_variant({0.5, 0.5}, 0.0) == 0);
    REQUIRE(choose_variant({0.5, 0.5}, 0.25) == 0);
    REQUIRE(choose_variant({0.5, 0.5}, 0.49999999) == 0);
    REQUIRE(choose_variant({0.5, 0.5}, 0.5) == 1);
    REQUIRE(choose_variant({0.5, 0.5}, 0.50000001) == 1);
    REQUIRE(choose_variant({0.5, 0.5}, 0.75) == 1);
    REQUIRE(choose_variant({0.5, 0.5}, 1.0) == 1);
}

TEST_CASE("choose_variant with [0.333, 0.333, 0.334] split", "[utils]") {
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.0) == 0);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.25) == 0);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.33299999) == 0);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.333) == 1);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.33300001) == 1);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.5) == 1);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.66599999) == 1);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.666) == 2);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.66600001) == 2);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 0.75) == 2);
    REQUIRE(choose_variant({0.333, 0.333, 0.334}, 1.0) == 2);
}

TEST_CASE("base64url_no_padding matches known encodings", "[utils]") {
    struct TestCase {
        std::string input;
        std::string expected;
    };

    std::vector<TestCase> cases = {
        {"", ""},
        {" ", "IA"},
        {"t", "dA"},
        {"te", "dGU"},
        {"tes", "dGVz"},
        {"test", "dGVzdA"},
        {"testy", "dGVzdHk"},
        {"testy1", "dGVzdHkx"},
        {"testy12", "dGVzdHkxMg"},
        {"testy123", "dGVzdHkxMjM"},
    };

    for (const auto& tc : cases) {
        CAPTURE(tc.input);
        auto encoded = base64url_no_padding(
            reinterpret_cast<const uint8_t*>(tc.input.data()),
            tc.input.size()
        );
        REQUIRE(encoded == tc.expected);
    }
}

TEST_CASE("Fix: choose_variant returns 0 for empty split", "[utils][fix44]") {
    REQUIRE(choose_variant({}, 0.0) == 0);
    REQUIRE(choose_variant({}, 0.5) == 0);
    REQUIRE(choose_variant({}, 1.0) == 0);
}
