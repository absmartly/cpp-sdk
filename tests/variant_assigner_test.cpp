#include <catch2/catch_test_macros.hpp>
#include "absmartly/variant_assigner.h"
#include "absmartly/hashing.h"
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
