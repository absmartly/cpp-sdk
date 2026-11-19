#pragma once
#include "absmartly/models.h"
#include "absmartly/switchback.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace absmartly::internal {

constexpr int64_t NO_SWITCHBACK_PERIOD = INT64_MIN;
int64_t floor_div(int64_t dividend, int64_t divisor);
std::optional<int32_t> try_parse_int(std::string_view s);
std::string_view java_trim(std::string_view s);

struct SwitchbackPath {
    std::vector<std::string> keys;
    std::vector<std::optional<int32_t>> indices;
    static std::optional<SwitchbackPath> parse_supported(std::string_view path);
    const std::string& root_key() const { return keys.front(); }
    const nlohmann::json* resolve_attribute(const nlohmann::json& root_value) const;
    const nlohmann::json* resolve(const nlohmann::json& attributes) const;
};

std::optional<std::string> normalize_cluster_key(const nlohmann::json& value);

struct SwitchbackIndex {
    uint64_t generation = 0;
    int id = 0;
    int iteration = 0;
    std::string unit_type;
    nlohmann::ordered_json clusters;
    std::optional<std::vector<std::string>> cluster_values;
    std::optional<int64_t> start_at;
    std::optional<int64_t> period_ms;
    std::string design_type;
    // A linked unit type and the attribute path it reads, in served order.
    struct Source {
        std::string type;
        SwitchbackPath path;
    };
    // Empty for the legacy path: no cluster map, or one with an unreadable entry.
    std::vector<Source> sources;
    bool servable = false;
    int64_t seed = 0;
    int arm_count = 0;
    bool same_identity(const SwitchbackIndex& previous) const;
    int64_t period(int64_t now) const;
    bool allowed(const std::string& key) const;
};

SwitchbackIndex make_switchback_index(const ExperimentData& experiment);

}
