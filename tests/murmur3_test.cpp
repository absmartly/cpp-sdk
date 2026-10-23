#include <catch2/catch_test_macros.hpp>
#include "absmartly/hashing.h"
#include <string>
#include <vector>
#include <cstdint>

using namespace absmartly;

static std::vector<uint8_t> utf8_bytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

TEST_CASE("murmur3_32 matches known hashes with seed 0", "[murmur3]") {
    struct TestCase {
        std::string input;
        uint32_t seed;
        uint32_t expected;
    };

    std::vector<TestCase> cases = {
        {"", 0x00000000, 0x00000000},
        {" ", 0x00000000, 0x7ef49b98},
        {"t", 0x00000000, 0xca87df4d},
        {"te", 0x00000000, 0xedb8ee1b},
        {"tes", 0x00000000, 0x0bb90e5a},
        {"test", 0x00000000, 0xba6bd213},
        {"testy", 0x00000000, 0x44af8342},
        {"testy1", 0x00000000, 0x8a1a243a},
        {"testy12", 0x00000000, 0x845461b9},
        {"testy123", 0x00000000, 0x47628ac4},
        {"The quick brown fox jumps over the lazy dog", 0x00000000, 0x2e4ff723},

        {"", 0xdeadbeef, 0x0de5c6a9},
        {" ", 0xdeadbeef, 0x25acce43},
        {"t", 0xdeadbeef, 0x3b15dcf8},
        {"te", 0xdeadbeef, 0xac981332},
        {"tes", 0xdeadbeef, 0xc1c78dda},
        {"test", 0xdeadbeef, 0xaa22d41a},
        {"testy", 0xdeadbeef, 0x84f5f623},
        {"testy1", 0xdeadbeef, 0x09ed28e9},
        {"testy12", 0xdeadbeef, 0x22467835},
        {"testy123", 0xdeadbeef, 0xd633060d},
        {"The quick brown fox jumps over the lazy dog", 0xdeadbeef, 0x3a7b3f4d},

        {"", 0x00000001, 0x514e28b7},
        {" ", 0x00000001, 0x4f0f7132},
        {"t", 0x00000001, 0x5db1831e},
        {"te", 0x00000001, 0xd248bb2e},
        {"tes", 0x00000001, 0xd432eb74},
        {"test", 0x00000001, 0x99c02ae2},
        {"testy", 0x00000001, 0xc5b2dc1e},
        {"testy1", 0x00000001, 0x33925ceb},
        {"testy12", 0x00000001, 0xd92c9f23},
        {"testy123", 0x00000001, 0x3bc1712d},
        {"The quick brown fox jumps over the lazy dog", 0x00000001, 0x78e69e27},
    };

    for (const auto& tc : cases) {
        auto bytes = utf8_bytes(tc.input);
        CAPTURE(tc.input, tc.seed);
        REQUIRE(murmur3_32(bytes.data(), bytes.size(), tc.seed) == tc.expected);
    }
}

TEST_CASE("murmur3_32 string overload matches buffer overload", "[murmur3]") {
    REQUIRE(murmur3_32("test", 0) == 0xba6bd213);
    REQUIRE(murmur3_32("test", 0xdeadbeef) == 0xaa22d41a);
    REQUIRE(murmur3_32("test", 1) == 0x99c02ae2);
}

TEST_CASE("murmur3_32 with UTF-8 multi-byte characters", "[murmur3]") {
    std::vector<uint8_t> special = {
        's', 'p', 'e', 'c', 'i', 'a', 'l', ' ',
        'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', ' ',
        0x61, 0xc3, 0xa7, 0x62, 0xe2, 0x86, 0x93, 0x63
    };
    REQUIRE(murmur3_32(special.data(), special.size(), 0) == 0xbe83b140);
    REQUIRE(murmur3_32(special.data(), special.size(), 0xdeadbeef) == 0xf7fdd8a2);
    REQUIRE(murmur3_32(special.data(), special.size(), 1) == 0x293327b5);
}

TEST_CASE("murmur3_32 specific SDK test vectors", "[murmur3]") {
    REQUIRE(murmur3_32("absmartly.com", 0) == 0x727245C3);

    std::vector<uint8_t> bleh = {
        'b', 'l', 'e', 'h', '@', 'a', 'b', 's',
        'm', 'a', 'r', 't', 'l', 'y', '.', 'c', 'o', 'm'
    };
    REQUIRE(murmur3_32(bleh.data(), bleh.size(), 0) == 0x3660C387);
}
