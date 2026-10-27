#include "absmartly/hashing.h"
#include <cstring>

namespace absmartly {

static constexpr uint32_t C1 = 0xcc9e2d51;
static constexpr uint32_t C2 = 0x1b873593;
static constexpr uint32_t C3 = 0xe6546b64;

static inline uint32_t rotl32(uint32_t a, int b) {
    return (a << b) | (a >> (32 - b));
}

static inline uint32_t scramble32(uint32_t block) {
    block *= C1;
    block = rotl32(block, 15);
    block *= C2;
    return block;
}

static inline uint32_t fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static inline uint32_t get_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
}

uint32_t murmur3_32(const uint8_t* data, size_t len, uint32_t seed) {
    uint32_t h = seed;

    size_t n = len & ~static_cast<size_t>(3);
    size_t i = 0;

    for (; i < n; i += 4) {
        uint32_t chunk = get_le32(data + i);
        h ^= scramble32(chunk);
        h = rotl32(h, 13);
        h = h * 5 + C3;
    }

    uint32_t remaining = 0;
    switch (len & 3) {
        case 3:
            remaining ^= static_cast<uint32_t>(data[i + 2]) << 16;
            [[fallthrough]];
        case 2:
            remaining ^= static_cast<uint32_t>(data[i + 1]) << 8;
            [[fallthrough]];
        case 1:
            remaining ^= static_cast<uint32_t>(data[i]);
            h ^= scramble32(remaining);
            [[fallthrough]];
        default:
            break;
    }

    h ^= static_cast<uint32_t>(len);
    h = fmix32(h);
    return h;
}

uint32_t murmur3_32(const std::string& str, uint32_t seed) {
    return murmur3_32(reinterpret_cast<const uint8_t*>(str.data()), str.size(), seed);
}

} // namespace absmartly
