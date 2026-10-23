#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace absmartly {

class VariantAssigner {
public:
    explicit VariantAssigner(const std::string& hashed_unit);
    int assign(const std::vector<double>& split, int32_t seed_hi, int32_t seed_lo) const;

private:
    uint32_t unit_hash_;
};

} // namespace absmartly
