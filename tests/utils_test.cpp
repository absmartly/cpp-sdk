#include <catch2/catch_test_macros.hpp>
#include "absmartly/hashing.h"
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
