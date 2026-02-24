#include "absmartly/variant_assigner.h"
#include "absmartly/hashing.h"
#include <cstring>

namespace absmartly {

VariantAssigner::VariantAssigner(const std::string& hashed_unit)
    : unit_hash_(murmur3_32(hashed_unit, 0)) {}

static inline void put_le32(uint8_t* buf, uint32_t val) {
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

int VariantAssigner::assign(const std::vector<double>& split, int seed_hi, int seed_lo) const {
    uint8_t buffer[12];
    put_le32(buffer, static_cast<uint32_t>(seed_lo));
    put_le32(buffer + 4, static_cast<uint32_t>(seed_hi));
    put_le32(buffer + 8, unit_hash_);

    uint32_t hash = murmur3_32(buffer, 12, 0);
    double probability = static_cast<double>(hash) / 0xFFFFFFFF;
    return choose_variant(split, probability);
}

} // namespace absmartly
