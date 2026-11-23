#include <catch2/catch_test_macros.hpp>
#include <absmartly/context.h>

#include <initializer_list>

using namespace absmartly;
using json = nlohmann::json;

// Several experiments can define the same variable key. The candidates are the experiments
// whose variants define it, by experiment id ascending; the winner is the first one the unit is
// in (assigned, overridden or rule-assigned) and the value comes from its variant only. Only
// the winner is exposed: a candidate that was checked but did not win was never seen by the
// user. Mirrors the Java SDK's Context.getVariableAssignment.

static const char* KEY = "banner.color";
static const char* LOW = "exp_low";
static const char* HIGH = "exp_high";

// A full-on experiment is assigned without hashing, so the winning arm is fixed by the fixture.
static ExperimentData make_exp(int id, const std::string& name, std::initializer_list<json> configs,
                               int full_on_variant) {
    ExperimentData exp;
    exp.id = id;
    exp.name = name;
    exp.unitType = "session_id";
    exp.iteration = 1;
    exp.seedHi = 3603515;
    exp.seedLo = 233373850;
    exp.trafficSeedHi = 449867249;
    exp.trafficSeedLo = 455443629;
    exp.trafficSplit = {0.0, 1.0};
    exp.fullOnVariant = full_on_variant;
    exp.audience = nullptr;
    int i = 0;
    for (const auto& config : configs) {
        ExperimentVariant variant;
        variant.name = std::string(1, static_cast<char>('A' + i++));
        variant.config = config.is_null() ? json(nullptr) : json(config.dump());
        exp.split.push_back(1.0 / static_cast<double>(configs.size()));
        exp.variants.push_back(variant);
    }
    return exp;
}

// The low-id candidate defines the key in variant 1 only; the high-id one in variant 1 only.
static ExperimentData low_exp(int full_on_variant = 1) {
    return make_exp(1, LOW, {nullptr, {{KEY, "low"}}}, full_on_variant);
}

static ExperimentData high_exp(int full_on_variant = 1) {
    return make_exp(2, HIGH, {nullptr, {{KEY, "high"}}}, full_on_variant);
}

// Not assigned: a strict audience the unit does not match.
static void strict_mismatch(ExperimentData& exp) {
    exp.audienceStrict = true;
    exp.audience = json(R"({"filter":[{"eq":[{"var":"country"},{"value":"US"}]}]})");
}

// Not assigned: the unit type has no unit, and the experiment is not full-on.
static void missing_unit(ExperimentData& exp) {
    exp.unitType = "device_id";
    exp.fullOnVariant = 0;
}

static ContextData data_of(std::initializer_list<ExperimentData> experiments) {
    ContextData data;
    // Reversed so candidate order must come from the id, not from the payload order.
    for (auto it = std::rbegin(experiments); it != std::rend(experiments); ++it) {
        data.experiments.push_back(*it);
    }
    return data;
}

static ContextConfig make_config() {
    ContextConfig config;
    config.publish_delay = -1;
    config.refresh_period = 0;
    config.units = {{"session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec"}};
    return config;
}

static std::vector<std::string> exposed_names(Context& ctx) {
    std::vector<std::string> names;
    for (const auto& exposure : ctx.publish().exposures) {
        names.push_back(exposure.name);
    }
    return names;
}

static json assign_rule_to(int variant) {
    return {{"rules", json::array({{{"name", "all"}, {"type", "assign"}, {"conditions", nullptr},
                                    {"environments", json::array()}, {"variant", variant}}})}};
}

TEST_CASE("Shared variable key: an unassigned lower-id candidate is skipped without exposure",
          "[context][shared-variable-key]") {
    auto low = low_exp();
    SECTION("strict audience mismatch") { strict_mismatch(low); }
    SECTION("no unit for its unit type") { missing_unit(low); }

    Context ctx(make_config(), data_of({low, high_exp()}));
    REQUIRE(ctx.variable_value(KEY, "default") == "high");
    REQUIRE(ctx.pending() == 1);
    REQUIRE(exposed_names(ctx) == std::vector<std::string>{HIGH});
}

TEST_CASE("Shared variable key: the winner's exposure is queued once", "[context][shared-variable-key]") {
    auto low = low_exp();
    strict_mismatch(low);
    Context ctx(make_config(), data_of({low, high_exp()}));

    REQUIRE(ctx.variable_value(KEY, "default") == "high");
    REQUIRE(ctx.variable_value(KEY, "default") == "high");

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].id == 2);
    REQUIRE(event.exposures[0].name == HIGH);
    REQUIRE(event.exposures[0].variant == 1);
    REQUIRE(event.exposures[0].assigned);
}

TEST_CASE("Shared variable key: the lowest-id assigned candidate wins", "[context][shared-variable-key]") {
    Context ctx(make_config(), data_of({low_exp(), high_exp()}));
    REQUIRE(ctx.variable_value(KEY, "default") == "low");
    REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
}

TEST_CASE("Shared variable key: a winner whose variant lacks the key gives the default",
          "[context][shared-variable-key]") {
    // The low-id experiment is assigned to variant 0, which does not define the key. Its value
    // is the caller default; a later candidate must not supply one, or the arm the user got
    // would decide which experiment they are counted in.
    auto low = low_exp();
    SECTION("traffic-assigned arm") {
        low.fullOnVariant = 0;
        low.split = {1.0, 0.0};
    }
    SECTION("full-on arm") {
        // The key is defined only by variant 2, so the experiment is still a candidate.
        low = make_exp(1, LOW, {nullptr, nullptr, {{KEY, "low"}}}, 1);
    }

    Context ctx(make_config(), data_of({low, high_exp()}));
    REQUIRE(ctx.peek_variable_value(KEY, "default") == "default");
    REQUIRE(ctx.pending() == 0);
    REQUIRE(ctx.variable_value(KEY, "default") == "default");
    REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
}

TEST_CASE("Shared variable key: no candidate assigned gives the default and no exposure",
          "[context][shared-variable-key]") {
    auto low = low_exp();
    auto high = high_exp();
    strict_mismatch(low);
    missing_unit(high);
    Context ctx(make_config(), data_of({low, high}));

    REQUIRE(ctx.variable_value(KEY, "default") == "default");
    REQUIRE(ctx.pending() == 0);
}

TEST_CASE("Shared variable key: peek exposes nothing", "[context][shared-variable-key]") {
    auto low = low_exp();
    strict_mismatch(low);
    Context ctx(make_config(), data_of({low, high_exp()}));

    REQUIRE(ctx.peek_variable_value(KEY, "default") == "high");
    REQUIRE(ctx.pending() == 0);
}

TEST_CASE("Shared variable key: overrides", "[context][shared-variable-key]") {
    SECTION("an override makes an unassigned candidate the winner") {
        auto low = low_exp();
        missing_unit(low);
        Context ctx(make_config(), data_of({low, high_exp()}));
        ctx.set_override(LOW, 1);
        REQUIRE(ctx.variable_value(KEY, "default") == "low");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
    }

    SECTION("an override to an arm without the key gives the default") {
        Context ctx(make_config(), data_of({low_exp(), high_exp()}));
        ctx.set_override(LOW, 0);
        REQUIRE(ctx.variable_value(KEY, "default") == "default");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
    }

    SECTION("an override to an unknown variant still wins and gives the default") {
        Context ctx(make_config(), data_of({low_exp(), high_exp()}));
        ctx.set_override(LOW, 11);
        REQUIRE(ctx.variable_value(KEY, "default") == "default");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
    }

    SECTION("an override on the higher-id candidate does not beat an assigned lower-id one") {
        Context ctx(make_config(), data_of({low_exp(), high_exp()}));
        ctx.set_override(HIGH, 1);
        REQUIRE(ctx.variable_value(KEY, "default") == "low");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
    }

    SECTION("an override on the higher-id candidate wins over an unassigned lower-id one") {
        auto low = low_exp();
        strict_mismatch(low);
        auto high = high_exp();
        missing_unit(high);
        Context ctx(make_config(), data_of({low, high}));
        ctx.set_override(HIGH, 1);
        REQUIRE(ctx.variable_value(KEY, "default") == "high");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{HIGH});
    }
}

TEST_CASE("Shared variable key: assignment rules", "[context][shared-variable-key]") {
    SECTION("a rule makes an otherwise unassigned candidate the winner") {
        auto low = low_exp();
        strict_mismatch(low);
        low.assignmentRules = assign_rule_to(1).dump();
        Context ctx(make_config(), data_of({low, high_exp()}));
        REQUIRE(ctx.variable_value(KEY, "default") == "low");
        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].name == LOW);
        REQUIRE(event.exposures[0].ruleOverride);
    }

    SECTION("a rule to an arm without the key gives the default") {
        auto low = low_exp();
        low.assignmentRules = assign_rule_to(0).dump();
        Context ctx(make_config(), data_of({low, high_exp()}));
        REQUIRE(ctx.variable_value(KEY, "default") == "default");
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{LOW});
    }

    SECTION("an override beats a rule on the same candidate") {
        auto low = low_exp();
        low.assignmentRules = assign_rule_to(1).dump();
        Context ctx(make_config(), data_of({low, high_exp()}));
        ctx.set_override(LOW, 0);
        REQUIRE(ctx.variable_value(KEY, "default") == "default");
        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].name == LOW);
        REQUIRE(event.exposures[0].overridden);
        REQUIRE_FALSE(event.exposures[0].ruleOverride);
    }
}

TEST_CASE("Shared variable key: falsy values are values, a missing key is not",
          "[context][shared-variable-key]") {
    for (const auto& value : {json(nullptr), json(false), json(0), json(""), json::array(), json::object()}) {
        auto low = low_exp();
        strict_mismatch(low);
        auto high = make_exp(2, HIGH, {nullptr, {{KEY, value}}}, 1);
        Context ctx(make_config(), data_of({low, high}));

        CAPTURE(value.dump());
        REQUIRE(ctx.peek_variable_value(KEY, "default") == value);
        REQUIRE(ctx.variable_value(KEY, "default") == value);
        REQUIRE(exposed_names(ctx) == std::vector<std::string>{HIGH});
    }
}
