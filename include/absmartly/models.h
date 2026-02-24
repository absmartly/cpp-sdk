#pragma once
#include <nlohmann/json.hpp>
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
    if (j.contains("config")) {
        v.config = j.at("config");
    }
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
    }
}

struct ExperimentData {
    int id = 0;
    std::string name;
    std::string unitType;
    int iteration = 0;
    int seedHi = 0;
    int seedLo = 0;
    std::vector<double> split;
    bool seedStrictMode = false;
    int trafficSeedHi = 0;
    int trafficSeedLo = 0;
    std::vector<double> trafficSplit;
    int fullOnVariant = 0;
    nlohmann::json audience;
    std::vector<ExperimentVariant> variants;
    bool audienceStrict = false;
    std::vector<CustomFieldValue> customFieldValues;
    std::string application;
    std::string environment;
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
}

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
}

struct ContextData {
    std::vector<ExperimentData> experiments;
};

inline void to_json(nlohmann::json& j, const ContextData& c) {
    j = nlohmann::json{{"experiments", c.experiments}};
}

inline void from_json(const nlohmann::json& j, ContextData& c) {
    j.at("experiments").get_to(c.experiments);
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
        {"audienceMismatch", e.audienceMismatch}
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
}

struct GoalAchievement {
    std::string name;
    int64_t achievedAt = 0;
    std::map<std::string, double> properties;
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
    if (j.contains("properties")) {
        for (auto& [key, val] : j.at("properties").items()) {
            if (val.is_number()) {
                g.properties[key] = val.get<double>();
            }
        }
    }
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
    if (j.contains("value")) a.value = j.at("value");
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
