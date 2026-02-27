#include "absmartly/context.h"
#include "absmartly/errors.h"

#include <algorithm>
#include <chrono>

namespace absmartly {

static int64_t now_millis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

static bool array_equals_shallow(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

Context::Context(const ContextConfig& config, ContextData data,
                 std::shared_ptr<ContextEventHandler> event_handler,
                 std::shared_ptr<ContextEventPublisher> event_publisher)
    : config_(config)
    , event_handler_(std::move(event_handler))
    , event_publisher_(std::move(event_publisher)) {
    setup_from_config();
    become_ready(std::move(data));
}

Context::Context(const ContextConfig& config, std::future<ContextData> data_future,
                 std::shared_ptr<ContextEventHandler> event_handler,
                 std::shared_ptr<ContextEventPublisher> event_publisher)
    : config_(config)
    , event_handler_(std::move(event_handler))
    , event_publisher_(std::move(event_publisher))
    , data_future_(std::move(data_future)) {
    setup_from_config();
}

void Context::setup_from_config() {
    for (const auto& [type, uid] : config_.units) {
        units_[type] = uid;
    }

    for (const auto& [name, variant] : config_.overrides) {
        overrides_[name] = variant;
    }

    for (const auto& [name, variant] : config_.custom_assignments) {
        cassignments_[name] = variant;
    }
}

void Context::become_ready(ContextData data) {
    data_ = std::move(data);
    init(data_);
    ready_ = true;

    nlohmann::json ready_data;
    nlohmann::json exps = nlohmann::json::array();
    for (const auto& exp : data_.experiments) {
        exps.push_back({{"id", exp.id}, {"name", exp.name}});
    }
    ready_data["experiments"] = exps;
    emit_event("ready", ready_data);
}

void Context::wait_until_ready() {
    if (ready_ || failed_) {
        return;
    }

    if (!data_future_.valid()) {
        failed_ = true;
        emit_event("error", {{"message", "No data future available"}});
        return;
    }

    try {
        auto data = data_future_.get();
        become_ready(std::move(data));
    } catch (const std::exception& e) {
        failed_ = true;
        emit_event("error", {{"message", e.what()}});
    }
}

bool Context::is_ready() const {
    return ready_;
}

bool Context::is_failed() const {
    return failed_;
}

bool Context::is_finalized() const {
    return finalized_;
}

bool Context::is_finalizing() const {
    return !finalized_ && finalizing_;
}

int Context::pending() const {
    return pending_;
}

const ContextData& Context::data() const {
    return data_;
}

std::vector<std::string> Context::experiments() const {
    std::vector<std::string> result;
    result.reserve(data_.experiments.size());
    for (const auto& exp : data_.experiments) {
        result.push_back(exp.name);
    }
    return result;
}

void Context::set_unit(const std::string& unit_type, const std::string& uid) {
    check_not_finalized();

    if (uid.empty()) {
        throw std::runtime_error("Unit '" + unit_type + "' UID must not be blank.");
    }

    auto it = units_.find(unit_type);
    if (it != units_.end() && it->second != uid) {
        throw std::runtime_error("Unit '" + unit_type + "' UID already set.");
    }

    units_[unit_type] = uid;
}

void Context::set_units(const std::map<std::string, std::string>& units) {
    for (const auto& [type, uid] : units) {
        set_unit(type, uid);
    }
}

std::optional<std::string> Context::get_unit(const std::string& unit_type) const {
    auto it = units_.find(unit_type);
    if (it != units_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::map<std::string, std::string> Context::get_units() const {
    return units_;
}

void Context::set_attribute(const std::string& name, const nlohmann::json& value) {
    check_not_finalized();

    Attribute attr;
    attr.name = name;
    attr.value = value;
    attr.setAt = now_millis();
    attrs_.push_back(std::move(attr));
    attrs_seq_++;
}

void Context::set_attributes(const std::map<std::string, nlohmann::json>& attrs) {
    for (const auto& [name, value] : attrs) {
        set_attribute(name, value);
    }
}

nlohmann::json Context::get_attribute(const std::string& name) const {
    nlohmann::json result = nullptr;
    for (const auto& attr : attrs_) {
        if (attr.name == name) {
            result = attr.value;
        }
    }
    return result;
}

std::map<std::string, nlohmann::json> Context::get_attributes() const {
    std::map<std::string, nlohmann::json> result;
    for (const auto& attr : attrs_) {
        result[attr.name] = attr.value;
    }
    return result;
}

void Context::set_override(const std::string& experiment_name, int variant) {
    overrides_[experiment_name] = variant;
}

void Context::set_overrides(const std::map<std::string, int>& overrides) {
    for (const auto& [name, variant] : overrides) {
        set_override(name, variant);
    }
}

void Context::set_custom_assignment(const std::string& experiment_name, int variant) {
    check_not_finalized();
    cassignments_[experiment_name] = variant;
}

void Context::set_custom_assignments(const std::map<std::string, int>& assignments) {
    for (const auto& [name, variant] : assignments) {
        set_custom_assignment(name, variant);
    }
}

int Context::treatment(const std::string& experiment_name) {
    check_ready();
    check_not_finalized();

    auto& assignment = get_or_create_assignment(experiment_name);

    if (!assignment.exposed) {
        assignment.exposed = true;
        queue_exposure(experiment_name, assignment);
    }

    return assignment.variant;
}

int Context::peek(const std::string& experiment_name) {
    check_ready();
    check_not_finalized();

    auto& assignment = get_or_create_assignment(experiment_name);
    return assignment.variant;
}

nlohmann::json Context::variable_value(const std::string& key, const nlohmann::json& default_value) {
    check_ready();
    check_not_finalized();

    auto var_it = index_variables_.find(key);
    if (var_it == index_variables_.end()) {
        return default_value;
    }

    for (const auto* experiment_index : var_it->second) {
        const std::string& experiment_name = experiment_index->data->name;
        auto& assignment = get_or_create_assignment(experiment_name);

        if (assignment.has_variables) {
            if (!assignment.exposed) {
                assignment.exposed = true;
                queue_exposure(experiment_name, assignment);
            }

            if ((assignment.assigned || assignment.overridden) &&
                assignment.variables.count(key) > 0) {
                return assignment.variables.at(key);
            }
        }
    }

    return default_value;
}

nlohmann::json Context::peek_variable_value(const std::string& key, const nlohmann::json& default_value) {
    check_ready();
    check_not_finalized();

    auto var_it = index_variables_.find(key);
    if (var_it == index_variables_.end()) {
        return default_value;
    }

    for (const auto* experiment_index : var_it->second) {
        const std::string& experiment_name = experiment_index->data->name;
        auto& assignment = get_or_create_assignment(experiment_name);

        if (assignment.has_variables) {
            if ((assignment.assigned || assignment.overridden) &&
                assignment.variables.count(key) > 0) {
                return assignment.variables.at(key);
            }
        }
    }

    return default_value;
}

std::map<std::string, std::vector<std::string>> Context::variable_keys() const {
    std::map<std::string, std::vector<std::string>> result;

    for (const auto& [key, experiments_for_var] : index_variables_) {
        for (const auto* exp_index : experiments_for_var) {
            result[key].push_back(exp_index->data->name);
        }
    }

    return result;
}

nlohmann::json Context::custom_field_value(const std::string& experiment_name, const std::string& key) const {
    auto it = index_.find(experiment_name);
    if (it == index_.end()) {
        return nullptr;
    }

    const auto* exp_data = it->second.data;
    for (const auto& field : exp_data->customFieldValues) {
        if (field.name == key) {
            if (!field.value.has_value()) {
                return nullptr;
            }
            const std::string& val = field.value.value();
            if (field.type == "text" || field.type == "string") {
                return val;
            }
            if (field.type == "number") {
                try {
                    return std::stod(val);
                } catch (...) {
                    return nullptr;
                }
            }
            if (field.type == "json") {
                try {
                    if (val == "null") return nullptr;
                    if (val.empty()) return "";
                    return nlohmann::json::parse(val);
                } catch (...) {
                    return nullptr;
                }
            }
            if (field.type == "boolean") {
                return val == "true";
            }
            return nullptr;
        }
    }

    return nullptr;
}

std::vector<std::string> Context::custom_field_keys() const {
    std::set<std::string> keys;
    for (const auto& exp : data_.experiments) {
        for (const auto& field : exp.customFieldValues) {
            keys.insert(field.name);
        }
    }
    return {keys.begin(), keys.end()};
}

void Context::track(const std::string& goal_name, const nlohmann::json& properties) {
    check_ready();
    check_not_finalized();

    GoalAchievement goal;
    goal.name = goal_name;
    goal.achievedAt = now_millis();

    if (properties.is_object()) {
        for (const auto& [key, val] : properties.items()) {
            if (val.is_number()) {
                goal.properties[key] = val.get<double>();
            }
        }
    }

    nlohmann::json goal_data;
    goal_data["name"] = goal.name;
    goal_data["achievedAt"] = goal.achievedAt;
    if (properties.is_null() || !properties.is_object()) {
        goal_data["properties"] = nullptr;
    } else {
        goal_data["properties"] = properties;
    }

    goals_.push_back(std::move(goal));
    pending_++;

    emit_event("goal", goal_data);
}

PublishEvent Context::publish() {
    if (pending_ == 0) {
        return {};
    }

    PublishEvent event;
    event.hashed = true;
    event.publishedAt = now_millis();

    for (const auto& [type, uid] : units_) {
        Unit u;
        u.type = type;
        u.uid = unit_hash(type);
        event.units.push_back(std::move(u));
    }

    if (!exposures_.empty()) {
        event.exposures = std::move(exposures_);
        exposures_.clear();
    }

    if (!goals_.empty()) {
        event.goals = std::move(goals_);
        goals_.clear();
    }

    if (!attrs_.empty()) {
        event.attributes = attrs_;
    }

    pending_ = 0;

    nlohmann::json pub_data = event;
    emit_event("publish", pub_data);

    return event;
}

PublishEvent Context::finalize() {
    if (finalized_) {
        return {};
    }

    finalizing_ = true;
    auto result = publish();
    finalized_ = true;
    finalizing_ = false;

    emit_event("finalize");

    return result;
}

void Context::refresh(const ContextData& new_data) {
    check_not_finalized();

    data_ = new_data;

    assignments_.clear();

    build_index();
    hashes_.clear();
    assigners_.clear();

    nlohmann::json refresh_data;
    nlohmann::json refresh_exps = nlohmann::json::array();
    for (const auto& exp : data_.experiments) {
        refresh_exps.push_back({{"id", exp.id}, {"name", exp.name}});
    }
    refresh_data["experiments"] = refresh_exps;
    emit_event("refresh", refresh_data);
}

Assignment& Context::get_or_create_assignment(const std::string& experiment_name) {
    bool has_custom = cassignments_.count(experiment_name) > 0;
    bool has_override = overrides_.count(experiment_name) > 0;
    auto exp_it = index_.find(experiment_name);
    const ExperimentIndex* experiment = (exp_it != index_.end()) ? &exp_it->second : nullptr;

    auto ass_it = assignments_.find(experiment_name);
    if (ass_it != assignments_.end()) {
        auto& existing = ass_it->second;
        if (has_override) {
            if (existing.overridden && existing.variant == overrides_[experiment_name]) {
                return existing;
            }
        } else if (experiment == nullptr) {
            if (!existing.assigned) {
                return existing;
            }
        } else if (!has_custom || cassignments_[experiment_name] == existing.variant) {
            if (experiment_matches(*experiment->data, existing) &&
                audience_matches(*experiment->data, existing)) {
                return existing;
            }
        }
    }

    Assignment assignment;
    assignment.id = 0;
    assignment.iteration = 0;
    assignment.full_on_variant = 0;
    assignment.variant = 0;
    assignment.overridden = false;
    assignment.assigned = false;
    assignment.exposed = false;
    assignment.eligible = true;
    assignment.full_on = false;
    assignment.custom = false;
    assignment.audience_mismatch = false;
    assignment.has_variables = false;

    if (has_override) {
        if (experiment != nullptr) {
            assignment.id = experiment->data->id;
            assignment.unit_type = experiment->data->unitType;
        }
        assignment.overridden = true;
        assignment.variant = overrides_[experiment_name];
    } else {
        if (experiment != nullptr) {
            const auto& exp_data = *experiment->data;
            const std::string& unit_type = exp_data.unitType;

            if (!exp_data.audience.is_null()) {
                std::string audience_str;
                if (exp_data.audience.is_string()) {
                    audience_str = exp_data.audience.get<std::string>();
                } else {
                    audience_str = exp_data.audience.dump();
                }

                if (!audience_str.empty()) {
                    auto result = audience_matcher_.evaluate(audience_str, get_attributes_map());
                    if (result.has_value()) {
                        assignment.audience_mismatch = !result.value();
                    }
                }
            }

            if (exp_data.audienceStrict && assignment.audience_mismatch) {
                assignment.variant = 0;
            } else if (exp_data.fullOnVariant == 0) {
                if (!unit_type.empty()) {
                    auto unit_it = units_.find(unit_type);
                    if (unit_it != units_.end()) {
                        std::string hashed = unit_hash(unit_type);
                        if (!hashed.empty()) {
                            auto assigner_it = assigners_.find(unit_type);
                            if (assigner_it == assigners_.end()) {
                                assigner_it = assigners_.emplace(unit_type, VariantAssigner(hashed)).first;
                            }
                            auto& assigner = assigner_it->second;

                            bool eligible = assigner.assign(
                                exp_data.trafficSplit,
                                exp_data.trafficSeedHi,
                                exp_data.trafficSeedLo) == 1;

                            assignment.assigned = true;
                            assignment.eligible = eligible;

                            if (eligible) {
                                if (has_custom) {
                                    assignment.variant = cassignments_[experiment_name];
                                    assignment.custom = true;
                                } else {
                                    assignment.variant = assigner.assign(
                                        exp_data.split, exp_data.seedHi, exp_data.seedLo);
                                }
                            } else {
                                assignment.variant = 0;
                            }
                        }
                    }
                }
            } else {
                assignment.assigned = true;
                assignment.eligible = true;
                assignment.variant = exp_data.fullOnVariant;
                assignment.full_on = true;
            }

            assignment.unit_type = unit_type;
            assignment.id = exp_data.id;
            assignment.iteration = exp_data.iteration;
            assignment.traffic_split = exp_data.trafficSplit;
            assignment.full_on_variant = exp_data.fullOnVariant;
            assignment.attrs_seq = attrs_seq_;
        }
    }

    if (experiment != nullptr &&
        assignment.variant >= 0 &&
        static_cast<size_t>(assignment.variant) < experiment->variables.size()) {
        assignment.variables = experiment->variables[assignment.variant];
        assignment.has_variables = true;
    }

    assignments_[experiment_name] = std::move(assignment);
    return assignments_[experiment_name];
}

void Context::queue_exposure(const std::string& experiment_name, const Assignment& assignment) {
    Exposure exposure;
    exposure.id = assignment.id;
    exposure.name = experiment_name;
    exposure.unit = assignment.unit_type;
    exposure.variant = assignment.variant;
    exposure.exposedAt = now_millis();
    exposure.assigned = assignment.assigned;
    exposure.eligible = assignment.eligible;
    exposure.overridden = assignment.overridden;
    exposure.fullOn = assignment.full_on;
    exposure.custom = assignment.custom;
    exposure.audienceMismatch = assignment.audience_mismatch;

    emit_event("exposure", nlohmann::json{
        {"id", exposure.id},
        {"name", exposure.name},
        {"unit", exposure.unit.empty() ? nlohmann::json(nullptr) : nlohmann::json(exposure.unit)},
        {"variant", exposure.variant},
        {"exposedAt", exposure.exposedAt},
        {"assigned", exposure.assigned},
        {"eligible", exposure.eligible},
        {"overridden", exposure.overridden},
        {"fullOn", exposure.fullOn},
        {"custom", exposure.custom},
        {"audienceMismatch", exposure.audienceMismatch}
    });

    exposures_.push_back(std::move(exposure));
    pending_++;
}

void Context::init(const ContextData& data) {
    data_ = data;
    build_index();
}

void Context::build_index() {
    index_.clear();
    index_variables_.clear();

    for (const auto& exp : data_.experiments) {
        ExperimentIndex entry;
        entry.data = &exp;

        for (size_t i = 0; i < exp.variants.size(); ++i) {
            const auto& variant = exp.variants[i];
            std::map<std::string, nlohmann::json> parsed;

            if (variant.config.is_string()) {
                const auto& config_str = variant.config.get_ref<const std::string&>();
                if (!config_str.empty()) {
                    try {
                        auto config_json = nlohmann::json::parse(config_str);
                        if (config_json.is_object()) {
                            for (auto it = config_json.begin(); it != config_json.end(); ++it) {
                                parsed[it.key()] = it.value();
                            }
                        }
                    } catch (...) {
                    }
                }
            } else if (variant.config.is_object()) {
                for (auto it = variant.config.begin(); it != variant.config.end(); ++it) {
                    parsed[it.key()] = it.value();
                }
            }

            entry.variables.push_back(std::move(parsed));
        }

        index_[exp.name] = std::move(entry);
    }

    for (auto& [exp_name, entry] : index_) {
        for (const auto& var_map : entry.variables) {
            for (const auto& [key, _] : var_map) {
                auto& vec = index_variables_[key];
                bool found = false;
                for (const auto* existing : vec) {
                    if (existing->data == entry.data) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    bool inserted = false;
                    for (auto it = vec.begin(); it != vec.end(); ++it) {
                        if (entry.data->id < (*it)->data->id) {
                            vec.insert(it, &entry);
                            inserted = true;
                            break;
                        }
                        if (entry.data->id == (*it)->data->id) {
                            inserted = true;
                            break;
                        }
                    }
                    if (!inserted) {
                        vec.push_back(&entry);
                    }
                }
            }
        }
    }
}

bool Context::experiment_matches(const ExperimentData& experiment, const Assignment& assignment) const {
    return experiment.id == assignment.id
        && experiment.unitType == assignment.unit_type
        && experiment.iteration == assignment.iteration
        && experiment.fullOnVariant == assignment.full_on_variant
        && array_equals_shallow(experiment.trafficSplit, assignment.traffic_split);
}

bool Context::audience_matches(const ExperimentData& experiment, Assignment& assignment) {
    std::string audience_str;
    if (experiment.audience.is_string()) {
        audience_str = experiment.audience.get<std::string>();
    } else if (!experiment.audience.is_null()) {
        audience_str = experiment.audience.dump();
    }

    if (!audience_str.empty()) {
        if (attrs_seq_ > assignment.attrs_seq) {
            auto result = audience_matcher_.evaluate(audience_str, get_attributes_map());
            bool new_audience_mismatch = result.has_value() ? !result.value() : false;

            if (new_audience_mismatch != assignment.audience_mismatch) {
                return false;
            }

            assignment.attrs_seq = attrs_seq_;
        }
    }
    return true;
}

nlohmann::json Context::get_attributes_map() const {
    nlohmann::json attrs = nlohmann::json::object();
    for (const auto& attr : attrs_) {
        attrs[attr.name] = attr.value;
    }
    return attrs;
}

std::string Context::unit_hash(const std::string& unit_type) {
    auto it = hashes_.find(unit_type);
    if (it != hashes_.end()) {
        return it->second;
    }

    auto unit_it = units_.find(unit_type);
    if (unit_it == units_.end()) {
        hashes_[unit_type] = "";
        return "";
    }

    std::string hashed = hash_unit(unit_it->second);
    hashes_[unit_type] = hashed;
    return hashed;
}

void Context::check_not_finalized() const {
    if (finalized_) {
        throw ContextFinalizedException();
    }
    if (finalizing_) {
        throw ContextFinalizedException();
    }
}

void Context::check_ready() const {
    if (!ready_) {
        throw ContextNotReadyException();
    }
}

void Context::emit_event(const std::string& type, const nlohmann::json& data) {
    if (event_handler_) {
        event_handler_->handle_event(*this, type, data);
    }
}

} // namespace absmartly
