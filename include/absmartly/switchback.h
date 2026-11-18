#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace absmartly {

// The platform's balanced-block switchback draw (lib/utils SwitchbackAssignment, ported from
// java-sdk SwitchbackAssigner): the variant a cluster gets in a period. cluster_unit is the raw
// cluster key, never hashed. seed is (seedHi << 32) | (seedLo & 0xffffffff). Throws
// std::invalid_argument when arm_count < 2.
int assign_switchback(const std::string& cluster_unit, int64_t period_index, int64_t seed, int arm_count);

// The permutation of [0, arm_count) that cycle cycle_index serves, in period order:
// assign_switchback(u, p, s, n) is switchback_permutation(u, floorDiv(p, n), s, n)[floorMod(p, n)].
// Throws std::invalid_argument when arm_count < 2.
std::vector<int> switchback_permutation(const std::string& cluster_unit, int64_t cycle_index, int64_t seed,
                                        int arm_count);

}
