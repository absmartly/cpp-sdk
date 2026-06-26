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
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <chrono>
#include <future>

namespace absmartly {

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
    bool exposed = false;
    std::vector<double> traffic_split;
    std::map<std::string, nlohmann::json> variables;
    bool has_variables = false;
    int attrs_seq = 0;
};

struct ExperimentIndex {
    const ExperimentData* data = nullptr;
    std::vector<std::map<std::string, nlohmann::json>> variables;
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

    void set_unit(const std::string& unit_type, const std::string& uid);
    void set_units(const std::map<std::string, std::string>& units);
    std::optional<std::string> get_unit(const std::string& unit_type) const;
    std::map<std::string, std::string> get_units() const;

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
    Assignment& get_or_create_assignment(const std::string& experiment_name);
    void queue_exposure(const std::string& experiment_name, const Assignment& assignment);

    void init(const ContextData& data);
    void build_index();

    bool experiment_matches(const ExperimentData& experiment, const Assignment& assignment) const;
    bool audience_matches(const ExperimentData& experiment, Assignment& assignment);
    nlohmann::json get_attributes_map() const;
    std::string unit_hash(const std::string& unit_type);

    void check_not_finalized() const;
    void check_ready() const;

    void emit_event(const std::string& type, const nlohmann::json& data = nlohmann::json());
    void setup_from_config();
    void become_ready(ContextData data);

    ContextConfig config_;
    ContextData data_;
    bool ready_ = false;
    bool failed_ = false;
    std::string failed_error_;
    bool finalized_ = false;
    bool finalizing_ = false;
    int pending_ = 0;

    std::map<std::string, std::string> units_;
    std::vector<Attribute> attrs_;
    int attrs_seq_ = 0;

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
};

} // namespace absmartly
