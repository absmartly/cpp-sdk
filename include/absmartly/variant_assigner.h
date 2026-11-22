#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace absmartly {

class VariantAssigner {
public:
    explicit VariantAssigner(const std::string& hashed_unit);
    int assign(const std::vector<double>& split, int32_t seed_hi, int32_t seed_lo) const;
    // As javascript-sdk and java-sdk: the hash scaled by 1/0xffffffff, so a hash can land
    // exactly on a split boundary. Used for holdouts; experiments keep assign()'s scale.
    int assign_holdout(const std::vector<double>& split, int32_t seed_hi, int32_t seed_lo) const;

private:
    uint32_t hash(int32_t seed_hi, int32_t seed_lo) const;

    uint32_t unit_hash_;
};

} // namespace absmartly
