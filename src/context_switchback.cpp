// Switchback experiments in a Context: a port of java-sdk Context's switchback branch (R33,
// SWITCHBACK.md). A switchback's assignments live in SwitchbackState, one slot per period, never
// in assignments_.
#include "absmartly/context.h"
#include "clock.h"
#include "switchback_state.h"

#include <algorithm>

namespace absmartly {

using internal::NO_SWITCHBACK_PERIOD;
using internal::SwitchbackIndex;
using internal::SwitchbackServing;
using internal::SwitchbackSlot;
using internal::SwitchbackState;

namespace {

constexpr size_t RETAINED_PERIODS = 2;

bool is_switchback(const ExperimentData& experiment) {
    return experiment.randomizationType == "switchback";
}

// The unit an override with no serving unit is recorded under: the first linked type of a
// readable cluster map, otherwise the declared unitType the legacy path reads.
const std::string& fallback_unit_type(const SwitchbackIndex& switchback) {
    return switchback.legacy ? switchback.unit_type : switchback.source_types.front();
}

std::string audience_string(const nlohmann::json& audience) {
    if (audience.is_string()) return audience.get<std::string>();
    return audience.is_null() ? std::string() : audience.dump();
}

} // namespace

void Context::index_switchbacks(const std::map<std::string, std::shared_ptr<SwitchbackIndex>>& previous) {
    for (auto& [name, entry] : index_) {
        if (!is_switchback(*entry.data)) continue;
        if (!switchbacks_) switchbacks_ = std::make_shared<SwitchbackState>();
        auto index = std::make_shared<SwitchbackIndex>(internal::make_switchback_index(*entry.data));
        // An unchanged identity keeps its generation, and so its slots; anything else, including a
        // definition that was removed or a unit experiment in the previous load, starts a new one.
        const auto it = previous.find(name);
        index->generation = (it != previous.end() && index->same_identity(*it->second))
                                ? it->second->generation
                                : ++switchbacks_->generation_counter;
        entry.switchback = std::move(index);
    }
}

Assignment& Context::get_or_create_assignment(const std::string& experiment_name, std::optional<int64_t>& now) {
    if (switchbacks_) {
        const auto it = index_.find(experiment_name);
        if (it != index_.end() && it->second.switchback) {
            if (!now) now = internal::now_millis();
            return switchback_assignment(experiment_name, it->second, *now);
        }
        // No longer a switchback, or removed: its slots go now, lazily, as in java-sdk.
        if (!switchbacks_->epochs.empty()) drop_switchback_epoch(experiment_name);
    }
    return get_or_create_assignment(experiment_name);
}

void Context::drop_switchback_epoch(const std::string& experiment_name) {
    const auto epoch = switchbacks_->epochs.find(experiment_name);
    if (epoch == switchbacks_->epochs.end()) return;
    auto& slots = epoch->second.slots;
    while (!slots.empty()) {
        switchbacks_->drop(slots, slots.begin());
    }
    switchbacks_->epochs.erase(epoch);
}

void Context::invalidate_absent_switchbacks() {
    if (!switchbacks_) return;
    // A switchback resolved before any linked unit was set is resolved again now that one may be.
    // One without an override never published anything, so it goes even when exposed.
    for (auto& [name, epoch] : switchbacks_->epochs) {
        auto& slots = epoch.slots;
        for (auto it = slots.begin(); it != slots.end();) {
            const auto& slot = it->second;
            const auto next = std::next(it);
            if (slot.serving.status == SwitchbackServing::Status::Absent &&
                (!slot.assignment.overridden || !slot.assignment.exposed)) {
                switchbacks_->drop(slots, it);
            }
            it = next;
        }
    }
}

// The Collector's ContextAPI.switchbackServing at `now`. Sources are walked in declared order; the
// first linked unit whose path gives a cluster key serves, and a later different key is a
// disagreement. Only attributes set by the user at or before now are read, the latest setAt first.
static SwitchbackServing read_serving(const SwitchbackIndex& switchback, const std::map<std::string, nlohmann::json>& units,
                                      const std::vector<Attribute>& attrs, int64_t now) {
    SwitchbackServing serving;
    if (switchback.legacy) {
        // The declared unitType only identifies an ineligible control response.
        if (!switchback.unit_type.empty() && units.count(switchback.unit_type) > 0) {
            serving.status = SwitchbackServing::Status::Ineligible;
            serving.unit_type = switchback.unit_type;
        }
        return serving;
    }

    const std::string* first = nullptr;
    const std::string* serving_type = nullptr;
    std::optional<std::string> key;
    bool disagreement = false;
    for (size_t s = 0; s < switchback.source_types.size(); ++s) {
        const auto& type = switchback.source_types[s];
        if (units.count(type) == 0) continue;
        if (first == nullptr) first = &type;

        const auto& path = switchback.source_paths[s];
        const Attribute* latest = nullptr;
        for (const auto& attr : attrs) {
            if (attr.setAt <= now && internal::java_trim(attr.name) == path.root_key() &&
                (latest == nullptr || attr.setAt >= latest->setAt)) {
                latest = &attr;
            }
        }
        const nlohmann::json* value = latest ? path.resolve_attribute(latest->value) : nullptr;
        auto normalized = value ? internal::normalize_cluster_key(*value) : std::nullopt;
        if (!normalized) continue;
        if (!key) {
            key = std::move(normalized);
            serving_type = &type;
        } else if (*key != *normalized) {
            disagreement = true;
        }
    }

    if (first == nullptr) return serving;
    if (!key || disagreement || !switchback.allowed(*key)) {
        serving.status = SwitchbackServing::Status::Ineligible;
        serving.unit_type = *first;
        return serving;
    }
    serving.status = SwitchbackServing::Status::Eligible;
    serving.unit_type = *serving_type;
    serving.cluster_key = std::move(*key);
    return serving;
}

Assignment& Context::switchback_assignment(const std::string& experiment_name, const ExperimentIndex& experiment,
                                           int64_t now) {
    const auto& switchback = *experiment.switchback;
    const auto& data = *experiment.data;
    const int64_t period = switchback.period(now);

    // An assignment cached while this was a unit experiment is not reused if it becomes one again.
    const auto stale = assignments_.find(experiment_name);
    if (stale != assignments_.end()) {
        if (switchbacks_->depth > 0) {
            switchbacks_->retired_assignments.push_back(assignments_.extract(stale));
        } else {
            assignments_.erase(stale);
        }
    }

    auto& epoch = switchbacks_->epochs[experiment_name];
    if (epoch.generation != switchback.generation) {
        // A new epoch: every slot of the old definition goes, the one without a period included.
        while (!epoch.slots.empty()) {
            switchbacks_->drop(epoch.slots, epoch.slots.begin());
        }
        epoch.generation = switchback.generation;
    }
    auto& slots = epoch.slots;

    const auto custom_it = cassignments_.find(experiment_name);
    const std::optional<int> custom =
        custom_it != cassignments_.end() ? std::optional<int>(custom_it->second) : std::nullopt;
    const auto override_it = overrides_.find(experiment_name);

    auto cached = slots.find(period);
    if (cached != slots.end()) {
        auto& slot = cached->second;
        auto& existing = slot.assignment;
        if (override_it != overrides_.end()) {
            if (existing.overridden && existing.variant == override_it->second) {
                return existing;
            }
        } else if (!existing.overridden && (existing.rule_override || custom == slot.custom)) {
            // Rules are consulted only for an eligible cluster, so they cannot make another slot stale.
            const bool rules_current = slot.serving.status != SwitchbackServing::Status::Eligible ||
                                       rule_matches(experiment, existing);
            if (data.id == existing.id && data.iteration == existing.iteration &&
                data.fullOnVariant == existing.full_on_variant && data.trafficSplit == existing.traffic_split &&
                rules_current && audience_matches(data, existing)) {
                existing.attrs_seq = attrs_seq_;
                return existing;
            }
        }
    }

    SwitchbackSlot slot;
    slot.custom = custom;
    // Within a slot the serving unit and cluster key are read once: a slot rebuilt in the same
    // epoch keeps them, unless no linked unit was set then.
    if (cached != slots.end() && cached->second.serving.status != SwitchbackServing::Status::Absent) {
        slot.serving = cached->second.serving;
    } else {
        slot.serving = read_serving(switchback, units_, attrs_, now);
    }
    const auto& serving = slot.serving;
    auto& assignment = slot.assignment;
    assignment.switchback = true;
    assignment.id = data.id;
    assignment.iteration = data.iteration;
    assignment.traffic_split = data.trafficSplit;
    assignment.full_on_variant = data.fullOnVariant;
    assignment.attrs_seq = attrs_seq_;
    assignment.rule_key = data.assignmentRules;
    assignment.rule_data_seq = data_seq_;

    if (override_it != overrides_.end()) {
        assignment.unit_type = serving.unit_type.empty() ? fallback_unit_type(switchback) : serving.unit_type;
        assignment.overridden = true;
        assignment.variant = override_it->second;
    } else {
        resolve_switchback(slot, experiment, period);
    }

    if (assignment.variant >= 0 && static_cast<size_t>(assignment.variant) < experiment.variables.size()) {
        assignment.variables = experiment.variables[static_cast<size_t>(assignment.variant)];
        assignment.has_variables = true;
    }
    // Nothing is ever recorded for a silent slot.
    assignment.exposed = slot.silent;

    if (cached != slots.end()) {
        switchbacks_->drop(slots, cached);
    }
    auto& installed = slots.emplace(period, std::move(slot)).first->second;

    if (period != NO_SWITCHBACK_PERIOD) {
        // At most RETAINED_PERIODS periods are kept; the lowest index other than this one goes, so
        // the slot just installed always survives. Retention is by index, not recency. The slot
        // without a period sorts first (NO_SWITCHBACK_PERIOD is INT64_MIN) and is not counted.
        const auto periods = [&slots] { return slots.size() - slots.count(NO_SWITCHBACK_PERIOD); };
        while (periods() > RETAINED_PERIODS) {
            auto evicted = slots.upper_bound(NO_SWITCHBACK_PERIOD);
            if (evicted->first == period) ++evicted;
            switchbacks_->drop(slots, evicted);
        }
    }
    return installed.assignment;
}

// The Collector's assignExperiment order for a switchback: the serving resolution decides whether
// it applies and under which unit; for an eligible cluster, assignment rules (a split rule
// randomises over the serving unit), the audience, full-on, traffic (sticky on the serving unit's
// hash), then the custom assignment and the draw over the raw cluster key. Only the draw needs a
// period: before the anchor or with an unservable schedule the result is a silent control.
void Context::resolve_switchback(SwitchbackSlot& slot, const ExperimentIndex& experiment, int64_t period) {
    const auto& switchback = *experiment.switchback;
    const auto& data = *experiment.data;
    const auto& serving = slot.serving;
    auto& assignment = slot.assignment;
    assignment.variant = 0;

    if (serving.status == SwitchbackServing::Status::Absent) {
        // No linked unit: the experiment does not apply, like an unset unit type.
        assignment.unit_type = fallback_unit_type(switchback);
        assignment.eligible = false;
        slot.silent = true;
        return;
    }
    assignment.unit_type = serving.unit_type;
    assignment.assigned = true;
    if (serving.status == SwitchbackServing::Status::Ineligible) {
        assignment.eligible = false;
        slot.silent = true;
        return;
    }

    if (auto rule = resolve_rule(experiment, assignment.unit_type, false)) {
        // Recorded like a unit experiment's rule win: ruleOverride set, not assigned.
        assignment.assigned = false;
        assignment.rule_override = true;
        assignment.rule_missing_unit = rule->missing_unit;
        assignment.variant = rule->variant;
        return;
    }

    const std::string audience = audience_string(data.audience);
    if (!audience.empty()) {
        const auto result = audience_matcher_.evaluate(audience, get_attributes_map());
        if (result.has_value()) {
            assignment.audience_mismatch = !*result;
        }
    }
    if (data.audienceStrict && assignment.audience_mismatch) {
        // Recorded like a unit experiment's strict mismatch: control, not assigned.
        assignment.assigned = false;
        return;
    }
    if (data.fullOnVariant != 0) {
        assignment.variant = data.fullOnVariant;
        assignment.full_on = true;
        return;
    }
    if (period == NO_SWITCHBACK_PERIOD) {
        // Before the anchor or an unservable schedule: no draw, and no exposure (an SDK choice).
        assignment.eligible = false;
        slot.silent = true;
        return;
    }

    const std::string hashed = unit_hash(assignment.unit_type);
    auto assigner_it = assigners_.find(assignment.unit_type);
    if (assigner_it == assigners_.end()) {
        assigner_it = assigners_.emplace(assignment.unit_type, VariantAssigner(hashed)).first;
    }
    if (assigner_it->second.assign(data.trafficSplit, data.trafficSeedHi, data.trafficSeedLo) != 1) {
        // A traffic miss on an eligible cluster is recorded, as for a unit experiment.
        assignment.eligible = false;
        return;
    }
    if (slot.custom) {
        assignment.variant = *slot.custom;
        assignment.custom = true;
        return;
    }
    assignment.variant = assign_switchback(serving.cluster_key, period, switchback.seed, switchback.arm_count);
}

void Context::expose(const std::string& experiment_name, const Assignment& assignment,
                     const std::optional<int64_t>& now) {
    if (!assignment.switchback) {
        trigger_exposures(experiment_name, assignment);
        return;
    }
    // Stamped with the reading that chose the period: analysis derives the period from exposedAt.
    emit_event("exposure", enqueue_exposure(experiment_name, assignment, now ? *now : internal::now_millis()));
}

} // namespace absmartly
