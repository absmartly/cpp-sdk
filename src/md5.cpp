#include "absmartly/hashing.h"
#include <cstring>
#include <cstdio>

namespace absmartly {

static inline uint32_t rotl(uint32_t a, int s) {
    return (a << s) | (a >> (32 - s));
}

static inline uint32_t cmn(uint32_t q, uint32_t a, uint32_t b, uint32_t x, int s, uint32_t t) {
    a = a + q + x + t;
    return rotl(a, s) + b;
}

static inline uint32_t ff(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t t) {
    return cmn((b & c) | (~b & d), a, b, x, s, t);
}

static inline uint32_t gg(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t t) {
    return cmn((b & d) | (c & ~d), a, b, x, s, t);
}

static inline uint32_t hh(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t t) {
    return cmn(b ^ c ^ d, a, b, x, s, t);
}

static inline uint32_t ii(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, int s, uint32_t t) {
    return cmn(c ^ (b | ~d), a, b, x, s, t);
}

static void md5cycle(uint32_t state[4], const uint32_t k[16]) {
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    a = ff(a, b, c, d, k[0],  7, 0xd76aa478);
    d = ff(d, a, b, c, k[1], 12, 0xe8c7b756);
    c = ff(c, d, a, b, k[2], 17, 0x242070db);
    b = ff(b, c, d, a, k[3], 22, 0xc1bdceee);
    a = ff(a, b, c, d, k[4],  7, 0xf57c0faf);
    d = ff(d, a, b, c, k[5], 12, 0x4787c62a);
    c = ff(c, d, a, b, k[6], 17, 0xa8304613);
    b = ff(b, c, d, a, k[7], 22, 0xfd469501);
    a = ff(a, b, c, d, k[8],  7, 0x698098d8);
    d = ff(d, a, b, c, k[9], 12, 0x8b44f7af);
    c = ff(c, d, a, b, k[10],17, 0xffff5bb1);
    b = ff(b, c, d, a, k[11],22, 0x895cd7be);
    a = ff(a, b, c, d, k[12], 7, 0x6b901122);
    d = ff(d, a, b, c, k[13],12, 0xfd987193);
    c = ff(c, d, a, b, k[14],17, 0xa679438e);
    b = ff(b, c, d, a, k[15],22, 0x49b40821);

    a = gg(a, b, c, d, k[1],  5, 0xf61e2562);
    d = gg(d, a, b, c, k[6],  9, 0xc040b340);
    c = gg(c, d, a, b, k[11],14, 0x265e5a51);
    b = gg(b, c, d, a, k[0], 20, 0xe9b6c7aa);
    a = gg(a, b, c, d, k[5],  5, 0xd62f105d);
    d = gg(d, a, b, c, k[10], 9, 0x02441453);
    c = gg(c, d, a, b, k[15],14, 0xd8a1e681);
    b = gg(b, c, d, a, k[4], 20, 0xe7d3fbc8);
    a = gg(a, b, c, d, k[9],  5, 0x21e1cde6);
    d = gg(d, a, b, c, k[14], 9, 0xc33707d6);
    c = gg(c, d, a, b, k[3], 14, 0xf4d50d87);
    b = gg(b, c, d, a, k[8], 20, 0x455a14ed);
    a = gg(a, b, c, d, k[13], 5, 0xa9e3e905);
    d = gg(d, a, b, c, k[2],  9, 0xfcefa3f8);
    c = gg(c, d, a, b, k[7], 14, 0x676f02d9);
    b = gg(b, c, d, a, k[12],20, 0x8d2a4c8a);

    a = hh(a, b, c, d, k[5],  4, 0xfffa3942);
    d = hh(d, a, b, c, k[8], 11, 0x8771f681);
    c = hh(c, d, a, b, k[11],16, 0x6d9d6122);
    b = hh(b, c, d, a, k[14],23, 0xfde5380c);
    a = hh(a, b, c, d, k[1],  4, 0xa4beea44);
    d = hh(d, a, b, c, k[4], 11, 0x4bdecfa9);
    c = hh(c, d, a, b, k[7], 16, 0xf6bb4b60);
    b = hh(b, c, d, a, k[10],23, 0xbebfbc70);
    a = hh(a, b, c, d, k[13], 4, 0x289b7ec6);
    d = hh(d, a, b, c, k[0], 11, 0xeaa127fa);
    c = hh(c, d, a, b, k[3], 16, 0xd4ef3085);
    b = hh(b, c, d, a, k[6], 23, 0x04881d05);
    a = hh(a, b, c, d, k[9],  4, 0xd9d4d039);
    d = hh(d, a, b, c, k[12],11, 0xe6db99e5);
    c = hh(c, d, a, b, k[15],16, 0x1fa27cf8);
    b = hh(b, c, d, a, k[2], 23, 0xc4ac5665);

    a = ii(a, b, c, d, k[0],  6, 0xf4292244);
    d = ii(d, a, b, c, k[7], 10, 0x432aff97);
    c = ii(c, d, a, b, k[14],15, 0xab9423a7);
    b = ii(b, c, d, a, k[5], 21, 0xfc93a039);
    a = ii(a, b, c, d, k[12], 6, 0x655b59c3);
    d = ii(d, a, b, c, k[3], 10, 0x8f0ccc92);
    c = ii(c, d, a, b, k[10],15, 0xffeff47d);
    b = ii(b, c, d, a, k[1], 21, 0x85845dd1);
    a = ii(a, b, c, d, k[8],  6, 0x6fa87e4f);
    d = ii(d, a, b, c, k[15],10, 0xfe2ce6e0);
    c = ii(c, d, a, b, k[6], 15, 0xa3014314);
    b = ii(b, c, d, a, k[13],21, 0x4e0811a1);
    a = ii(a, b, c, d, k[4],  6, 0xf7537e82);
    d = ii(d, a, b, c, k[11],10, 0xbd3af235);
    c = ii(c, d, a, b, k[2], 15, 0x2ad7d2bb);
    b = ii(b, c, d, a, k[9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

static inline uint32_t get_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
}

static void md5_compute(const uint8_t* data, size_t len, uint32_t state_out[4]) {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    uint32_t block[16];

    size_t n = len & ~static_cast<size_t>(63);
    size_t i = 0;

    for (; i < n; i += 64) {
        for (int w = 0; w < 16; ++w) {
            block[w] = get_le32(data + i + (w << 2));
        }
        md5cycle(state, block);
    }

    int w = 0;
    size_t m = len & ~static_cast<size_t>(3);
    for (; i < m; i += 4) {
        block[w++] = get_le32(data + i);
    }

    size_t p = len & 3;
    switch (p) {
        case 3:
            block[w++] = 0x80000000u
                       | static_cast<uint32_t>(data[i])
                       | (static_cast<uint32_t>(data[i + 1]) << 8)
                       | (static_cast<uint32_t>(data[i + 2]) << 16);
            break;
        case 2:
            block[w++] = 0x00800000u
                       | static_cast<uint32_t>(data[i])
                       | (static_cast<uint32_t>(data[i + 1]) << 8);
            break;
        case 1:
            block[w++] = 0x00008000u
                       | static_cast<uint32_t>(data[i]);
            break;
        default:
            block[w++] = 0x00000080u;
            break;
    }

    if (w > 14) {
        for (; w < 16; ++w) {
            block[w] = 0;
        }
        md5cycle(state, block);
        w = 0;
    }

    for (; w < 16; ++w) {
        block[w] = 0;
    }

    block[14] = static_cast<uint32_t>(len << 3);
    block[15] = static_cast<uint32_t>(len >> 29);
    md5cycle(state, block);

    state_out[0] = state[0];
    state_out[1] = state[1];
    state_out[2] = state[2];
    state_out[3] = state[3];
}

std::vector<uint8_t> md5_raw(const std::string& input) {
    const auto* data = reinterpret_cast<const uint8_t*>(input.data());
    uint32_t state[4];
    md5_compute(data, input.size(), state);

    std::vector<uint8_t> result(16);
    for (int i = 0; i < 4; ++i) {
        result[i * 4 + 0] = static_cast<uint8_t>(state[i] & 0xFF);
        result[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 8) & 0xFF);
        result[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
        result[i * 4 + 3] = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
    }
    return result;
}

std::string md5_hex(const std::string& input) {
    auto raw = md5_raw(input);
    std::string hex;
    hex.reserve(32);
    static const char digits[] = "0123456789abcdef";
    for (uint8_t byte : raw) {
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0x0F]);
    }
    return hex;
}

} // namespace absmartly
