#include "switchback_internal.h"

#include "absmartly/hashing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace absmartly {

namespace {

constexpr char SEPARATOR = '\x1f';
constexpr double NORMALIZER = 1.0 / 0xffffffffULL;
constexpr int64_t ANCHOR_ALIGNMENT_MS = 3600000;
// 10000-01-01T00:00:00Z: the Collector's Registry serves no anchor at or after it.
constexpr int64_t MAX_ANCHOR_MS = 253402300800000;

void put_le32(uint8_t* buffer, uint32_t value) {
    buffer[0] = static_cast<uint8_t>(value);
    buffer[1] = static_cast<uint8_t>(value >> 8);
    buffer[2] = static_cast<uint8_t>(value >> 16);
    buffer[3] = static_cast<uint8_t>(value >> 24);
}

void require_arms(int arm_count) {
    if (arm_count < 2) {
        throw std::invalid_argument("armCount must be >= 2, got " + std::to_string(arm_count));
    }
}

// murmur3_32(le64(seed) ++ le32(murmur3_32(utf8(clusterUnit + "\x1f" + cycleIndex)))). The key
// is hashed as the bytes given: a std::string holds UTF-8 already.
uint32_t base_hash(const std::string& cluster_unit, int64_t cycle_index, int64_t seed) {
    std::string key;
    key.reserve(cluster_unit.size() + 21);
    key += cluster_unit;
    key += SEPARATOR;
    key += std::to_string(cycle_index);
    const uint32_t key_hash = murmur3_32(key, 0);

    const auto bits = static_cast<uint64_t>(seed);
    uint8_t buffer[12];
    put_le32(buffer, static_cast<uint32_t>(bits));
    put_le32(buffer + 4, static_cast<uint32_t>(bits >> 32));
    put_le32(buffer + 8, key_hash);
    return murmur3_32(buffer, sizeof(buffer), 0);
}

std::vector<int> fisher_yates(int arm_count, uint32_t base) {
    std::vector<int> permutation(static_cast<size_t>(arm_count));
    for (int i = 0; i < arm_count; ++i) {
        permutation[static_cast<size_t>(i)] = i;
    }
    uint8_t buffer[8];
    put_le32(buffer, base);
    for (int i = arm_count - 1; i > 0; --i) {
        put_le32(buffer + 4, static_cast<uint32_t>(i));
        const double probability = murmur3_32(buffer, sizeof(buffer), 0) * NORMALIZER;
        // probability == 1.0 would map to i + 1.
        const int j = std::min(i, static_cast<int>(std::floor(probability * (i + 1))));
        std::swap(permutation[static_cast<size_t>(i)], permutation[static_cast<size_t>(j)]);
    }
    return permutation;
}

} // namespace

int assign_switchback(const std::string& cluster_unit, int64_t period_index, int64_t seed, int arm_count) {
    require_arms(arm_count);
    const int64_t cycle_index = internal::floor_div(period_index, arm_count);
    // floorMod, computed modulo 2^64: the product can wrap near INT64_MIN, the difference cannot.
    const auto slot = static_cast<size_t>(static_cast<uint64_t>(period_index) -
                                          static_cast<uint64_t>(cycle_index) * static_cast<uint64_t>(arm_count));
    return fisher_yates(arm_count, base_hash(cluster_unit, cycle_index, seed))[slot];
}

std::vector<int> switchback_permutation(const std::string& cluster_unit, int64_t cycle_index, int64_t seed,
                                        int arm_count) {
    require_arms(arm_count);
    return fisher_yates(arm_count, base_hash(cluster_unit, cycle_index, seed));
}

namespace internal {

int64_t floor_div(int64_t dividend, int64_t divisor) {
    const int64_t quotient = dividend / divisor;
    return (dividend % divisor != 0 && ((dividend < 0) != (divisor < 0))) ? quotient - 1 : quotient;
}

std::optional<int32_t> try_parse_int(std::string_view s) {
    if (s.empty()) {
        return std::nullopt;
    }
    int32_t value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<SwitchbackPath> SwitchbackPath::parse_supported(std::string_view path) {
    // A '.' anywhere is malformed, quoted or not: dotted paths are not a nesting syntax.
    if (path.find('.') != std::string_view::npos) {
        return std::nullopt;
    }
    SwitchbackPath parsed;
    size_t i = 0;
    while (i < path.size()) {
        if (path[i] == '/') {
            ++i;
            continue;
        }
        const char first = path[i];
        if (first == '\'' || first == '"') {
            const size_t end = path.find(first, i + 1);
            if (end == std::string_view::npos) {
                return std::nullopt;
            }
            parsed.keys.emplace_back(path.substr(i + 1, end - i - 1));
            parsed.indices.emplace_back(std::nullopt);
            i = end + 1;
        } else {
            const size_t end = std::min(path.find('/', i), path.size());
            const std::string_view key = path.substr(i, end - i);
            parsed.keys.emplace_back(key);
            parsed.indices.push_back(try_parse_int(key));
            i = end;
        }
    }
    const size_t n = parsed.keys.size();
    if (n == 0) {
        return std::nullopt;
    }
    for (size_t k = 0; k < n; ++k) {
        const auto& index = parsed.indices[k];
        if (index.has_value() && (*index < 0 || k == 0 || k == n - 1 || parsed.indices[k - 1].has_value())) {
            return std::nullopt;
        }
    }
    return parsed;
}

static const nlohmann::json* resolve_from(const SwitchbackPath& path, const nlohmann::json* current, size_t first) {
    for (size_t k = first; k < path.keys.size() && current != nullptr; ++k) {
        const auto& index = path.indices[k];
        if (index.has_value()) {
            current = (current->is_array() && static_cast<size_t>(*index) < current->size())
                          ? &(*current)[static_cast<size_t>(*index)]
                          : nullptr;
        } else if (current->is_object()) {
            const auto it = current->find(path.keys[k]);
            current = it != current->end() ? &*it : nullptr;
        } else {
            current = nullptr;
        }
    }
    return current;
}

const nlohmann::json* SwitchbackPath::resolve_attribute(const nlohmann::json& root_value) const {
    return resolve_from(*this, &root_value, 1);
}

const nlohmann::json* SwitchbackPath::resolve(const nlohmann::json& attributes) const {
    return resolve_from(*this, &attributes, 0);
}

std::optional<std::string> normalize_cluster_key(const nlohmann::json& value) {
    if (value.is_string()) {
        const auto& s = value.get_ref<const std::string&>();
        return s.empty() ? std::nullopt : std::optional<std::string>(s);
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<uint64_t>());
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<int64_t>());
    }
    return std::nullopt;
}

std::string_view java_trim(std::string_view s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && static_cast<unsigned char>(s[begin]) <= ' ') ++begin;
    while (end > begin && static_cast<unsigned char>(s[end - 1]) <= ' ') --end;
    return s.substr(begin, end - begin);
}

bool SwitchbackIndex::same_identity(const SwitchbackIndex& previous) const {
    return id == previous.id && iteration == previous.iteration && clusters == previous.clusters &&
           cluster_values == previous.cluster_values && start_at == previous.start_at &&
           period_ms == previous.period_ms && design_type == previous.design_type &&
           ((!sources.empty() && !previous.sources.empty()) || unit_type == previous.unit_type);
}

int64_t SwitchbackIndex::period(int64_t now) const {
    if (sources.empty() || !servable || now < *start_at) {
        return NO_SWITCHBACK_PERIOD;
    }
    return floor_div(now - *start_at, *period_ms);
}

bool SwitchbackIndex::allowed(const std::string& key) const {
    return !cluster_values.has_value() || cluster_values->empty() ||
           std::find(cluster_values->begin(), cluster_values->end(), key) != cluster_values->end();
}

SwitchbackIndex make_switchback_index(const ExperimentData& experiment) {
    SwitchbackIndex index;
    index.id = experiment.id;
    index.iteration = experiment.iteration;
    index.unit_type = experiment.unitType;
    index.clusters = experiment.switchbackClusters;
    index.cluster_values = experiment.switchbackClusterValues;
    index.start_at = experiment.switchbackStartAt;
    index.period_ms = experiment.switchbackPeriodMs;
    index.design_type = experiment.switchbackDesignType;

    const auto& clusters = experiment.switchbackClusters;
    if (clusters.is_object()) {
        for (auto it = clusters.begin(); it != clusters.end(); ++it) {
            std::optional<SwitchbackPath> path;
            if (it.value().is_string()) {
                path = SwitchbackPath::parse_supported(it.value().get_ref<const std::string&>());
            }
            if (!path.has_value()) {
                index.sources.clear();
                break;
            }
            index.sources.push_back({it.key(), std::move(*path)});
        }
    }

    const auto& start = experiment.switchbackStartAt;
    const auto& period = experiment.switchbackPeriodMs;
    index.servable = start.has_value() && *start >= 0 && *start < MAX_ANCHOR_MS &&
                     *start % ANCHOR_ALIGNMENT_MS == 0 && period.has_value() && *period > 0 &&
                     experiment.split.size() >= 2;
    index.seed = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(experiment.seedHi)) << 32) |
                                      static_cast<uint32_t>(experiment.seedLo));
    index.arm_count = static_cast<int>(experiment.split.size());
    return index;
}

} // namespace internal

} // namespace absmartly
