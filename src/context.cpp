#include "absmartly/context.h"
#include "absmartly/errors.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iterator>

namespace absmartly {

static int64_t now_millis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// Arm 0 always holds the unit out. In a 3-arm holdout, arm 1 holds it out only of
// covered experiments that are not full-on. The arity is the split's length, never
// holdoutType (javascript-sdk isHeldOutBy).
static bool is_held_out_by(int holdout_variant, size_t holdout_arm_count, int full_on_variant) {
    if (holdout_variant == 0) return true;
    if (holdout_arm_count == 3 && holdout_variant == 1) return full_on_variant == 0;
    return false;
}

template <typename T>
static void prepend_all(std::vector<T>& queue, std::vector<T>& items) {
    queue.insert(queue.begin(),
                 std::make_move_iterator(items.begin()),
                 std::make_move_iterator(items.end()));
}

Context::Context(const ContextConfig& config, ContextData data,
                 std::shared_ptr<ContextEventLogger> event_handler,
                 std::shared_ptr<ContextPublisher> event_publisher,
                 std::shared_ptr<ContextDataProvider> data_provider)
    : event_handler_(std::move(event_handler))
    , event_publisher_(std::move(event_publisher))
    , data_provider_(std::move(data_provider)) {
    setup_from_config(config);
    become_ready(std::move(data));
}

Context::Context(const ContextConfig& config, std::future<ContextData> data_future,
                 std::shared_ptr<ContextEventLogger> event_handler,
                 std::shared_ptr<ContextPublisher> event_publisher,
                 std::shared_ptr<ContextDataProvider> data_provider)
    : event_handler_(std::move(event_handler))
    , event_publisher_(std::move(event_publisher))
    , data_provider_(std::move(data_provider))
    , data_future_(std::move(data_future)) {
    setup_from_config(config);
}

void Context::setup_from_config(const ContextConfig& config) {
    for (const auto& [type, uid] : config.units) {
        store_unit(type, uid);
    }
    overrides_ = config.overrides;
    cassignments_ = config.custom_assignments;
    application_ = config.application;
    environment_ = config.environment;
}

void Context::become_ready(ContextData data) {
    data_ = std::move(data);
    build_index();
    ready_ = true;

    nlohmann::json ready_data;
    nlohmann::json exps = nlohmann::json::array();
    for (const auto& exp : data_.experiments) {
        exps.push_back({{"id", exp.id}, {"name", exp.name}});
    }
    ready_data["experiments"] = exps;
    emit_event("ready", ready_data);
}

void Context::become_failed(const std::string& error) {
    // Like the JS SDK, a failed load leaves the context ready with empty data
    // so reads return control/default values.
    data_ = ContextData{};
    build_index();
    failed_ = true;
    ready_ = true;
    failed_error_ = error;
    emit_event("error", {{"message", error}});
}

void Context::wait_until_ready() {
    if (ready_ || failed_) {
        return;
    }

    if (!data_future_.valid()) {
        become_failed("No data future available");
        return;
    }

    try {
        auto data = data_future_.get();
        become_ready(std::move(data));
    } catch (const std::exception& e) {
        become_failed(e.what());
    }
}

bool Context::is_ready() {
    if (!ready_ && !failed_ && data_future_.valid()) {
        if (data_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            wait_until_ready();
        }
    }
    return ready_;
}

bool Context::is_failed() const noexcept {
    return failed_;
}

std::string Context::ready_error() const noexcept {
    return failed_error_;
}

bool Context::is_finalized() const noexcept {
    return finalized_;
}

bool Context::is_finalizing() const noexcept {
    return !finalized_ && finalizing_;
}

bool Context::is_closed() const noexcept {
    return is_finalized();
}

bool Context::is_closing() const noexcept {
    return is_finalizing();
}

int Context::pending() const noexcept {
    return pending_;
}

const ContextData& Context::data() const {
    return data_;
}

std::vector<std::string> Context::experiments() {
    check_ready();

    std::vector<std::string> result;
    result.reserve(data_.experiments.size());
    for (const auto& exp : data_.experiments) {
        result.push_back(exp.name);
    }
    return result;
}

void Context::set_unit(const std::string& unit_type, const nlohmann::json& uid) {
    check_not_finalized();
    store_unit(unit_type, uid);
    invalidate_assignments_pinned_with_missing_unit(unit_type);
}

void Context::invalidate_assignments_pinned_with_missing_unit(const std::string& unit_type) {
    // A split rule that matched without its unit gave the full-on variant, and a holdout
    // could not be resolved without it (a null pin). With the unit now set the assignment
    // is resolved again, but only while unexposed: rebuilding an exposed one would queue a
    // second, conflicting exposure.
    for (auto it = assignments_.begin(); it != assignments_.end();) {
        const auto& assignment = it->second;
        bool missing_holdout = false;
        if (assignment.holdouts) {
            for (const auto& pin : *assignment.holdouts) {
                missing_holdout = missing_holdout || !pin.assignment;
            }
        }
        if ((missing_holdout || (assignment.rule_override && assignment.rule_missing_unit)) &&
            !assignment.exposed && assignment.unit_type == unit_type) {
            it = assignments_.erase(it);
        } else {
            ++it;
        }
    }
}

void Context::store_unit(const std::string& unit_type, const nlohmann::json& uid) {
    nlohmann::json value = uid;
    if (value.is_string()) {
        static const char* ws = " \t\n\r\f\v";
        std::string s = value.get<std::string>();
        auto first = s.find_first_not_of(ws);
        s = first == std::string::npos ? std::string() : s.substr(first, s.find_last_not_of(ws) - first + 1);
        if (s.empty()) {
            throw std::runtime_error("Unit '" + unit_type + "' UID must not be blank.");
        }
        value = s;
    } else if (!value.is_number()) {
        throw std::runtime_error("Unit '" + unit_type + "' must be a string or a number.");
    }

    auto it = units_.find(unit_type);
    if (it != units_.end()) {
        // Numbers compare as JavaScript Numbers (doubles); the first value is kept.
        const auto& prev = it->second;
        bool same = prev.is_number() && value.is_number()
            ? prev.get<double>() == value.get<double>()
            : prev == value;
        if (!same) {
            throw std::runtime_error("Unit '" + unit_type + "' UID already set.");
        }
        return;
    }

    units_[unit_type] = value;
}

void Context::set_units(const std::map<std::string, nlohmann::json>& units) {
    for (const auto& [type, uid] : units) {
        set_unit(type, uid);
    }
}

std::optional<nlohmann::json> Context::get_unit(const std::string& unit_type) const {
    auto it = units_.find(unit_type);
    if (it != units_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::map<std::string, nlohmann::json> Context::get_units() const {
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
    cassignments_[experiment_name] = variant;
}

void Context::set_custom_assignments(const std::map<std::string, int>& assignments) {
    for (const auto& [name, variant] : assignments) {
        set_custom_assignment(name, variant);
    }
}

int Context::treatment(const std::string& experiment_name) {
    check_ready(true);

    auto& assignment = get_or_create_assignment(experiment_name);
    // Read before the exposure: its logger may refresh and re-resolve this assignment, and
    // JS returns the decision it exposed.
    const int variant = assignment.variant;

    if (!assignment.exposed) {
        assignment.exposed = true;
        trigger_exposures(experiment_name, assignment);
    }

    return variant;
}

int Context::peek(const std::string& experiment_name) {
    check_ready(true);

    auto& assignment = get_or_create_assignment(experiment_name);
    return assignment.variant;
}

nlohmann::json Context::variable_value(const std::string& key, const nlohmann::json& default_value) {
    check_ready(true);

    auto var_it = index_variables_.find(key);
    if (var_it == index_variables_.end()) {
        return default_value;
    }

    // A held-out unit reads the control value, not the caller's default, when nothing
    // else wins. The first suppressed candidate is kept even if it lacks the key, so a
    // later one cannot win (javascript-sdk _resolveVariableValue).
    bool has_suppressed_fallback = false;
    std::optional<nlohmann::json> suppressed_fallback;

    // An exposure runs the event logger, which may refresh the context. The data and index
    // a refresh replaces are kept until the outermost walk ends, so the walk goes on over the
    // candidate list it started with, resolved against the live data, as in JS.
    WalkGuard walk(*this);
    const auto& candidates = var_it->second;
    const int generation = data_seq_;

    for (const auto* candidate : candidates) {
        const std::string& name = candidate->data->name;
        // Reuse the indexed candidate only while it is live. An exposure logger may
        // refresh between candidates, in which case resolve the old name in the new index.
        auto& assignment = generation == data_seq_
            ? get_or_create_assignment(name, candidate)
            : get_or_create_assignment(name);
        if (!assignment.has_variables) {
            continue;
        }

        const auto value_it = assignment.variables.find(key);
        const bool wins = (assignment.assigned || assignment.overridden || assignment.rule_override) &&
                          value_it != assignment.variables.end();
        if (!wins && assignment.suppressed && !assignment.overridden && !assignment.rule_override &&
            !has_suppressed_fallback) {
            has_suppressed_fallback = true;
            if (value_it != assignment.variables.end()) {
                suppressed_fallback = value_it->second;
            }
        }

        if (!assignment.exposed) {
            // The decision is taken before the exposure: its logger may re-resolve this
            // assignment, and JS returns the value it exposed.
            std::optional<nlohmann::json> value;
            if (wins) {
                value = value_it->second;
            }
            walk.start();
            assignment.exposed = true;
            trigger_exposures(name, assignment);
            if (value.has_value()) {
                return std::move(*value);
            }
        } else if (wins) {
            return value_it->second;
        }
    }

    if (suppressed_fallback.has_value()) {
        return *suppressed_fallback;
    }

    return default_value;
}

nlohmann::json Context::peek_variable_value(const std::string& key, const nlohmann::json& default_value) {
    check_ready(true);

    auto var_it = index_variables_.find(key);
    if (var_it == index_variables_.end()) {
        return default_value;
    }

    const std::map<std::string, nlohmann::json>* suppressed_fallback = nullptr;

    for (const auto* experiment_index : var_it->second) {
        const std::string& experiment_name = experiment_index->data->name;
        auto& assignment = get_or_create_assignment(experiment_name, experiment_index);

        if (assignment.has_variables) {
            if (assignment.assigned || assignment.overridden || assignment.rule_override) {
                auto value_it = assignment.variables.find(key);
                if (value_it != assignment.variables.end()) {
                    return value_it->second;
                }
            }

            if (assignment.suppressed && !assignment.overridden && !assignment.rule_override &&
                suppressed_fallback == nullptr) {
                suppressed_fallback = &assignment.variables;
            }
        }
    }

    if (suppressed_fallback != nullptr) {
        auto it = suppressed_fallback->find(key);
        if (it != suppressed_fallback->end()) {
            return it->second;
        }
    }

    return default_value;
}

std::map<std::string, std::vector<std::string>> Context::variable_keys() {
    check_ready(true);

    std::map<std::string, std::vector<std::string>> result;

    for (const auto& [key, experiments_for_var] : index_variables_) {
        for (const auto* exp_index : experiments_for_var) {
            result[key].push_back(exp_index->data->name);
        }
    }

    return result;
}

nlohmann::json Context::custom_field_value(const std::string& experiment_name, const std::string& key) {
    check_ready(true);

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
                double result = 0.0;
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
                if (ec == std::errc{} && ptr == val.data() + val.size() && std::isfinite(result)) {
                    return result;
                }
                return nullptr;
            }
            if (field.type == "json") {
                try {
                    if (val.empty()) return "";
                    return nlohmann::json::parse(val);
                } catch (const nlohmann::json::parse_error&) {
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

std::optional<std::string> Context::custom_field_value_type(const std::string& experiment_name, const std::string& key) {
    check_ready(true);

    auto it = index_.find(experiment_name);
    if (it == index_.end()) {
        return std::nullopt;
    }

    const auto* exp_data = it->second.data;
    for (const auto& field : exp_data->customFieldValues) {
        if (field.name == key) {
            return field.type;
        }
    }

    return std::nullopt;
}

std::vector<std::string> Context::custom_field_keys() {
    check_ready(true);

    std::set<std::string> keys;
    for (const auto& exp : data_.experiments) {
        for (const auto& field : exp.customFieldValues) {
            keys.insert(field.name);
        }
    }
    return {keys.begin(), keys.end()};
}

void Context::track(const std::string& goal_name, const nlohmann::json& properties) {
    check_not_finalized();

    if (!properties.is_null() && !properties.is_object()) {
        throw std::runtime_error("Goal '" + goal_name + "' properties must be of type object.");
    }

    GoalAchievement goal;
    goal.name = goal_name;
    goal.achievedAt = now_millis();
    goal.properties = properties;

    nlohmann::json goal_data;
    goal_data["name"] = goal.name;
    goal_data["achievedAt"] = goal.achievedAt;
    goal_data["properties"] = properties;

    goals_.push_back(std::move(goal));
    pending_++;

    emit_event("goal", goal_data);
}

PublishEvent Context::publish() {
    check_ready(true);

    return flush();
}

PublishEvent Context::flush() {
    if (pending_ == 0) {
        return {};
    }

    if (failed_) {
        // Same as JS: a context that failed to load has nothing valid to
        // publish against, so its queued events are dropped and reported.
        const std::string message = "Discarding " + std::to_string(exposures_.size()) +
            " exposures and " + std::to_string(goals_.size()) +
            " goals because context failed to initialize";
        exposures_.clear();
        goals_.clear();
        pending_ = 0;
        emit_event("error", {{"message", message}});
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

    event.exposures = std::move(exposures_);
    exposures_.clear();

    event.goals = std::move(goals_);
    goals_.clear();

    if (!attrs_.empty()) {
        event.attributes = attrs_;
    }

    const int published = pending_;
    pending_ = 0;

    if (event_publisher_) {
        // A failed publish must not lose events: put them back at the front of
        // the queues (ahead of any event a re-entrant handler queued) and
        // restore the pending count, so a later publish() or finalize()
        // retries them. The error is then rethrown to the caller.
        auto requeue = [&]() {
            prepend_all(exposures_, event.exposures);
            prepend_all(goals_, event.goals);
            pending_ += published;
        };

        try {
            std::future<void> publish_future = event_publisher_->publish(event);
            if (publish_future.valid()) {
                publish_future.get();
            }
        } catch (const std::exception& e) {
            requeue();
            emit_event("error", {{"message", e.what()}});
            throw;
        } catch (...) {
            requeue();
            emit_event("error", {{"message", "Unknown publish error"}});
            throw;
        }
    }

    nlohmann::json pub_data = event;
    emit_event("publish", pub_data);

    return event;
}

PublishEvent Context::finalize() {
    // A finalize() re-entered from a publisher or event logger callback while
    // the outer one is still flushing must not seal the context ahead of the
    // outer flush's outcome; the outer call decides.
    if (finalized_ || finalizing_) {
        return {};
    }

    finalizing_ = true;
    PublishEvent result;
    try {
        result = flush();
    } catch (...) {
        finalizing_ = false;
        throw;
    }
    finalized_ = true;
    finalizing_ = false;

    emit_event("finalize");

    return result;
}

PublishEvent Context::close() {
    return finalize();
}

void Context::refresh() {
    check_ready(true);

    if (failed_) {
        return;
    }

    if (!data_provider_) {
        throw std::runtime_error("Cannot refresh: no data provider configured");
    }
    auto future = data_provider_->get_context_data();
    auto new_data = future.get();
    refresh(new_data);
}

void Context::end_walk() {
    walk_generations_.pop_back();
    retired_.remove_if([this](const RetiredData& retired) {
        return std::find(walk_generations_.begin(), walk_generations_.end(), retired.generation) ==
               walk_generations_.end();
    });
}

void Context::refresh(const ContextData& new_data) {
    check_ready(true);

    if (failed_) {
        return;
    }

    // Cached assignments are kept (sticky): get_or_create_assignment() decides
    // per experiment whether a cached one is still valid against the new data.
    // An override stays valid while its variant is unchanged, as in JS.
    if (std::find(walk_generations_.begin(), walk_generations_.end(), data_seq_) != walk_generations_.end()) {
        // A variable_value() walk iterates the current index; keep it until that walk ends.
        // Copied first: new_data may be data() itself. The node is allocated before anything
        // moves, so a failed allocation leaves the live index intact.
        ContextData installed = new_data;
        auto& retired = retired_.emplace_back();
        retired.generation = data_seq_;
        retired.data = std::move(data_);
        retired.index = std::move(index_);
        retired.index_variables = std::move(index_variables_);
        data_ = std::move(installed);
    } else {
        data_ = new_data;
    }
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

Assignment& Context::get_or_create_assignment(const std::string& experiment_name, const ExperimentIndex* experiment) {
    bool has_custom = cassignments_.count(experiment_name) > 0;
    bool has_override = overrides_.count(experiment_name) > 0;
    if (experiment == nullptr) {
        ++index_lookups_;
        auto exp_it = index_.find(experiment_name);
        experiment = (exp_it != index_.end()) ? &exp_it->second : nullptr;
    }

    auto ass_it = assignments_.find(experiment_name);
    if (ass_it != assignments_.end()) {
        auto& existing = ass_it->second;
        if (has_override) {
            if (existing.overridden && existing.variant == overrides_[experiment_name] &&
                (experiment == nullptr || holdouts_match(*experiment, existing))) {
                return existing;
            }
        } else if (experiment == nullptr) {
            // A rule-won assignment is also unassigned, but its experiment is gone.
            if (!existing.assigned && !existing.rule_override) {
                return existing;
            }
        } else if (existing.suppressed || existing.rule_override || !has_custom ||
                   cassignments_[experiment_name] == existing.variant) {
            // A suppressed assignment is forced to variant 0 and a matching rule wins over a
            // custom assignment, so a custom mismatch is not staleness for either; treating it
            // as stale rebuilt the assignment (and re-queued its exposure) on every call.
            if (experiment_matches(*experiment->data, existing) &&
                holdouts_match(*experiment, existing) &&
                rule_matches(*experiment, existing) &&
                audience_matches(*experiment->data, existing)) {
                // Both checks ran against the current attributes; advance the sequence
                // so later reads skip them until an attribute changes again.
                existing.attrs_seq = attrs_seq_;
                return existing;
            }
        }
    }

    Assignment assignment;

    // Resolved for overrides too: an override wins, but the holdouts still fire.
    if (experiment != nullptr && !experiment->holdouts.empty()) {
        const std::string& unit_type = experiment->data->unitType;
        auto pins = std::make_shared<std::vector<HoldoutPin>>();
        pins->reserve(experiment->holdouts.size());
        for (const auto* holdout : experiment->holdouts) {
            HoldoutPin pin;
            pin.id = holdout->id;
            pin.iteration = holdout->iteration;
            pin.name = holdout->name;
            // The covered experiment's unit type, never the holdout's own.
            if (!unit_type.empty()) {
                pin.assignment = get_holdout_assignment(*holdout, unit_type);
            }
            if (pin.assignment &&
                is_held_out_by(pin.assignment->variant, pin.assignment->arm_count,
                               experiment->data->fullOnVariant)) {
                assignment.suppressed = true;
            }
            pins->push_back(std::move(pin));
        }
        assignment.holdouts = std::move(pins);
    }

    if (has_override) {
        if (experiment != nullptr) {
            assignment.id = experiment->data->id;
            assignment.unit_type = experiment->data->unitType;
            assignment.iteration = experiment->data->iteration;
        }
        assignment.overridden = true;
        assignment.variant = overrides_[experiment_name];
    } else {
        if (experiment != nullptr) {
            const auto& exp_data = *experiment->data;
            const std::string& unit_type = exp_data.unitType;

            assignment.rule_key = exp_data.assignmentRules;
            assignment.rule_data_seq = data_seq_;
            auto rule = resolve_rule(*experiment, false);
            if (rule.has_value()) {
                // The rule's variant wins; the audience is not consulted, so
                // audienceMismatch stays false. assigned=false/eligible=true.
                assignment.rule_override = true;
                assignment.rule_missing_unit = rule->missing_unit;
                assignment.variant = rule->variant;
            } else if (assignment.suppressed) {
                // Held out: control, unassigned. Audience, full-on, traffic and custom
                // assignment are not consulted, so a holdout beats all of them.
                assignment.variant = 0;
            } else {
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

void Context::trigger_exposures(const std::string& experiment_name, const Assignment& assignment) {
    const bool own = !assignment.suppressed || assignment.overridden || assignment.rule_override;
    if (!assignment.holdouts) {
        if (own) {
            queue_exposure(experiment_name, assignment);
        }
        return;
    }

    // Every exposure of this call is queued before any event logger runs, so a logger that
    // finalizes (or refreshes) from the first callback still flushes the whole batch. JS
    // queues each exposure just before its callback, so there an exposure a logger queues
    // from that callback lands between the experiment's and its holdouts'; here it follows
    // them. The set published is the same.
    // Held by value: a re-entrant event logger may replace this assignment.
    const auto holdouts = assignment.holdouts;
    std::vector<nlohmann::json> events;
    events.reserve(holdouts->size() + 1);

    // A suppressed experiment publishes no exposure of its own; the holdouts record it.
    if (own) {
        events.push_back(enqueue_exposure(experiment_name, assignment));
    }

    // The pins from the decision, not a live re-resolution, so a refresh between the
    // decision and here cannot publish a holdout exposure from another iteration.
    for (const auto& pin : *holdouts) {
        if (pin.assignment && !pin.assignment->exposed) {
            pin.assignment->exposed = true;
            Assignment exposed;
            exposed.id = pin.assignment->id;
            exposed.unit_type = pin.assignment->unit_type;
            exposed.variant = pin.assignment->variant;
            exposed.assigned = true;
            exposed.eligible = true;
            events.push_back(enqueue_exposure(pin.name, exposed));
        }
    }

    for (const auto& event : events) {
        emit_event("exposure", event);
    }
}

std::shared_ptr<HoldoutAssignment> Context::get_holdout_assignment(const HoldoutData& holdout,
                                                                  const std::string& unit_type) {
    // Valid while the iteration is unchanged: a same-iteration seed or split edit neither
    // re-randomises nor re-exposes the unit.
    auto key = std::make_pair(holdout.id, unit_type);
    auto it = holdout_assignments_.find(key);
    if (it != holdout_assignments_.end() && it->second->iteration == holdout.iteration) {
        return it->second;
    }

    // Never cached when the unit is missing: it can still be set later.
    if (units_.count(unit_type) == 0) {
        return nullptr;
    }
    const std::string hashed = unit_hash(unit_type);
    if (hashed.empty()) {
        return nullptr;
    }

    auto assigner_it = assigners_.find(unit_type);
    if (assigner_it == assigners_.end()) {
        assigner_it = assigners_.emplace(unit_type, VariantAssigner(hashed)).first;
    }

    auto assignment = std::make_shared<HoldoutAssignment>();
    assignment->id = holdout.id;
    assignment->iteration = holdout.iteration;
    assignment->unit_type = unit_type;
    assignment->variant = assigner_it->second.assign_holdout(holdout.split, holdout.seedHi, holdout.seedLo);
    assignment->arm_count = holdout.split.size();
    holdout_assignments_[std::move(key)] = assignment;
    return assignment;
}

bool Context::holdouts_match(const ExperimentIndex& experiment, const Assignment& assignment) {
    // (id, iteration) only: a seed, split or name edit must not force a second exposure.
    const size_t pinned = assignment.holdouts ? assignment.holdouts->size() : 0;
    if (experiment.holdouts.size() != pinned) {
        return false;
    }
    for (size_t i = 0; i < pinned; ++i) {
        const auto& pin = (*assignment.holdouts)[i];
        if (experiment.holdouts[i]->id != pin.id || experiment.holdouts[i]->iteration != pin.iteration) {
            return false;
        }
    }
    return true;
}

void Context::queue_exposure(const std::string& experiment_name, const Assignment& assignment) {
    emit_event("exposure", enqueue_exposure(experiment_name, assignment));
}

nlohmann::json Context::enqueue_exposure(const std::string& experiment_name, const Assignment& assignment) {
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
    exposure.ruleOverride = assignment.rule_override;

    nlohmann::json exposure_data = exposure;
    if (exposure.unit.empty()) {
        exposure_data["unit"] = nullptr;
    }

    exposures_.push_back(std::move(exposure));
    pending_++;

    return exposure_data;
}

void Context::build_index() {
    ++data_seq_;
    index_.clear();
    index_variables_.clear();
    holdouts_by_id_.clear();
    // Reported once the index is complete: a logger may refresh from the error event.
    std::vector<nlohmann::json> errors;

    // A holdout without a split cannot be assigned, so it covers nothing.
    for (const auto& holdout : data_.holdouts) {
        if (!holdout.split.empty()) {
            holdouts_by_id_[holdout.id] = &holdout;
        }
    }

    for (const auto& exp : data_.experiments) {
        ExperimentIndex entry;
        entry.data = &exp;

        // Dangling ids are ignored and duplicates collapse, as in javascript-sdk.
        for (int holdout_id : exp.holdoutIds) {
            auto holdout_it = holdouts_by_id_.find(holdout_id);
            if (holdout_it == holdouts_by_id_.end()) {
                continue;
            }
            auto pos = std::lower_bound(entry.holdouts.begin(), entry.holdouts.end(), holdout_id,
                [](const HoldoutData* existing, int id) { return existing->id < id; });
            if (pos == entry.holdouts.end() || (*pos)->id != holdout_id) {
                entry.holdouts.insert(pos, holdout_it->second);
            }
        }

        // A value that is not a JSON object, like an object without a "rules" array,
        // means no rules for this experiment. No error event: JS and Java only log it,
        // and the context is healthy, so a logger must not see it as a failure.
        if (!exp.assignmentRules.empty()) {
            auto rules = nlohmann::json::parse(exp.assignmentRules, nullptr, false);
            if (rules.is_object()) {
                entry.assignment_rules = std::move(rules);
            }
        }

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
                    } catch (const nlohmann::json::exception& e) {
                        errors.push_back({{"message", "Failed to parse variant config for " + exp.name},
                                          {"error", e.what()}, {"variant", i}});
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
                auto pos = std::lower_bound(vec.begin(), vec.end(), entry.data->id,
                    [](const ExperimentIndex* existing, int id) { return existing->data->id < id; });
                if (pos == vec.end() || (*pos)->data->id != entry.data->id) {
                    vec.insert(pos, &entry);
                }
            }
        }
    }

    for (const auto& error : errors) {
        emit_event("error", error);
    }
}

bool Context::experiment_matches(const ExperimentData& experiment, const Assignment& assignment) const {
    return experiment.id == assignment.id
        && experiment.unitType == assignment.unit_type
        && experiment.iteration == assignment.iteration
        && experiment.fullOnVariant == assignment.full_on_variant
        && experiment.trafficSplit == assignment.traffic_split;
}

std::optional<RuleResolution> Context::resolve_rule(const ExperimentIndex& experiment, bool unit_pinned_missing) {
    if (experiment.assignment_rules.is_null()) {
        return std::nullopt;
    }

    const auto& data = *experiment.data;
    auto action = audience_matcher_.evaluate_rules(experiment.assignment_rules, environment_, get_attributes_map());
    if (!action.has_value()) {
        return std::nullopt;
    }

    // Like an out-of-range assign variant, a split without one percentage per variant
    // stops rule evaluation (first match wins) and normal assignment applies.
    const size_t variant_count = data.variants.size();
    if (!action->is_split) {
        if (action->variant < 0 || static_cast<size_t>(action->variant) >= variant_count) {
            return std::nullopt;
        }
        return RuleResolution{action->variant, false};
    }

    if (action->split.size() != variant_count) {
        return std::nullopt;
    }

    // Without the unit there is nothing to randomise over: the experiment's own no-unit
    // result applies (the full-on variant, or 0 when it is not full-on).
    const std::string& unit_type = data.unitType;
    std::string hashed;
    if (!unit_type.empty() && !unit_pinned_missing && units_.count(unit_type) > 0) {
        hashed = unit_hash(unit_type);
    }
    if (hashed.empty()) {
        return RuleResolution{data.fullOnVariant, true};
    }

    auto assigner_it = assigners_.find(unit_type);
    if (assigner_it == assigners_.end()) {
        assigner_it = assigners_.emplace(unit_type, VariantAssigner(hashed)).first;
    }
    // The experiment's own seed: a rule whose split equals the experiment's keeps every
    // unit on the variant it would have had anyway.
    return RuleResolution{assigner_it->second.assign(action->split, data.seedHi, data.seedLo), false};
}

bool Context::rule_matches(const ExperimentIndex& experiment, Assignment& assignment) {
    // The rules string is compared once per data load, not on every cached read.
    bool rules_changed = false;
    if (assignment.rule_data_seq != data_seq_) {
        if (experiment.data->assignmentRules == assignment.rule_key) {
            assignment.rule_data_seq = data_seq_;
        } else {
            rules_changed = true;
        }
    }
    if (!rules_changed && attrs_seq_ <= assignment.attrs_seq) {
        return true;
    }

    // An exposed split resolved without its unit keeps resolving as if the unit were
    // still missing, so a unit set afterwards cannot switch the variant the user saw.
    const bool unit_pinned_missing = assignment.exposed && assignment.rule_missing_unit;
    auto rule = resolve_rule(experiment, unit_pinned_missing);

    if (rule.has_value() != assignment.rule_override ||
        (rule.has_value() && rule->variant != assignment.variant)) {
        return false;
    }

    // Same outcome. Refresh the bookkeeping so an unexposed split now resolved without
    // its unit (or no longer so) is re-resolved, or kept, correctly on set_unit().
    // The caller advances attrs_seq once the audience check has also run.
    if (rules_changed) {
        assignment.rule_key = experiment.data->assignmentRules;
        assignment.rule_data_seq = data_seq_;
    }
    if (rule.has_value()) {
        assignment.rule_missing_unit = rule->missing_unit;
    }
    return true;
}

bool Context::audience_matches(const ExperimentData& experiment, Assignment& assignment) {
    // A rule-won or held-out assignment never consulted the audience, so an audience change
    // cannot make it stale.
    if (assignment.rule_override || assignment.suppressed) {
        return true;
    }

    std::string audience_str;
    if (experiment.audience.is_string()) {
        audience_str = experiment.audience.get<std::string>();
    } else if (!experiment.audience.is_null()) {
        audience_str = experiment.audience.dump();
    }

    if (!audience_str.empty()) {
        if (attrs_seq_ > assignment.attrs_seq) {
            auto result = audience_matcher_.evaluate(audience_str, get_attributes_map());
            bool new_audience_mismatch = result.has_value() ? !result.value() : assignment.audience_mismatch;

            if (new_audience_mismatch != assignment.audience_mismatch) {
                return false;
            }

            assignment.attrs_seq = attrs_seq_;
        }
    }
    return true;
}

const nlohmann::json& Context::get_attributes_map() const {
    if (attrs_map_cache_seq_ == attrs_seq_) {
        return attrs_map_cache_;
    }
    nlohmann::json attrs = nlohmann::json::object();
    // System attributes (application/environment from the client config) go in
    // first; user-set attributes overlay them, so a value the user sets under the
    // same name always wins, even one set after an assignment was cached (the
    // attrs_seq bump invalidates the cached audience result). An empty name is
    // omitted rather than set to null/"", so the key is simply absent and the
    // filter decides the result (an eq equality filter, for example, does not
    // match an absent key). app_version is intentionally not injected.
    if (!application_.empty()) {
        attrs["application"] = application_;
    }
    if (!environment_.empty()) {
        attrs["environment"] = environment_;
    }
    for (const auto& attr : attrs_) {
        attrs[attr.name] = attr.value;
    }
    attrs_map_cache_ = std::move(attrs);
    attrs_map_cache_seq_ = attrs_seq_;
    return attrs_map_cache_;
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
        throw ContextFinalizingException();
    }
}

void Context::check_ready(bool expect_not_finalized) {
    // A failed load leaves the context usable with empty data, so only a
    // pending load is "not ready". is_ready() picks up a completed load future.
    if (!is_ready() && !failed_) {
        throw ContextNotReadyException();
    }

    if (expect_not_finalized) {
        check_not_finalized();
    }
}

void Context::emit_event(const std::string& type, const nlohmann::json& data) {
    if (event_handler_) {
        // Logging is a side effect: an exception from a user logger must not
        // fail initialization, turn a delivered publish into a failure, or
        // abort the operation that emitted the event.
        try {
            event_handler_->handle_event(*this, type, data);
        } catch (...) {
        }
    }
}

} // namespace absmartly
