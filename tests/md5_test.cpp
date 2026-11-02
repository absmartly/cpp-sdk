#include <catch2/catch_test_macros.hpp>
#include "absmartly/hashing.h"
#include <string>
#include <vector>

using namespace absmartly;

TEST_CASE("md5_hex matches known hashes", "[md5]") {
    REQUIRE(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    REQUIRE(md5_hex("a") == "0cc175b9c0f1b6a831c399e269772661");
    REQUIRE(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    REQUIRE(md5_hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    REQUIRE(md5_hex("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
    REQUIRE(md5_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") == "d174ab98d277d9f5a5611c2c9f419d9f");
    REQUIRE(md5_hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") == "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_CASE("md5_raw returns 16 bytes", "[md5]") {
    auto raw = md5_raw("");
    REQUIRE(raw.size() == 16);
    REQUIRE(raw[0] == 0xd4);
    REQUIRE(raw[1] == 0x1d);
    REQUIRE(raw[2] == 0x8c);
    REQUIRE(raw[3] == 0xd9);
}

TEST_CASE("md5 hash_unit base64url matches JS SDK", "[md5]") {
    struct TestCase {
        std::string input;
        std::string expected;
    };

    std::vector<TestCase> cases = {
        {"", "1B2M2Y8AsgTpgAmY7PhCfg"},
        {" ", "chXunH2dwinSkhpA6JnsXw"},
        {"t", "41jvpIn1gGLxDdcxa2Vkng"},
        {"te", "Vp73JkK-D63XEdakaNaO4Q"},
        {"tes", "KLZi2IO212_Zbk3cXpungA"},
        {"test", "CY9rzUYh03PK3k6DJie09g"},
        {"testy", "K5I_V6RgP8c6sYKz-TVn8g"},
        {"testy1", "8fT8xGipOhPkZ2DncKU-1A"},
        {"testy12", "YqRAtOz000gIu61ErEH18A"},
        {"testy123", "pfV2H07L6WvdqlY0zHuYIw"},

        // Characters outside the BMP are stored as UTF-16 surrogate pairs and must encode to 4-byte UTF-8; these canonical hashes are shared across all SDKs.
        {"😀", "KgLqw51xanDs83V5GFkntg"},
        {"😀😁", "ZJuDalvUWRJnVtkspj-2bQ"},
        {"世界你好", "v2CJG7YcjjWncKOSCzF2GA"},
        {"user_世界_123", "SCgk4OzXlFMvo1UMsP88fA"},
    };

    for (const auto& tc : cases) {
        CAPTURE(tc.input);
        auto raw = md5_raw(tc.input);
        auto encoded = base64url_no_padding(raw.data(), raw.size());
        REQUIRE(encoded == tc.expected);
    }
}

TEST_CASE("Fix: md5 block[15] high bits set for length encoding", "[md5][fix4]") {
    std::string input_56(56, 'a');
    auto hex = md5_hex(input_56);
    REQUIRE(hex.size() == 32);

    std::string input_64(64, 'b');
    auto hex2 = md5_hex(input_64);
    REQUIRE(hex2.size() == 32);
    REQUIRE(hex != hex2);

    std::string input_119(119, 'c');
    auto hex3 = md5_hex(input_119);
    REQUIRE(hex3.size() == 32);
}

TEST_CASE("md5 with longer strings", "[md5]") {
    auto raw = md5_raw("The quick brown fox jumps over the lazy dog");
    auto encoded = base64url_no_padding(raw.data(), raw.size());
    REQUIRE(encoded == "nhB9nTcrtoJr2B01QqQZ1g");

    raw = md5_raw("The quick brown fox jumps over the lazy dog and eats a pie");
    encoded = base64url_no_padding(raw.data(), raw.size());
    REQUIRE(encoded == "iM-8ECRrLUQzixl436y96A");

    raw = md5_raw("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
    encoded = base64url_no_padding(raw.data(), raw.size());
    REQUIRE(encoded == "24m7XOq4f5wPzCqzbBicLA");
}
