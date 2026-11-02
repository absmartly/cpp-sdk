#include <catch2/catch_test_macros.hpp>
#include "absmartly/variant_assigner.h"
#include "absmartly/hashing.h"
#include <limits>
#include <string>
#include <vector>

using namespace absmartly;

TEST_CASE("VariantAssigner with bleh@absmartly.com", "[assigner]") {
    VariantAssigner assigner(hash_unit("bleh@absmartly.com"));

    SECTION("50/50 split") {
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000000) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000001) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x8015406f, 0x7ef49b98) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x3b2e7d90, 0xca87df4d) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x52c1f657, 0xd248bb2e) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x865a84d0, 0xaa22d41a) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x27d1dc86, 0x845461b9) == 1);
    }

    SECTION("33/33/34 split") {
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000000) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000001) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x8015406f, 0x7ef49b98) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x3b2e7d90, 0xca87df4d) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x52c1f657, 0xd248bb2e) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x865a84d0, 0xaa22d41a) == 1);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x27d1dc86, 0x845461b9) == 1);
    }
}

TEST_CASE("VariantAssigner with 123456789", "[assigner]") {
    VariantAssigner assigner(hash_unit("123456789"));

    SECTION("50/50 split") {
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000000) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000001) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x8015406f, 0x7ef49b98) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x3b2e7d90, 0xca87df4d) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x52c1f657, 0xd248bb2e) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x865a84d0, 0xaa22d41a) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x27d1dc86, 0x845461b9) == 0);
    }

    SECTION("33/33/34 split") {
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000000) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000001) == 1);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x8015406f, 0x7ef49b98) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x3b2e7d90, 0xca87df4d) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x52c1f657, 0xd248bb2e) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x865a84d0, 0xaa22d41a) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x27d1dc86, 0x845461b9) == 0);
    }
}

TEST_CASE("Fix: VariantAssigner probability never reaches 1.0", "[assigner][fix5]") {
    VariantAssigner assigner(hash_unit("test_max_hash"));
    int result = assigner.assign({0.5, 0.5}, 0x7FFFFFFF, 0x7FFFFFFF);
    REQUIRE(result >= 0);
    REQUIRE(result <= 1);
}

TEST_CASE("Fix: VariantAssigner works with negative int32_t seeds", "[assigner][fix19]") {
    VariantAssigner assigner(hash_unit("test_negative_seed"));
    int32_t neg_seed_hi = -1;
    int32_t neg_seed_lo = -1;
    int result = assigner.assign({0.5, 0.5}, neg_seed_hi, neg_seed_lo);
    REQUIRE(result >= 0);
    REQUIRE(result <= 1);

    int32_t min_seed = std::numeric_limits<int32_t>::min();
    result = assigner.assign({0.5, 0.5}, min_seed, min_seed);
    REQUIRE(result >= 0);
    REQUIRE(result <= 1);
}

TEST_CASE("VariantAssigner with e791e240fcd3df7d238cfc285f475e8152fcc0ec", "[assigner]") {
    VariantAssigner assigner(hash_unit("e791e240fcd3df7d238cfc285f475e8152fcc0ec"));

    SECTION("50/50 split") {
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000000) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x00000000, 0x00000001) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x8015406f, 0x7ef49b98) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x3b2e7d90, 0xca87df4d) == 1);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x52c1f657, 0xd248bb2e) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x865a84d0, 0xaa22d41a) == 0);
        REQUIRE(assigner.assign({0.5, 0.5}, 0x27d1dc86, 0x845461b9) == 0);
    }

    SECTION("33/33/34 split") {
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000000) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x00000000, 0x00000001) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x8015406f, 0x7ef49b98) == 2);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x3b2e7d90, 0xca87df4d) == 1);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x52c1f657, 0xd248bb2e) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x865a84d0, 0xaa22d41a) == 0);
        REQUIRE(assigner.assign({0.33, 0.33, 0.34}, 0x27d1dc86, 0x845461b9) == 1);
    }
}

// Shared cross-SDK UTF-8 vectors (validated against the JS SDK's
// stringToUint8Array/hashUnit and VariantAssigner). A unit is hashed over its
// UTF-8 bytes; non-BMP characters are the standard 4-byte sequence, and a lone
// surrogate (not representable in valid UTF-8) is U+FFFD (EF BF BD).
TEST_CASE("hash_unit and assignment with UTF-8 vectors", "[assigner][utf8]") {
    struct Vector {
        std::string value;
        std::vector<uint8_t> utf8;
        std::string hash;
    };

    const std::vector<Vector> vectors = {
        {"user\xF0\x9F\x98\x80id", {117, 115, 101, 114, 240, 159, 152, 128, 105, 100}, "nnPMxN6wY36LGRnuJ6jZrw"},
        {"\xF0\x9F\x98\x80", {240, 159, 152, 128}, "KgLqw51xanDs83V5GFkntg"},
        {"\xF0\xA0\x80\x8B", {240, 160, 128, 139}, "8mWvgvNGn-DZqb6Ft0HVjA"},
        {"a\xE2\x82\xAC\xF0\x9D\x84\x9Ez", {97, 226, 130, 172, 240, 157, 132, 158, 122}, "AGGq8R9K1XjO77q33izhLg"},
        {"\xF0\x9F\x87\xB5\xF0\x9F\x87\xB9", {240, 159, 135, 181, 240, 159, 135, 185}, "1RRpVphazpblARsm5DaSkg"},
        {"ab\xEF\xBF\xBD" "cd", {97, 98, 239, 191, 189, 99, 100}, "jvDxBUzS9Tdsml3QQStWSg"},
    };

    for (const auto& v : vectors) {
        CAPTURE(v.hash);
        REQUIRE(std::vector<uint8_t>(v.value.begin(), v.value.end()) == v.utf8);
        REQUIRE(hash_unit(v.value) == v.hash);
    }

    const int32_t seed_hi = 3603583;
    const int32_t seed_lo = 233373918;
    const std::vector<double> split = {0.5, 0.5};

    REQUIRE(VariantAssigner(hash_unit("\xF0\x9F\x98\x80")).assign(split, seed_hi, seed_lo) == 1);
    REQUIRE(VariantAssigner(hash_unit("\xF0\x9F\x87\xB5\xF0\x9F\x87\xB9")).assign(split, seed_hi, seed_lo) == 0);
    REQUIRE(VariantAssigner(hash_unit("a\xE2\x82\xAC\xF0\x9D\x84\x9Ez")).assign(split, seed_hi, seed_lo) == 1);
    REQUIRE(VariantAssigner(hash_unit("user\xF0\x9F\x98\x80id")).assign(split, seed_hi, seed_lo) == 0);
}
