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
#include <tuple>
#include <utility>

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

// Keep the one-element structured-binding shape published in 1.0.0. New fields remain
// available by name without changing existing `auto& [experiments] = data` consumers.
template <std::size_t I>
decltype(auto) get(ContextData& data) noexcept {
    static_assert(I == 0, "ContextData has one structured-binding element");
    return (data.experiments);
}

template <std::size_t I>
decltype(auto) get(const ContextData& data) noexcept {
    static_assert(I == 0, "ContextData has one structured-binding element");
    return (data.experiments);
}

template <std::size_t I>
decltype(auto) get(ContextData&& data) noexcept {
    static_assert(I == 0, "ContextData has one structured-binding element");
    return std::move(data.experiments);
}

template <std::size_t I>
decltype(auto) get(const ContextData&& data) noexcept {
    static_assert(I == 0, "ContextData has one structured-binding element");
    return std::move(data.experiments);
}

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

namespace std {

template <>
struct tuple_size<absmartly::ContextData> : integral_constant<size_t, 1> {};

template <>
struct tuple_element<0, absmartly::ContextData> {
    using type = vector<absmartly::ExperimentData>;
};

} // namespace std
