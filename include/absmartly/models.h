#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <climits>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace absmartly {

struct ExperimentVariant {
    std::string name;
    nlohmann::json config;
};

inline void to_json(nlohmann::json& j, const ExperimentVariant& v) {
    j = nlohmann::json{{"name", v.name}, {"config", v.config}};
}

inline void from_json(const nlohmann::json& j, ExperimentVariant& v) {
    j.at("name").get_to(v.name);
    v.config = j.value("config", nlohmann::json());
}

struct CustomFieldValue {
    std::string name;
    std::string type;
    std::optional<std::string> value;
};

inline void to_json(nlohmann::json& j, const CustomFieldValue& v) {
    j = nlohmann::json{{"name", v.name}, {"type", v.type}};
    if (v.value.has_value()) {
        j["value"] = v.value.value();
    } else {
        j["value"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, CustomFieldValue& v) {
    j.at("name").get_to(v.name);
    j.at("type").get_to(v.type);
    if (j.contains("value") && !j.at("value").is_null()) {
        v.value = j.at("value").get<std::string>();
    } else {
        v.value = std::nullopt;
    }
}

struct ExperimentData {
    int id = 0;
    std::string name;
    std::string unitType;
    int iteration = 0;
    int32_t seedHi = 0;
    int32_t seedLo = 0;
    std::vector<double> split;
    bool seedStrictMode = false;
    int32_t trafficSeedHi = 0;
    int32_t trafficSeedLo = 0;
    std::vector<double> trafficSplit;
    int fullOnVariant = 0;
    nlohmann::json audience;
    std::vector<ExperimentVariant> variants;
    bool audienceStrict = false;
    std::vector<CustomFieldValue> customFieldValues;
    std::string application;
    std::string environment;
    // JSON string of ordered assignment rules ({"rules": [...]}); empty when the
    // experiment has none. A value sent as anything but a string is ignored.
    std::string assignmentRules;
    // Ids of the holdouts covering this experiment, resolved against
    // ContextData::holdouts; an id with no usable holdout is ignored.
    std::vector<int> holdoutIds;
    // Carried for wire fidelity only: a holdout's arm count comes from its split.
    std::string holdoutType;
    // Switchback configuration (R33). randomizationType is "switchback" for a switchback and
    // "unit" or empty for a unit experiment. Instants and durations are epoch/duration
    // milliseconds; the carryover and burn windows are analysis-only and never read.
    std::string randomizationType;
    // Unit type name -> attribute path, as served. The order is semantic (the first linked unit
    // with a value serves), so it is kept as decoded: decode with nlohmann::ordered_json to keep
    // the served order, since nlohmann::json sorts object keys. Null when absent.
    nlohmann::ordered_json switchbackClusters;
    // Allowed cluster values; nullopt or empty admits every value.
    std::optional<std::vector<std::string>> switchbackClusterValues;
    std::optional<int64_t> switchbackStartAt;
    std::optional<int64_t> switchbackPeriodMs;
    std::optional<int64_t> switchbackCarryoverDurationMs;
    std::optional<int64_t> switchbackBurnInDurationMs;
    std::optional<int64_t> switchbackBurnOutDurationMs;
    std::string switchbackDesignType;
    std::string switchbackEstimand;
};

inline void to_json(nlohmann::json& j, const ExperimentData& e) {
    j = nlohmann::json{
        {"id", e.id},
        {"name", e.name},
        {"unitType", e.unitType},
        {"iteration", e.iteration},
        {"seedHi", e.seedHi},
        {"seedLo", e.seedLo},
        {"split", e.split},
        {"seedStrictMode", e.seedStrictMode},
        {"trafficSeedHi", e.trafficSeedHi},
        {"trafficSeedLo", e.trafficSeedLo},
        {"trafficSplit", e.trafficSplit},
        {"fullOnVariant", e.fullOnVariant},
        {"audience", e.audience},
        {"variants", e.variants},
        {"audienceStrict", e.audienceStrict},
        {"customFieldValues", e.customFieldValues},
        {"application", e.application},
        {"environment", e.environment}
    };
    if (!e.assignmentRules.empty()) {
        j["assignmentRules"] = e.assignmentRules;
    }
    if (!e.holdoutIds.empty()) {
        j["holdoutIds"] = e.holdoutIds;
    }
    if (!e.holdoutType.empty()) {
        j["holdoutType"] = e.holdoutType;
    }
    if (!e.randomizationType.empty()) {
        j["randomizationType"] = e.randomizationType;
    }
    if (!e.switchbackClusters.is_null()) {
        j["switchbackClusters"] = e.switchbackClusters;
    }
    if (e.switchbackClusterValues.has_value()) {
        j["switchbackClusterValues"] = *e.switchbackClusterValues;
    }
    const std::pair<const char*, const std::optional<int64_t>*> grid[] = {
        {"switchbackStartAt", &e.switchbackStartAt},
        {"switchbackPeriodMs", &e.switchbackPeriodMs},
        {"switchbackCarryoverDurationMs", &e.switchbackCarryoverDurationMs},
        {"switchbackBurnInDurationMs", &e.switchbackBurnInDurationMs},
        {"switchbackBurnOutDurationMs", &e.switchbackBurnOutDurationMs},
    };
    for (const auto& [name, value] : grid) {
        if (value->has_value()) {
            j[name] = **value;
        }
    }
    if (!e.switchbackDesignType.empty()) {
        j["switchbackDesignType"] = e.switchbackDesignType;
    }
    if (!e.switchbackEstimand.empty()) {
        j["switchbackEstimand"] = e.switchbackEstimand;
    }
}

namespace detail {

// A JSON integer (or integral float) that fits an int, as JS reads a holdout id.
inline bool get_int(const nlohmann::json& j, int& out) {
    if (j.is_number_integer()) {
        if (j.is_number_unsigned()) {
            const auto value = j.get<uint64_t>();
            if (value > static_cast<uint64_t>(INT32_MAX)) return false;
            out = static_cast<int>(value);
            return true;
        }
        const auto value = j.get<int64_t>();
        if (value < INT32_MIN || value > INT32_MAX) return false;
        out = static_cast<int>(value);
        return true;
    }
    if (j.is_number_float()) {
        const double value = j.get<double>();
        if (!(value >= INT32_MIN && value <= INT32_MAX) ||
            std::islessgreater(static_cast<double>(static_cast<int>(value)), value)) {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    }
    return false;
}

// A seed as JS reads it (ECMAScript ToInt32 of a JSON number, which JS holds as a
// double): truncated toward zero and wrapped to 32 bits, so 4294967297 reads as 1
// and 1e20 as 1661992960.
inline int32_t to_int32(double d) {
    if (!std::isfinite(d)) return 0;
    double wrapped = std::fmod(std::trunc(d), 4294967296.0);
    if (wrapped < 0) wrapped += 4294967296.0;
    return static_cast<int32_t>(static_cast<uint32_t>(wrapped));
}

inline bool get_seed(const nlohmann::json& j, int32_t& out) {
    if (!j.is_number()) return false;
    // Exact for integers within 2^53; larger ones round to a double first, as in JS.
    if (j.is_number_unsigned()) {
        const auto value = j.get<uint64_t>();
        out = value <= (1ULL << 53) ? static_cast<int32_t>(static_cast<uint32_t>(value))
                                    : to_int32(static_cast<double>(value));
    } else if (j.is_number_integer()) {
        const auto value = j.get<int64_t>();
        out = (value <= (1LL << 53) && value >= -(1LL << 53))
                  ? static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(value)))
                  : to_int32(static_cast<double>(value));
    } else {
        out = to_int32(j.get<double>());
    }
    return true;
}

// A JSON integer, or an integral float, that fits an int64_t. Anything else is absent.
inline std::optional<int64_t> get_int64(const nlohmann::json& j) {
    if (j.is_number_unsigned()) {
        const auto value = j.get<uint64_t>();
        if (value > static_cast<uint64_t>(INT64_MAX)) return std::nullopt;
        return static_cast<int64_t>(value);
    }
    if (j.is_number_integer()) {
        return j.get<int64_t>();
    }
    if (j.is_number_float()) {
        const double value = j.get<double>();
        // [-2^63, 2^63): the double range that converts to int64_t without overflow.
        if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0) ||
            std::islessgreater(std::trunc(value), value)) {
            return std::nullopt;
        }
        return static_cast<int64_t>(value);
    }
    return std::nullopt;
}

// The switchback fields are replaced on every decode, like the holdout coverage. An allow-list
// entry that is neither a string nor an integer is kept as "", which no cluster key equals (a
// key is never empty), so a malformed entry never widens the list.
inline void decode_switchback(const nlohmann::json& j, ExperimentData& e) {
    e.randomizationType.clear();
    e.switchbackClusters = nullptr;
    e.switchbackClusterValues.reset();
    e.switchbackStartAt.reset();
    e.switchbackPeriodMs.reset();
    e.switchbackCarryoverDurationMs.reset();
    e.switchbackBurnInDurationMs.reset();
    e.switchbackBurnOutDurationMs.reset();
    e.switchbackDesignType.clear();
    e.switchbackEstimand.clear();
    if (!j.is_object()) {
        return;
    }
    const auto string_field = [&j](const char* name, std::string& out) {
        const auto it = j.find(name);
        if (it != j.end() && it->is_string()) {
            out = it->get<std::string>();
        }
    };
    string_field("randomizationType", e.randomizationType);
    string_field("switchbackDesignType", e.switchbackDesignType);
    string_field("switchbackEstimand", e.switchbackEstimand);

    const auto clusters = j.find("switchbackClusters");
    if (clusters != j.end() && clusters->is_object()) {
        e.switchbackClusters = *clusters;
    }
    const auto values = j.find("switchbackClusterValues");
    if (values != j.end() && values->is_array()) {
        std::vector<std::string> allowed;
        allowed.reserve(values->size());
        for (const auto& value : *values) {
            if (value.is_string()) {
                allowed.push_back(value.get<std::string>());
            } else if (value.is_number_unsigned()) {
                allowed.push_back(std::to_string(value.get<uint64_t>()));
            } else if (value.is_number_integer()) {
                allowed.push_back(std::to_string(value.get<int64_t>()));
            } else {
                allowed.emplace_back();
            }
        }
        e.switchbackClusterValues = std::move(allowed);
    }
    const std::pair<const char*, std::optional<int64_t>*> grid[] = {
        {"switchbackStartAt", &e.switchbackStartAt},
        {"switchbackPeriodMs", &e.switchbackPeriodMs},
        {"switchbackCarryoverDurationMs", &e.switchbackCarryoverDurationMs},
        {"switchbackBurnInDurationMs", &e.switchbackBurnInDurationMs},
        {"switchbackBurnOutDurationMs", &e.switchbackBurnOutDurationMs},
    };
    for (const auto& [name, value] : grid) {
        const auto it = j.find(name);
        if (it != j.end()) {
            *value = get_int64(*it);
        }
    }
}

} // namespace detail

inline void from_json(const nlohmann::json& j, ExperimentData& e) {
    j.at("id").get_to(e.id);
    j.at("name").get_to(e.name);
    j.at("unitType").get_to(e.unitType);
    j.at("iteration").get_to(e.iteration);
    j.at("seedHi").get_to(e.seedHi);
    j.at("seedLo").get_to(e.seedLo);
    if (j.contains("split") && j["split"].is_array()) {
        j.at("split").get_to(e.split);
    }
    if (j.contains("seedStrictMode") && j["seedStrictMode"].is_boolean()) {
        j.at("seedStrictMode").get_to(e.seedStrictMode);
    }
    if (j.contains("trafficSeedHi") && j["trafficSeedHi"].is_number()) {
        j.at("trafficSeedHi").get_to(e.trafficSeedHi);
    }
    if (j.contains("trafficSeedLo") && j["trafficSeedLo"].is_number()) {
        j.at("trafficSeedLo").get_to(e.trafficSeedLo);
    }
    if (j.contains("trafficSplit") && j["trafficSplit"].is_array()) {
        j.at("trafficSplit").get_to(e.trafficSplit);
    }
    if (j.contains("fullOnVariant") && j["fullOnVariant"].is_number()) {
        j.at("fullOnVariant").get_to(e.fullOnVariant);
    }
    if (j.contains("audience")) {
        e.audience = j.at("audience");
    }
    if (j.contains("variants") && j["variants"].is_array()) {
        j.at("variants").get_to(e.variants);
    }
    if (j.contains("audienceStrict") && j["audienceStrict"].is_boolean()) {
        j.at("audienceStrict").get_to(e.audienceStrict);
    }
    if (j.contains("customFieldValues") && j["customFieldValues"].is_array()) {
        j.at("customFieldValues").get_to(e.customFieldValues);
    }
    if (j.contains("application") && j["application"].is_string()) {
        j.at("application").get_to(e.application);
    } else if (j.contains("applications") && j["applications"].is_array() && !j["applications"].empty()) {
        auto& first = j["applications"][0];
        if (first.contains("name") && first["name"].is_string()) {
            e.application = first["name"].get<std::string>();
        }
    }
    if (j.contains("environment") && j["environment"].is_string()) {
        j.at("environment").get_to(e.environment);
    }
    if (j.contains("assignmentRules") && j["assignmentRules"].is_string()) {
        j.at("assignmentRules").get_to(e.assignmentRules);
    }
    // Coverage is replaced on every decode, so a model decoded again from a payload without
    // holdouts never keeps stale coverage (unlike the older optional fields).
    e.holdoutIds.clear();
    e.holdoutType.clear();
    if (j.contains("holdoutIds") && j["holdoutIds"].is_array()) {
        for (const auto& id : j["holdoutIds"]) {
            int value = 0;
            if (detail::get_int(id, value)) {
                e.holdoutIds.push_back(value);
            }
        }
    }
    if (j.contains("holdoutType") && j["holdoutType"].is_string()) {
        j.at("holdoutType").get_to(e.holdoutType);
    }
    detail::decode_switchback(j, e);
}

// Keeps switchbackClusters in its served order; otherwise identical to the nlohmann::json decode.
inline void from_json(const nlohmann::ordered_json& j, ExperimentData& e) {
    from_json(nlohmann::json(j), e);
    if (j.is_object()) {
        const auto it = j.find("switchbackClusters");
        if (it != j.end() && it->is_object()) {
            e.switchbackClusters = *it;
        }
    }
}

inline void to_json(nlohmann::ordered_json& j, const ExperimentData& e) {
    j = nlohmann::json(e);
    if (!e.switchbackClusters.is_null()) {
        j["switchbackClusters"] = e.switchbackClusters;
    }
}

// A holdout: a randomisation over the units of the experiments that list its id
// in holdoutIds. Arm 0 holds the unit out of every covered experiment; in a
// 3-arm holdout, arm 1 holds it out of covered experiments that are not full-on.
struct HoldoutData {
    int id = 0;
    std::string name;
    std::string unitType;
    int iteration = 0;
    int32_t seedHi = 0;
    int32_t seedLo = 0;
    std::vector<double> split;
    std::string holdoutType;
};

inline void to_json(nlohmann::json& j, const HoldoutData& h) {
    j = nlohmann::json{
        {"id", h.id},
        {"name", h.name},
        {"unitType", h.unitType},
        {"iteration", h.iteration},
        {"seedHi", h.seedHi},
        {"seedLo", h.seedLo},
        {"split", h.split}
    };
    if (!h.holdoutType.empty()) {
        j["holdoutType"] = h.holdoutType;
    }
}

// Only the id is required; other fields keep their defaults when absent, of
// another type or out of range, and unknown fields are ignored. A missing name
// publishes "" (the Collector always sends one).
inline void from_json(const nlohmann::json& j, HoldoutData& h) {
    if (!j.is_object() || !j.contains("id") || !detail::get_int(j["id"], h.id)) {
        throw nlohmann::json::type_error::create(302, "holdout without an integer id", &j);
    }
    if (j.contains("name") && j["name"].is_string()) {
        j.at("name").get_to(h.name);
    }
    if (j.contains("unitType") && j["unitType"].is_string()) {
        j.at("unitType").get_to(h.unitType);
    }
    if (j.contains("iteration")) {
        detail::get_int(j["iteration"], h.iteration);
    }
    if (j.contains("seedHi")) {
        detail::get_seed(j["seedHi"], h.seedHi);
    }
    if (j.contains("seedLo")) {
        detail::get_seed(j["seedLo"], h.seedLo);
    }
    if (j.contains("split") && j["split"].is_array()) {
        const auto& split = j["split"];
        if (std::all_of(split.begin(), split.end(), [](const nlohmann::json& v) { return v.is_number(); })) {
            split.get_to(h.split);
        }
    }
    if (j.contains("holdoutType") && j["holdoutType"].is_string()) {
        j.at("holdoutType").get_to(h.holdoutType);
    }
}

struct ContextData {
    std::vector<ExperimentData> experiments;
    std::vector<HoldoutData> holdouts;
};

inline void to_json(nlohmann::json& j, const ContextData& c) {
    j = nlohmann::json{{"experiments", c.experiments}};
    if (!c.holdouts.empty()) {
        j["holdouts"] = c.holdouts;
    }
}

inline void from_json(const nlohmann::json& j, ContextData& c) {
    j.at("experiments").get_to(c.experiments);
    c.holdouts.clear();
    if (j.contains("holdouts") && j["holdouts"].is_array()) {
        for (const auto& entry : j["holdouts"]) {
            if (!entry.is_object()) {
                continue;
            }
            try {
                c.holdouts.push_back(entry.get<HoldoutData>());
            } catch (const nlohmann::json::exception&) {
            }
        }
    }
}

// Decoding from nlohmann::ordered_json keeps each switchbackClusters map in its served order; a
// nlohmann::json decode sorts it by unit type name. Everything else decodes the same.
inline void from_json(const nlohmann::ordered_json& j, ContextData& c) {
    from_json(nlohmann::json(j), c);
    const auto& experiments = j.at("experiments");
    for (size_t i = 0; i < c.experiments.size() && i < experiments.size(); ++i) {
        const auto& experiment = experiments[i];
        const auto clusters = experiment.find("switchbackClusters");
        if (clusters != experiment.end() && clusters->is_object()) {
            c.experiments[i].switchbackClusters = *clusters;
        }
    }
}

inline void to_json(nlohmann::ordered_json& j, const ContextData& c) {
    j = nlohmann::json(c);
    auto& experiments = j["experiments"];
    for (size_t i = 0; i < c.experiments.size(); ++i) {
        if (!c.experiments[i].switchbackClusters.is_null()) {
            experiments[i]["switchbackClusters"] = c.experiments[i].switchbackClusters;
        }
    }
}

struct Exposure {
    int id = 0;
    std::string name;
    std::string unit;
    int variant = 0;
    int64_t exposedAt = 0;
    bool assigned = false;
    bool eligible = false;
    bool overridden = false;
    bool fullOn = false;
    bool custom = false;
    bool audienceMismatch = false;
    // Set when a matching assignment rule chose the variant, so the backend can
    // exclude the exposure from analysis.
    bool ruleOverride = false;
};

inline void to_json(nlohmann::json& j, const Exposure& e) {
    j = nlohmann::json{
        {"id", e.id},
        {"name", e.name},
        {"unit", e.unit},
        {"variant", e.variant},
        {"exposedAt", e.exposedAt},
        {"assigned", e.assigned},
        {"eligible", e.eligible},
        {"overridden", e.overridden},
        {"fullOn", e.fullOn},
        {"custom", e.custom},
        {"audienceMismatch", e.audienceMismatch},
        {"ruleOverride", e.ruleOverride}
    };
}

inline void from_json(const nlohmann::json& j, Exposure& e) {
    j.at("id").get_to(e.id);
    j.at("name").get_to(e.name);
    if (j.contains("unit")) j.at("unit").get_to(e.unit);
    j.at("variant").get_to(e.variant);
    j.at("exposedAt").get_to(e.exposedAt);
    j.at("assigned").get_to(e.assigned);
    j.at("eligible").get_to(e.eligible);
    if (j.contains("overridden")) j.at("overridden").get_to(e.overridden);
    if (j.contains("fullOn")) j.at("fullOn").get_to(e.fullOn);
    if (j.contains("custom")) j.at("custom").get_to(e.custom);
    if (j.contains("audienceMismatch")) j.at("audienceMismatch").get_to(e.audienceMismatch);
    if (j.contains("ruleOverride")) j.at("ruleOverride").get_to(e.ruleOverride);
}

struct GoalAchievement {
    std::string name;
    int64_t achievedAt = 0;
    nlohmann::json properties;
};

inline void to_json(nlohmann::json& j, const GoalAchievement& g) {
    j = nlohmann::json{
        {"name", g.name},
        {"achievedAt", g.achievedAt},
        {"properties", g.properties}
    };
}

inline void from_json(const nlohmann::json& j, GoalAchievement& g) {
    j.at("name").get_to(g.name);
    j.at("achievedAt").get_to(g.achievedAt);
    g.properties = j.value("properties", nlohmann::json());
}

struct Attribute {
    std::string name;
    nlohmann::json value;
    int64_t setAt = 0;
};

inline void to_json(nlohmann::json& j, const Attribute& a) {
    j = nlohmann::json{{"name", a.name}, {"value", a.value}, {"setAt", a.setAt}};
}

inline void from_json(const nlohmann::json& j, Attribute& a) {
    j.at("name").get_to(a.name);
    a.value = j.value("value", nlohmann::json());
    j.at("setAt").get_to(a.setAt);
}

struct Unit {
    std::string type;
    std::string uid;
};

inline void to_json(nlohmann::json& j, const Unit& u) {
    j = nlohmann::json{{"type", u.type}, {"uid", u.uid}};
}

inline void from_json(const nlohmann::json& j, Unit& u) {
    j.at("type").get_to(u.type);
    j.at("uid").get_to(u.uid);
}

struct PublishEvent {
    bool hashed = false;
    std::vector<Unit> units;
    int64_t publishedAt = 0;
    std::vector<Exposure> exposures;
    std::vector<GoalAchievement> goals;
    std::vector<Attribute> attributes;
};

inline void to_json(nlohmann::json& j, const PublishEvent& p) {
    j = nlohmann::json{
        {"hashed", p.hashed},
        {"units", p.units},
        {"publishedAt", p.publishedAt},
        {"exposures", p.exposures},
        {"goals", p.goals},
        {"attributes", p.attributes}
    };
}

inline void from_json(const nlohmann::json& j, PublishEvent& p) {
    j.at("hashed").get_to(p.hashed);
    j.at("units").get_to(p.units);
    j.at("publishedAt").get_to(p.publishedAt);
    if (j.contains("exposures")) j.at("exposures").get_to(p.exposures);
    if (j.contains("goals")) j.at("goals").get_to(p.goals);
    if (j.contains("attributes")) j.at("attributes").get_to(p.attributes);
}

} // namespace absmartly
