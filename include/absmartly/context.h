#pragma once

#include <absmartly/models.h>
#include <absmartly/context_config.h>
#include <absmartly/context_data_provider.h>
#include <absmartly/context_event_logger.h>
#include <absmartly/context_publisher.h>
#include <absmartly/audience_matcher.h>
#include <absmartly/variant_assigner.h>
#include <absmartly/hashing.h>
#include <nlohmann/json.hpp>
#include <map>
#include <utility>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <future>

namespace absmartly {

namespace internal {
struct SwitchbackIndex;
struct SwitchbackSlot;
struct SwitchbackState;
struct ContextTestAccess;
} // namespace internal

// A unit's arm in one holdout, cached per (holdout id, unit type) while the
// holdout's iteration is unchanged. The arm count is pinned with the arm: arm 1
// means something else in a 2-arm and a 3-arm holdout.
struct HoldoutAssignment {
    int id = 0;
    int iteration = 0;
    std::string unit_type;
    int variant = 0;
    size_t arm_count = 0;
    bool exposed = false;
};

// A holdout covering an experiment, as it was when the assignment was resolved.
// `assignment` is null when the experiment's unit was missing.
struct HoldoutPin {
    int id = 0;
    int iteration = 0;
    std::string name;
    std::shared_ptr<HoldoutAssignment> assignment;
};

struct Assignment {
    int id = 0;
    int iteration = 0;
    int full_on_variant = 0;
    std::string unit_type;
    int variant = 0;
    bool assigned = false;
    bool eligible = true;
    bool overridden = false;
    bool full_on = false;
    bool custom = false;
    bool audience_mismatch = false;
    // A matching assignment rule chose the variant. It wins over the audience,
    // traffic, full-on and custom assignments; only an override beats it.
    bool rule_override = false;
    // A split rule matched while the experiment's unit was missing, giving the
    // full-on variant (0 when not full-on). Setting the unit re-resolves it while
    // it is still unexposed.
    bool rule_missing_unit = false;
    // The assignmentRules string this assignment was resolved against; a change
    // (a refresh) re-resolves the rules. The environment is fixed per context.
    std::string rule_key;
    // The data load rule_key was last checked against, so cached reads skip the
    // string comparison until the next refresh.
    int rule_data_seq = 0;
    bool exposed = false;
    std::vector<double> traffic_split;
    std::map<std::string, nlohmann::json> variables;
    bool has_variables = false;
    int attrs_seq = 0;
    bool suppressed = false;
    // The holdouts covering the experiment when this was resolved, in id order;
    // null when there are none. Exposures fire from these, never re-resolved.
    std::shared_ptr<const std::vector<HoldoutPin>> holdouts;
    // A switchback slot: exposed with the clock reading of the call that exposes it.
    bool switchback = false;
};

struct ExperimentIndex {
    const ExperimentData* data = nullptr; // Non-owning; rebuilt whenever Context::data_ changes.
    std::vector<std::map<std::string, nlohmann::json>> variables;
    // data->assignmentRules parsed once; null when there are none or they are invalid.
    nlohmann::json assignment_rules;
    std::vector<const HoldoutData*> holdouts; // Sorted by id, unique; non-owning, like data.
    // Switchback metadata is computed once per data load, preserving cluster order.
    std::shared_ptr<internal::SwitchbackIndex> switchback;
};

struct RuleResolution {
    int variant = 0;
    bool missing_unit = false;
};

class Context {
public:
    Context(const ContextConfig& config, ContextData data,
            std::shared_ptr<ContextEventLogger> event_handler = nullptr,
            std::shared_ptr<ContextPublisher> event_publisher = nullptr,
            std::shared_ptr<ContextDataProvider> data_provider = nullptr);

    Context(const ContextConfig& config, std::future<ContextData> data_future,
            std::shared_ptr<ContextEventLogger> event_handler = nullptr,
            std::shared_ptr<ContextPublisher> event_publisher = nullptr,
            std::shared_ptr<ContextDataProvider> data_provider = nullptr);

    void wait_until_ready();

    bool is_ready();
    bool is_failed() const noexcept;
    bool is_finalized() const noexcept;
    bool is_finalizing() const noexcept;
    bool is_closed() const noexcept;
    bool is_closing() const noexcept;
    int pending() const noexcept;
    std::string ready_error() const noexcept;

    const ContextData& data() const;
    std::vector<std::string> experiments();

    void set_unit(const std::string& unit_type, const nlohmann::json& uid);
    void set_units(const std::map<std::string, nlohmann::json>& units);
    std::optional<nlohmann::json> get_unit(const std::string& unit_type) const;
    std::map<std::string, nlohmann::json> get_units() const;

    void set_attribute(const std::string& name, const nlohmann::json& value);
    void set_attributes(const std::map<std::string, nlohmann::json>& attrs);
    nlohmann::json get_attribute(const std::string& name) const;
    std::map<std::string, nlohmann::json> get_attributes() const;

    void set_override(const std::string& experiment_name, int variant);
    void set_overrides(const std::map<std::string, int>& overrides);

    void set_custom_assignment(const std::string& experiment_name, int variant);
    void set_custom_assignments(const std::map<std::string, int>& assignments);

    int treatment(const std::string& experiment_name);
    int peek(const std::string& experiment_name);

    nlohmann::json variable_value(const std::string& key, const nlohmann::json& default_value);
    nlohmann::json peek_variable_value(const std::string& key, const nlohmann::json& default_value);
    std::map<std::string, std::vector<std::string>> variable_keys();

    nlohmann::json custom_field_value(const std::string& experiment_name, const std::string& key);
    std::optional<std::string> custom_field_value_type(const std::string& experiment_name, const std::string& key);
    std::vector<std::string> custom_field_keys();

    void track(const std::string& goal_name, const nlohmann::json& properties = nlohmann::json());

    PublishEvent publish();

    PublishEvent finalize();
    PublishEvent close();

    void refresh();
    void refresh(const ContextData& new_data);

private:
    Assignment& get_or_create_assignment(const std::string& experiment_name, const ExperimentIndex* experiment);
    void queue_exposure(const std::string& experiment_name, const Assignment& assignment);
    void trigger_exposures(const std::string& experiment_name, const Assignment& assignment);
    nlohmann::json enqueue_exposure(const std::string& experiment_name, const Assignment& assignment);
    std::shared_ptr<HoldoutAssignment> get_holdout_assignment(const HoldoutData& holdout, const std::string& unit_type);
    static bool holdouts_match(const ExperimentIndex& experiment, const Assignment& assignment);

    void build_index();

    bool experiment_matches(const ExperimentData& experiment, const Assignment& assignment) const;
    bool audience_matches(const ExperimentData& experiment, Assignment& assignment);
    bool rule_matches(const ExperimentIndex& experiment, Assignment& assignment);
    std::optional<RuleResolution> resolve_rule(const ExperimentIndex& experiment, bool unit_pinned_missing);
    void invalidate_assignments_pinned_with_missing_unit(const std::string& unit_type);
    const nlohmann::json& get_attributes_map() const;
    std::string unit_hash(const std::string& unit_type);

    PublishEvent flush();

    void check_not_finalized() const;
    void check_ready(bool expect_not_finalized = false);

    void emit_event(const std::string& type, const nlohmann::json& data = nlohmann::json());
    void setup_from_config(const ContextConfig& config);
    void store_unit(const std::string& unit_type, const nlohmann::json& uid);
    void become_ready(ContextData data);
    void become_failed(const std::string& error);

    ContextData data_;
    bool ready_ = false;
    bool failed_ = false;
    std::string failed_error_;
    bool finalized_ = false;
    bool finalizing_ = false;
    int pending_ = 0;

    std::map<std::string, nlohmann::json> units_;
    std::vector<Attribute> attrs_;
    int attrs_seq_ = 0;
    // get_attributes_map() memoised per attrs_seq_, as in JS.
    mutable nlohmann::json attrs_map_cache_;
    mutable int attrs_map_cache_seq_ = -1;
    // Incremented by every data load (build_index), for Assignment::rule_data_seq.
    int data_seq_ = 0;
    // System attribute names from the client config; injected under the "application"
    // and "environment" keys when the audience evaluation map is built. Empty means
    // the key is omitted. User-set attributes override them.
    std::string application_;
    std::string environment_;

    std::map<std::string, Assignment> assignments_;

    std::map<std::string, int> overrides_;
    std::map<std::string, int> cassignments_;

    std::vector<Exposure> exposures_;
    std::vector<GoalAchievement> goals_;

    std::map<std::string, ExperimentIndex> index_;
    std::map<std::string, std::vector<const ExperimentIndex*>> index_variables_;

    std::map<std::string, std::string> hashes_;
    std::map<std::string, VariantAssigner> assigners_;

    std::shared_ptr<ContextEventLogger> event_handler_;
    std::shared_ptr<ContextPublisher> event_publisher_;
    std::shared_ptr<ContextDataProvider> data_provider_;

    std::future<ContextData> data_future_;

    AudienceMatcher audience_matcher_;

    // Non-owning pointers into data_.
    std::map<int, const HoldoutData*> holdouts_by_id_;
    // Kept across refreshes so a holdout exposure fires once per (id, unit type) and iteration.
    std::map<std::pair<int, std::string>, std::shared_ptr<HoldoutAssignment>> holdout_assignments_;

    // Switchback experiments (src/context_switchback.cpp). A call reads the clock at most once,
    // when it first meets a switchback, and resolves and exposes every switchback at that reading.
    Assignment& get_or_create_assignment(const std::string& experiment_name, std::optional<int64_t>& now);
    Assignment& switchback_assignment(const std::string& experiment_name, const ExperimentIndex& experiment,
                                      int64_t now);
    void resolve_switchback(internal::SwitchbackSlot& slot, const ExperimentIndex& experiment, int64_t period);
    void expose(const std::string& experiment_name, const Assignment& assignment, const std::optional<int64_t>& now);
    nlohmann::json enqueue_exposure(const std::string& experiment_name, const Assignment& assignment,
                                    int64_t exposed_at);
    std::optional<RuleResolution> resolve_rule(const ExperimentIndex& experiment, const std::string& unit_type,
                                               bool unit_pinned_missing);
    void index_switchbacks(const std::map<std::string, std::shared_ptr<internal::SwitchbackIndex>>& previous);
    void drop_switchback_epoch(const std::string& experiment_name);
    void invalidate_absent_switchbacks();
    // The SDK's own tests only (tests/switchback_support.h); not API.
    friend struct internal::ContextTestAccess;

    // Null until the first data load that has a switchback.
    std::shared_ptr<internal::SwitchbackState> switchbacks_;
    // treatment() and variable_value() calls in progress, re-entrant ones included.
    int call_depth_ = 0;
};

} // namespace absmartly
