#include <catch2/catch_test_macros.hpp>
#include <absmartly/context.h>
#include <absmartly/context_event_logger.h>
#include <absmartly/hashing.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <new>
#include <sstream>

// Counts heap allocations so tests can pin the cost of the holdout paths.
static std::atomic<long> g_allocations{0};

void* operator new(std::size_t size) {
    ++g_allocations;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace absmartly;
using json = nlohmann::json;

// Ported from java-sdk ContextHoldoutTest (core-api/src/test/java/com/absmartly/sdk/
// ContextHoldoutTest.java, blob 88f716cd) and checked against javascript-sdk src/context.ts (the
// reference), with the same units, seeds and splits, so every SDK resolves the same holdout arms.
//
// A holdout arrives in ContextData::holdouts; a covered experiment lists its id in holdoutIds.
// Arm 0 holds the unit out; in a 3-arm holdout (split of length 3) arm 1 holds it out of covered
// experiments that are not full-on. A held-out unit gets control and no exposure of its own; the
// holdout publishes one ordinary exposure per (holdout, unit type) and iteration.

namespace {

const char* UNIT_TYPE = "session_id";
const char* UID = "e791e240fcd3df7d238cfc285f475e8152fcc0ec";
const char* UID_NOT_HELD_OUT = "b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3";

// split[0.5,0.5], seedHi=100, seedLo=200 -> variant 1 for UID, variant 0 for UID_NOT_HELD_OUT.
const int NORMAL_SEED_HI = 100;
const int NORMAL_SEED_LO = 200;
const int NORMAL_VARIANT = 1;

// split[0.1,0.9]: 13/111 holds UID out (arm 0) and not UID_NOT_HELD_OUT (arm 1).
const int HOLDOUT_A_SEED_HI = 13;
const int HOLDOUT_A_SEED_LO = 111;
// split[0.1,0.9]: 1/222 holds neither out (arm 1).
const int HOLDOUT_B_SEED_HI = 1;
const int HOLDOUT_B_SEED_LO = 222;

// For UID on split[0.3,0.3,0.4]: seedLo=1 -> arm 0, seedLo=3 -> arm 1, seedLo=0 -> arm 2.
const std::vector<double> THREE_ARM_SPLIT = {0.3, 0.3, 0.4};
const int THREE_ARM_0_SEED_LO = 1;
const int THREE_ARM_1_SEED_LO = 3;
const int THREE_ARM_2_SEED_LO = 0;

ExperimentData new_experiment(int id, const std::string& name, const std::string& unit_type = UNIT_TYPE,
                              int full_on_variant = 0) {
    ExperimentData experiment;
    experiment.id = id;
    experiment.name = name;
    experiment.unitType = unit_type;
    experiment.iteration = 1;
    experiment.seedHi = NORMAL_SEED_HI;
    experiment.seedLo = NORMAL_SEED_LO;
    experiment.split = {0.5, 0.5};
    experiment.trafficSeedHi = 1;
    experiment.trafficSeedLo = 2;
    experiment.trafficSplit = {0.0, 1.0};
    experiment.fullOnVariant = full_on_variant;
    experiment.variants = {{"A", nullptr}, {"B", nullptr}};
    experiment.application = "website";
    return experiment;
}

ExperimentData new_experiment(int id, const std::string& name, int full_on_variant) {
    return new_experiment(id, name, UNIT_TYPE, full_on_variant);
}

HoldoutData new_holdout(int id, const std::string& name, int seed_hi, int seed_lo,
                        const std::string& unit_type = UNIT_TYPE, const std::string& holdout_type = "full") {
    HoldoutData holdout;
    holdout.id = id;
    holdout.name = name;
    holdout.unitType = unit_type;
    holdout.iteration = 1;
    holdout.seedHi = seed_hi;
    holdout.seedLo = seed_lo;
    holdout.split = {0.1, 0.9};
    holdout.holdoutType = holdout_type;
    return holdout;
}

// holdoutType is set for wire fidelity only; the arity comes from split.
HoldoutData new_three_arm_holdout(int id, const std::string& name, int seed_lo) {
    HoldoutData holdout = new_holdout(id, name, 0, seed_lo, UNIT_TYPE, "all_full_on");
    holdout.split = THREE_ARM_SPLIT;
    return holdout;
}

ExperimentData covered_by(ExperimentData experiment, std::vector<int> holdout_ids) {
    experiment.holdoutIds = std::move(holdout_ids);
    return experiment;
}

ExperimentData with_variables(ExperimentData experiment, const std::string& key) {
    experiment.variants = {{"A", json({{key, "control"}}).dump()}, {"B", json({{key, "treatment"}}).dump()}};
    return experiment;
}

ContextData data_of(std::vector<HoldoutData> holdouts, std::vector<ExperimentData> experiments) {
    ContextData data;
    data.experiments = std::move(experiments);
    data.holdouts = std::move(holdouts);
    return data;
}

ContextConfig config_with(const std::string& uid, const std::string& unit_type = UNIT_TYPE) {
    ContextConfig config;
    config.units[unit_type] = uid;
    return config;
}

// The exposure fields that reach the wire, apart from exposedAt.
struct Expected {
    int id;
    std::string name;
    std::string unit;
    int variant;
    bool assigned = true;
    bool eligible = true;
    bool overridden = false;
    bool full_on = false;
    bool custom = false;
    bool audience_mismatch = false;
    bool rule_override = false;
};

Expected holdout_exposure(int id, const std::string& name, int variant, const std::string& unit = UNIT_TYPE) {
    return {id, name, unit, variant};
}

Expected own_exposure(int id, const std::string& name, int variant, const std::string& unit = UNIT_TYPE) {
    return {id, name, unit, variant};
}

json to_json_expected(const Expected& e) {
    return {{"id", e.id}, {"name", e.name}, {"unit", e.unit}, {"variant", e.variant},
            {"assigned", e.assigned}, {"eligible", e.eligible}, {"overridden", e.overridden},
            {"fullOn", e.full_on}, {"custom", e.custom}, {"audienceMismatch", e.audience_mismatch},
            {"ruleOverride", e.rule_override}};
}

json to_json_actual(const Exposure& e) {
    return {{"id", e.id}, {"name", e.name}, {"unit", e.unit}, {"variant", e.variant},
            {"assigned", e.assigned}, {"eligible", e.eligible}, {"overridden", e.overridden},
            {"fullOn", e.fullOn}, {"custom", e.custom}, {"audienceMismatch", e.audienceMismatch},
            {"ruleOverride", e.ruleOverride}};
}

// Publishes and compares every exposure, in order, plus the hashed units.
void require_published(Context& context, const std::vector<Expected>& expected,
                       const std::vector<std::pair<std::string, std::string>>& units = {{UNIT_TYPE, UID}}) {
    const PublishEvent event = context.publish();
    json actual = json::array();
    for (const auto& exposure : event.exposures) {
        actual.push_back(to_json_actual(exposure));
    }
    json wanted = json::array();
    for (const auto& exposure : expected) {
        wanted.push_back(to_json_expected(exposure));
    }
    INFO("published " << actual.dump());
    REQUIRE(actual == wanted);
    REQUIRE(event.hashed);
    REQUIRE(event.units.size() == units.size());
    for (size_t i = 0; i < units.size(); ++i) {
        REQUIRE(event.units[i].type == units[i].first);
        REQUIRE(event.units[i].uid == hash_unit(units[i].second));
    }
}

struct ExposureLog : ContextEventLogger {
    std::vector<json> exposures;
    std::function<void(Context&, const json&)> on_exposure;
    void handle_event(Context& context, const std::string& type, const json& data) override {
        if (type != "exposure") return;
        exposures.push_back(data);
        if (on_exposure) on_exposure(context, data);
    }
};

struct ThrowingLogger : ContextEventLogger {
    std::function<bool(const json&)> throws_on;
    void handle_event(Context&, const std::string& type, const json& data) override {
        if (type == "exposure" && throws_on(data)) throw std::runtime_error("boom");
    }
};

const char* US_RULE = R"({"rules":[{"name":"US Internal Users","type":"assign",)"
                      R"("conditions":{"and":[{"eq":[{"var":"country"},{"value":"US"}]}]},)"
                      R"("environments":[],"variant":1}]})";

const char* AGE_AUDIENCE = R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})";

} // namespace

// --- Two-arm holdouts ---------------------------------------------------------------------------

TEST_CASE("Holdouts: a held-out unit gets control and only the holdout exposure", "[holdouts]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_in"), {11})}));

    REQUIRE(context.peek("exp_holdout_in") == 0);
    context.treatment("exp_holdout_in");
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

TEST_CASE("Holdouts: a not-held-out unit publishes its own exposure and the holdout's arm 1", "[holdouts]") {
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_out"), {11})}));

    REQUIRE(context.treatment("exp_holdout_out") == 0);
    require_published(context, {own_exposure(1, "exp_holdout_out", 0), holdout_exposure(11, "holdout_a", 1)},
                      {{UNIT_TYPE, UID_NOT_HELD_OUT}});
}

TEST_CASE("Holdouts: an experiment without holdoutIds is not covered", "[holdouts]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_in"), {11}), new_experiment(2, "exp_not_covered")}));

    REQUIRE(context.treatment("exp_holdout_in") == 0);
    REQUIRE(context.treatment("exp_not_covered") == NORMAL_VARIANT);
    require_published(context, {holdout_exposure(11, "holdout_a", 0),
                                own_exposure(2, "exp_not_covered", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: the union of two holdouts suppresses, and both publish", "[holdouts][union]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_multi_holdout"), {11, 12})}));

    REQUIRE(context.treatment("exp_multi_holdout") == 0);
    require_published(context, {holdout_exposure(11, "holdout_a", 0), holdout_exposure(12, "holdout_b", 1)});
}

TEST_CASE("Holdouts: the union applies when only the higher-id holdout holds the unit out", "[holdouts][union]") {
    // Fails an implementation that stops at, or only consults, the first holdout.
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_low_id", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO),
                             new_holdout(12, "holdout_high_id", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_union_high_id_decides"), {11, 12})}));

    REQUIRE(context.treatment("exp_union_high_id_decides") == 0);
    require_published(context, {holdout_exposure(11, "holdout_low_id", 1), holdout_exposure(12, "holdout_high_id", 0)});
}

TEST_CASE("Holdouts: a unit held out by neither holdout is not suppressed", "[holdouts][union]") {
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_multi_holdout_miss"), {11, 12})}));

    REQUIRE(context.treatment("exp_multi_holdout_miss") == 0);
    require_published(context, {own_exposure(1, "exp_multi_holdout_miss", 0), holdout_exposure(11, "holdout_a", 1),
                                holdout_exposure(12, "holdout_b", 1)},
                      {{UNIT_TYPE, UID_NOT_HELD_OUT}});
}

TEST_CASE("Holdouts: a holdout suppresses a full-on experiment", "[holdouts]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_fullon_holdout", 2), {11})}));

    REQUIRE(context.treatment("exp_fullon_holdout") == 0);
}

// --- Three-arm holdouts -------------------------------------------------------------------------

TEST_CASE("Holdout arms: arm 0 holds out full-on and non-full-on experiments", "[holdouts][arms]") {
    Context context(config_with(UID),
                    data_of({new_three_arm_holdout(21, "holdout_three_arm", THREE_ARM_0_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_three_arm_0_non_fullon"), {21}),
                             covered_by(new_experiment(2, "exp_three_arm_0_fullon", 2), {21})}));

    REQUIRE(context.treatment("exp_three_arm_0_non_fullon") == 0);
    REQUIRE(context.treatment("exp_three_arm_0_fullon") == 0);
    require_published(context, {holdout_exposure(21, "holdout_three_arm", 0)});
}

TEST_CASE("Holdout arms: arm 1 forces a non-full-on experiment to control and assigns a full-on one",
          "[holdouts][arms]") {
    Context context(config_with(UID),
                    data_of({new_three_arm_holdout(21, "holdout_three_arm", THREE_ARM_1_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_three_arm_1_non_fullon"), {21}),
                             covered_by(new_experiment(2, "exp_three_arm_1_fullon", 2), {21})}));

    REQUIRE(context.treatment("exp_three_arm_1_non_fullon") == 0);
    REQUIRE(context.treatment("exp_three_arm_1_fullon") == 2);

    Expected full_on = own_exposure(2, "exp_three_arm_1_fullon", 2);
    full_on.full_on = true;
    require_published(context, {holdout_exposure(21, "holdout_three_arm", 1), full_on});
}

TEST_CASE("Holdout arms: arm 2 assigns both experiment kinds normally", "[holdouts][arms]") {
    Context context(config_with(UID),
                    data_of({new_three_arm_holdout(21, "holdout_three_arm", THREE_ARM_2_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_three_arm_2_non_fullon"), {21}),
                             covered_by(new_experiment(2, "exp_three_arm_2_fullon", 2), {21})}));

    REQUIRE(context.treatment("exp_three_arm_2_non_fullon") == NORMAL_VARIANT);
    REQUIRE(context.treatment("exp_three_arm_2_fullon") == 2);

    Expected full_on = own_exposure(2, "exp_three_arm_2_fullon", 2);
    full_on.full_on = true;
    require_published(context, {own_exposure(1, "exp_three_arm_2_non_fullon", NORMAL_VARIANT),
                                holdout_exposure(21, "holdout_three_arm", 2), full_on});
}

TEST_CASE("Holdout arms: a 2-arm holdout wins the union over a 3-arm arm 2", "[holdouts][arms][union]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_two_arm", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_three_arm_holdout(22, "holdout_three_arm", THREE_ARM_2_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_union_two_arm_wins"), {11, 22})}));

    REQUIRE(context.treatment("exp_union_two_arm_wins") == 0);
    require_published(context, {holdout_exposure(11, "holdout_two_arm", 0), holdout_exposure(22, "holdout_three_arm", 2)});
}

TEST_CASE("Holdout arms: a 3-arm arm 1 wins the union over a 2-arm arm 1", "[holdouts][arms][union]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_two_arm", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO),
                             new_three_arm_holdout(22, "holdout_three_arm", THREE_ARM_1_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_union_three_arm_wins"), {11, 22})}));

    REQUIRE(context.treatment("exp_union_three_arm_wins") == 0);
    require_published(context, {holdout_exposure(11, "holdout_two_arm", 1), holdout_exposure(22, "holdout_three_arm", 1)});
}

TEST_CASE("Holdout arms: arm 1 defers a full-on experiment to the normal path, so a strict audience still applies",
          "[holdouts][arms]") {
    for (int seed_lo : {THREE_ARM_1_SEED_LO, THREE_ARM_2_SEED_LO}) {
        const int arm = seed_lo == THREE_ARM_1_SEED_LO ? 1 : 2;
        auto experiment = covered_by(new_experiment(1, "exp_three_arm_audience_fullon", 2), {21});
        experiment.audienceStrict = true;
        experiment.audience = AGE_AUDIENCE;
        Context context(config_with(UID), data_of({new_three_arm_holdout(21, "holdout_three_arm", seed_lo)}, {experiment}));
        context.set_attribute("age", 5);

        REQUIRE(context.treatment("exp_three_arm_audience_fullon") == 0);

        Expected own = own_exposure(1, "exp_three_arm_audience_fullon", 0);
        own.assigned = false;
        own.audience_mismatch = true;
        require_published(context, {own, holdout_exposure(21, "holdout_three_arm", arm)});
    }
}

TEST_CASE("Holdout arms: the arity comes from the split, not holdoutType", "[holdouts][arms]") {
    // A 3-arm split labelled "full" (or not labelled at all) is still 3-arm, and a 2-arm split
    // labelled "all_full_on" is still 2-arm (scenario 222).
    for (const std::string type : {"", "full"}) {
        auto holdout = new_three_arm_holdout(21, "holdout_three_arm", THREE_ARM_1_SEED_LO);
        holdout.holdoutType = type;
        Context context(config_with(UID), data_of({holdout}, {covered_by(new_experiment(1, "exp_non_fullon"), {21})}));
        REQUIRE(context.treatment("exp_non_fullon") == 0);
    }

    auto two_arm = new_holdout(31, "holdout_two_arm", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, UNIT_TYPE, "all_full_on");
    Context context(config_with(UID), data_of({two_arm}, {covered_by(new_experiment(1, "exp_non_fullon"), {31})}));
    REQUIRE(context.treatment("exp_non_fullon") == NORMAL_VARIANT);
}

// --- Missing units and malformed coverage ------------------------------------------------------

TEST_CASE("Holdouts: an unconfigured unit type gets control and publishes nothing", "[holdouts]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_user", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_user_holdout", "user_id", 0), {11})}));

    REQUIRE(context.peek("exp_user_holdout") == 0);
    context.treatment("exp_user_holdout");
    // The experiment's own unassigned exposure (no unit) is unchanged from an uncovered one;
    // the holdout cannot be evaluated, so it publishes nothing.
    const auto event = context.publish();
    REQUIRE(std::none_of(event.exposures.begin(), event.exposures.end(),
                         [](const Exposure& e) { return e.id == 11; }));
}

TEST_CASE("Holdouts: assigns normally without a holdouts key", "[holdouts]") {
    Context context(config_with(UID), data_of({}, {new_experiment(1, "exp_no_holdouts_key")}));
    REQUIRE(context.peek("exp_no_holdouts_key") == NORMAL_VARIANT);
}

TEST_CASE("Holdouts: a holdout id missing from holdouts is ignored", "[holdouts]") {
    Context context(config_with(UID), data_of({}, {covered_by(new_experiment(1, "exp_dangling_holdout_id"), {999})}));
    REQUIRE(context.peek("exp_dangling_holdout_id") == NORMAL_VARIANT);
    context.treatment("exp_dangling_holdout_id");
    require_published(context, {own_exposure(1, "exp_dangling_holdout_id", NORMAL_VARIANT)});
}

// --- Precedence ---------------------------------------------------------------------------------

TEST_CASE("Holdouts: a custom assignment cannot override a held-out unit", "[holdouts][precedence]") {
    auto config = config_with(UID);
    config.custom_assignments["exp_holdout_custom"] = 3;
    Context context(config, data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                    {covered_by(new_experiment(1, "exp_holdout_custom"), {11})}));

    REQUIRE(context.peek("exp_holdout_custom") == 0);
}

TEST_CASE("Holdouts: a held-out assignment with a custom assignment is reused from the cache", "[holdouts][cache]") {
    auto config = config_with(UID);
    config.custom_assignments["exp_holdout_custom_cache"] = 3;
    Context context(config, data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                    {covered_by(new_experiment(1, "exp_holdout_custom_cache"), {11})}));

    REQUIRE(context.treatment("exp_holdout_custom_cache") == 0);
    REQUIRE(context.treatment("exp_holdout_custom_cache") == 0);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: an override takes precedence over a holdout", "[holdouts][precedence]") {
    auto config = config_with(UID);
    config.overrides["exp_holdout_override"] = 3;
    Context context(config, data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                    {covered_by(new_experiment(1, "exp_holdout_override"), {11})}));

    REQUIRE(context.peek("exp_holdout_override") == 3);
}

TEST_CASE("Holdouts: treatment on an overridden experiment still publishes the holdout, for both arms",
          "[holdouts][precedence]") {
    for (const char* uid : {UID, UID_NOT_HELD_OUT}) {
        const int arm = uid == UID ? 0 : 1;
        auto config = config_with(uid);
        config.overrides["exp_holdout_override"] = 3;
        Context context(config, data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                        {covered_by(new_experiment(1, "exp_holdout_override"), {11})}));

        REQUIRE(context.treatment("exp_holdout_override") == 3);

        Expected own = own_exposure(1, "exp_holdout_override", 3);
        own.assigned = false;
        own.overridden = true;
        require_published(context, {own, holdout_exposure(11, "holdout_a", arm)}, {{UNIT_TYPE, uid}});
    }
}

TEST_CASE("Holdouts: a holdout takes precedence over a strict audience mismatch", "[holdouts][precedence]") {
    auto experiment = covered_by(new_experiment(1, "exp_holdout_audience"), {11});
    experiment.audienceStrict = true;
    experiment.audience = AGE_AUDIENCE;
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {experiment}));
    context.set_attribute("age", 5);

    REQUIRE(context.peek("exp_holdout_audience") == 0);
    context.treatment("exp_holdout_audience");
    // Suppressed: no exposure of its own, so no audienceMismatch exposure either.
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

// --- Exposure once per (holdout, unit type) ----------------------------------------------------

TEST_CASE("Holdouts: the holdout publishes once, not once per suppressed experiment", "[holdouts][exposure]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_shared_a"), {11}),
                             covered_by(new_experiment(2, "exp_shared_b"), {11})}));

    REQUIRE(context.treatment("exp_shared_a") == 0);
    REQUIRE(context.treatment("exp_shared_b") == 0);
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

TEST_CASE("Holdouts: the holdout publishes once when the covered experiments are not suppressed",
          "[holdouts][exposure]") {
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_shared_a"), {11}),
                             covered_by(new_experiment(2, "exp_shared_b"), {11})}));

    context.treatment("exp_shared_a");
    context.treatment("exp_shared_b");
    require_published(context, {own_exposure(1, "exp_shared_a", 0), holdout_exposure(11, "holdout_a", 1),
                                own_exposure(2, "exp_shared_b", 0)},
                      {{UNIT_TYPE, UID_NOT_HELD_OUT}});
}

TEST_CASE("Holdouts: repeated calls reuse the cached suppressed assignment", "[holdouts][cache]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_cache"), {11})}));

    REQUIRE(context.treatment("exp_holdout_cache") == 0);
    REQUIRE(context.treatment("exp_holdout_cache") == 0);
    REQUIRE(context.pending() == 1);
}

// --- Refresh ------------------------------------------------------------------------------------

TEST_CASE("Holdouts: a same-iteration seed change keeps the arm and does not re-expose", "[holdouts][refresh]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_seed_thrash"), {11})}));

    REQUIRE(context.treatment("exp_holdout_seed_thrash") == 0);
    REQUIRE(context.pending() == 1);

    context.refresh(data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_seed_thrash"), {11})}));

    REQUIRE(context.treatment("exp_holdout_seed_thrash") == 0);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: a cosmetic holdout edit does not re-expose a held-out unit", "[holdouts][refresh]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_cosmetic"), {11})}));

    REQUIRE(context.treatment("exp_holdout_cosmetic") == 0);
    REQUIRE(context.pending() == 1);

    auto renamed = new_holdout(11, "holdout_a_renamed", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);
    renamed.holdoutType = "all_full_on";
    context.refresh(data_of({renamed}, {covered_by(new_experiment(1, "exp_holdout_cosmetic"), {11})}));

    REQUIRE(context.treatment("exp_holdout_cosmetic") == 0);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: a cosmetic holdout edit does not duplicate the covered experiment's exposure",
          "[holdouts][refresh]") {
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_cosmetic_covered"), {11})}));

    REQUIRE(context.treatment("exp_holdout_cosmetic_covered") == 0);
    REQUIRE(context.pending() == 2);

    context.refresh(data_of({new_holdout(11, "holdout_a_renamed", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_cosmetic_covered"), {11})}));

    REQUIRE(context.treatment("exp_holdout_cosmetic_covered") == 0);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: an iteration bump re-randomises into exactly one new arm", "[holdouts][refresh]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_iteration"), {11})}));

    REQUIRE(context.treatment("exp_holdout_iteration") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);

    auto new_epoch = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);
    new_epoch.iteration = 2;
    context.refresh(data_of({new_epoch}, {covered_by(new_experiment(1, "exp_holdout_iteration"), {11})}));

    REQUIRE(context.treatment("exp_holdout_iteration") == 0);
    REQUIRE(context.pending() == 3);
    REQUIRE(context.treatment("exp_holdout_iteration") == 0);
    REQUIRE(context.pending() == 3);
}

TEST_CASE("Holdouts: a same-iteration seed change does not desync a newly evaluated experiment from the pinned arm",
          "[holdouts][refresh]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_holdout_desync_a"), {11})}));

    REQUIRE(context.treatment("exp_holdout_desync_a") == 0);
    REQUIRE(context.pending() == 1);

    context.refresh(data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_holdout_desync_a"), {11}),
                             covered_by(new_experiment(2, "exp_holdout_desync_b"), {11})}));

    REQUIRE(context.treatment("exp_holdout_desync_b") == 0);
    REQUIRE(context.treatment("exp_holdout_desync_a") == 0);
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

TEST_CASE("Holdouts: a same-iteration 2 to 3 arm change keeps arm 1's 2-arm meaning", "[holdouts][refresh][arms]") {
    Context context(config_with(UID), data_of({new_holdout(31, "holdout_arity", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_arity_2to3_a"), {31})}));

    REQUIRE(context.treatment("exp_arity_2to3_a") == NORMAL_VARIANT);

    context.refresh(data_of({new_three_arm_holdout(31, "holdout_arity", THREE_ARM_1_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_arity_2to3_a"), {31}),
                             covered_by(new_experiment(2, "exp_arity_2to3_b"), {31})}));

    REQUIRE(context.treatment("exp_arity_2to3_b") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_arity_2to3_a", NORMAL_VARIANT),
                                holdout_exposure(31, "holdout_arity", 1),
                                own_exposure(2, "exp_arity_2to3_b", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: a same-iteration 3 to 2 arm change keeps arm 1's 3-arm meaning", "[holdouts][refresh][arms]") {
    Context context(config_with(UID), data_of({new_three_arm_holdout(41, "holdout_arity_rev", THREE_ARM_1_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_arity_3to2_a"), {41})}));

    REQUIRE(context.treatment("exp_arity_3to2_a") == 0);
    REQUIRE(context.pending() == 1);

    context.refresh(data_of({new_holdout(41, "holdout_arity_rev", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_arity_3to2_a"), {41}),
                             covered_by(new_experiment(2, "exp_arity_3to2_b"), {41})}));

    REQUIRE(context.treatment("exp_arity_3to2_b") == 0);
    require_published(context, {holdout_exposure(41, "holdout_arity_rev", 1)});
}

TEST_CASE("Holdouts: experiments evaluated before and after a same-iteration arity change agree",
          "[holdouts][refresh][arms]") {
    Context context(config_with(UID),
                    data_of({new_holdout(71, "holdout_arity_consistency", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_arity_consistency_pre"), {71})}));

    REQUIRE(context.treatment("exp_arity_consistency_pre") == NORMAL_VARIANT);

    context.refresh(data_of({new_three_arm_holdout(71, "holdout_arity_consistency", THREE_ARM_1_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_arity_consistency_pre"), {71}),
                             covered_by(new_experiment(2, "exp_arity_consistency_post"), {71})}));

    REQUIRE(context.treatment("exp_arity_consistency_post") == NORMAL_VARIANT);
    REQUIRE(context.treatment("exp_arity_consistency_pre") == NORMAL_VARIANT);
}

TEST_CASE("Holdouts: a refresh between the decision and the holdout trigger publishes the decision's epoch",
          "[holdouts][refresh]") {
    auto iteration_2 = new_holdout(11, "holdout_h", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);
    iteration_2.iteration = 2;
    const auto refreshed = data_of({iteration_2}, {covered_by(new_experiment(1, "exp_epoch_race"), {11})});

    auto logger = std::make_shared<ExposureLog>();
    logger->on_exposure = [&](Context& context, const json& data) {
        if (data["id"] == 1) context.refresh(refreshed);
    };
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_h", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_epoch_race"), {11})}),
                    logger);

    REQUIRE(context.treatment("exp_epoch_race") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_epoch_race", NORMAL_VARIANT), holdout_exposure(11, "holdout_h", 1)});
}

// --- Late units ---------------------------------------------------------------------------------

TEST_CASE("Holdouts: set_unit evicts an unresolved holdout pin so its exposure is not lost", "[holdouts][late-unit]") {
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_user_id", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_null_snapshot_fullon", "user_id", 2), {11})}));

    REQUIRE(context.peek("exp_null_snapshot_fullon") == 2);
    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_null_snapshot_fullon") == 2);
    REQUIRE(context.pending() == 2);

    Expected own = own_exposure(1, "exp_null_snapshot_fullon", 2, "user_id");
    own.full_on = true;
    require_published(context, {own, holdout_exposure(11, "holdout_user_id", 1, "user_id")}, {{"user_id", UID}});
}

TEST_CASE("Holdouts: set_unit keeps an exposed assignment, so there is no duplicate exposure", "[holdouts][late-unit]") {
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_user_id", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_exposed_before_unit", "user_id", 2), {11})}));

    REQUIRE(context.treatment("exp_exposed_before_unit") == 2);
    REQUIRE(context.pending() == 1);

    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_exposed_before_unit") == 2);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: set_unit of an unrelated unit type evicts nothing", "[holdouts][late-unit]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_unrelated_unit"), {11})}));

    REQUIRE(context.treatment("exp_unrelated_unit") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);

    context.set_unit("other_unit_type", "some-other-uid");

    REQUIRE(context.treatment("exp_unrelated_unit") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: set_unit evicts only the affected assignment", "[holdouts][late-unit]") {
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_late", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id"),
                             new_holdout(12, "holdout_present", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_late_unit", "user_id", 2), {11}),
                             covered_by(new_experiment(2, "exp_present_unit"), {12})}));

    REQUIRE(context.peek("exp_late_unit") == 2);
    REQUIRE(context.treatment("exp_present_unit") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);

    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_late_unit") == 2);
    REQUIRE(context.pending() == 4);
    REQUIRE(context.treatment("exp_present_unit") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 4);
}

TEST_CASE("Holdouts: set_unit evicts by the covered experiment's unit type, not the holdout's", "[holdouts][late-unit]") {
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_declares_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "B")},
                            {covered_by(new_experiment(1, "exp_mismatched_unit_type", "A", 2), {11})}));

    REQUIRE(context.peek("exp_mismatched_unit_type") == 2);
    context.set_unit("A", UID);

    REQUIRE(context.treatment("exp_mismatched_unit_type") == 2);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: set_unit re-resolves suppression when the late holdout holds the unit out", "[holdouts][late-unit]") {
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_user_id_suppress", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_null_snapshot_suppresses", "user_id", 2), {11})}));

    REQUIRE(context.peek("exp_null_snapshot_suppresses") == 2);
    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_null_snapshot_suppresses") == 0);
    REQUIRE(context.pending() == 1);
    require_published(context, {holdout_exposure(11, "holdout_user_id_suppress", 0, "user_id")}, {{"user_id", UID}});
}

// Java's detachedTreatment*/settleExposure/retire tests exercise its lock-free exposure CAS and
// the retry of a "detached" Assignment replaced under a concurrent reader. The C++ Context is
// not thread-safe and has no detached assignments; the closest reachable case is an assignment
// replaced while an event logger runs. These pin the observable outcomes of those tests.

TEST_CASE("Holdouts: a replaced assignment cannot publish beside a suppressing replacement", "[holdouts][late-unit]") {
    // detachedTreatmentAssignmentCannotPublishBesideSuppressingReplacement
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_detached_suppressed", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_detached_suppressed", "user_id", 2), {11})}));

    REQUIRE(context.peek("exp_detached_suppressed") == 2);
    context.set_unit("user_id", UID);
    REQUIRE(context.treatment("exp_detached_suppressed") == 0);
    REQUIRE(context.pending() == 1);
    REQUIRE(context.treatment("exp_detached_suppressed") == 0);
    REQUIRE(context.pending() == 1);
    require_published(context, {holdout_exposure(11, "holdout_detached_suppressed", 0, "user_id")}, {{"user_id", UID}});
}

TEST_CASE("Holdouts: a replaced assignment's exposure is not lost or duplicated", "[holdouts][late-unit]") {
    // detachedTreatmentAssignmentRetriesCurrentEntryWithoutLosingExposure
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_detached_retry", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_detached_retry", "user_id", 2), {11})}));

    REQUIRE(context.peek("exp_detached_retry") == 2);
    context.set_unit("user_id", UID);
    REQUIRE(context.treatment("exp_detached_retry") == 2);
    REQUIRE(context.pending() == 2);
    REQUIRE(context.treatment("exp_detached_retry") == 2);
    REQUIRE(context.pending() == 2);

    Expected own = own_exposure(1, "exp_detached_retry", 2, "user_id");
    own.full_on = true;
    require_published(context, {own, holdout_exposure(11, "holdout_detached_retry", 1, "user_id")}, {{"user_id", UID}});
}

TEST_CASE("Holdouts: treatment returns the re-resolved variant after a late unit", "[holdouts][late-unit]") {
    // detachedTreatmentReturnsTheReplacementVariant
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_detached_variant", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_detached_variant", "user_id", 2), {11})}));

    REQUIRE(context.peek("exp_detached_variant") == 2);
    context.set_unit("user_id", UID);
    REQUIRE(context.treatment("exp_detached_variant") == 0);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: an assignment replaced while the logger runs settles on one exposure", "[holdouts][late-unit]") {
    // treatmentExposureSettlesAfterTwoConsecutiveRetirements and
    // exposureSettlementStopsAfterTheBoundedNumberOfRetirements: a logger that replaces the
    // assignment (here by a refresh changing the holdout coverage) mid-exposure neither loses
    // nor repeats the exposure that was already queued, and never loops.
    const auto refreshed = data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                   {covered_by(new_experiment(1, "exp_settled", 1), {11})});
    auto logger = std::make_shared<ExposureLog>();
    int refreshes = 0;
    logger->on_exposure = [&](Context& context, const json&) {
        if (refreshes++ < 2) context.refresh(refreshed);
    };
    Context context(config_with(UID), data_of({}, {new_experiment(1, "exp_settled", 1)}), logger);

    REQUIRE(context.treatment("exp_settled") == 1);
    REQUIRE(context.pending() == 1);
    // The refreshed coverage is a new decision: the experiment and its holdout publish once each.
    REQUIRE(context.treatment("exp_settled") == 1);
    REQUIRE(context.treatment("exp_settled") == 1);
    REQUIRE(context.pending() == 3);
}

TEST_CASE("Holdouts: a variable key re-resolves to a new winner after a late unit and refresh",
          "[holdouts][late-unit][variables]") {
    // detachedVariableAssignmentReresolvesTheKeyToANewWinningExperiment
    auto original = covered_by(new_experiment(2, "exp_detached_variable_old", "user_id", 1), {11});
    original.variants[1].config = R"({"detached_var":"old"})";
    const auto holdout = new_holdout(11, "holdout_detached_variable", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id");
    Context context(ContextConfig{}, data_of({holdout}, {original}));

    REQUIRE(context.peek_variable_value("detached_var", "default") == "old");
    context.set_unit("user_id", UID);

    auto winner = covered_by(new_experiment(1, "exp_detached_variable_winner", "user_id", 1), {11});
    winner.variants[1].config = R"({"detached_var":"new winner"})";
    context.refresh(data_of({holdout}, {winner, original}));

    REQUIRE(context.variable_value("detached_var", "default") == "new winner");
    REQUIRE(context.pending() == 2);
    REQUIRE(context.variable_value("detached_var", "default") == "new winner");
    REQUIRE(context.pending() == 2);

    Expected own = own_exposure(1, "exp_detached_variable_winner", 1, "user_id");
    own.full_on = true;
    require_published(context, {own, holdout_exposure(11, "holdout_detached_variable", 1, "user_id")},
                      {{"user_id", UID}});
}

TEST_CASE("Holdouts: an exposed assignment's unresolved pin is never resolved live later", "[holdouts][late-unit]") {
    auto experiment = covered_by(new_experiment(1, "exp_a_mutant_pinned_null", "user_id", 1), {11});
    experiment.variants[1].config = R"({"my_var":"value_from_variant"})";
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_a_mutant_pinned_null", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO,
                                         "user_id")},
                            {experiment}));

    REQUIRE(context.treatment("exp_a_mutant_pinned_null") == 1);
    REQUIRE(context.pending() == 1);

    context.set_unit("user_id", UID);

    REQUIRE(context.variable_value("my_var", "default") == "value_from_variant");
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: set_unit evicts an assignment whose holdouts declare other unit types", "[holdouts][late-unit]") {
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_11", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "type_x"),
                             new_holdout(12, "holdout_12", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "type_y"),
                             new_holdout(13, "holdout_13", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_multi_holdout_unit_types", "user_id", 2), {11, 12, 13})}));

    REQUIRE(context.peek("exp_multi_holdout_unit_types") == 2);
    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_multi_holdout_unit_types") == 2);
    REQUIRE(context.pending() == 4);
}

TEST_CASE("Holdouts: one holdout over two unit types keeps an exposure state per unit type", "[holdouts][exposure]") {
    ContextConfig config;
    config.units = {{"user_id", UID}, {"session_id", UID}};
    Context context(config, data_of({new_holdout(11, "holdout_shared", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                                    {covered_by(new_experiment(1, "exp_unit_a1", "user_id", 0), {11}),
                                     covered_by(new_experiment(2, "exp_unit_b", "session_id", 0), {11}),
                                     covered_by(new_experiment(3, "exp_unit_a2", "user_id", 0), {11})}));

    REQUIRE(context.treatment("exp_unit_a1") == NORMAL_VARIANT);
    REQUIRE(context.treatment("exp_unit_b") == NORMAL_VARIANT);
    REQUIRE(context.treatment("exp_unit_a2") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 5);

    const auto event = context.publish();
    std::vector<std::string> holdout_units;
    for (const auto& exposure : event.exposures) {
        if (exposure.id == 11) holdout_units.push_back(exposure.unit);
    }
    REQUIRE(holdout_units == std::vector<std::string>{"user_id", "session_id"});
}

TEST_CASE("Holdouts: set_unit with nothing cached assigns normally", "[holdouts][late-unit]") {
    Context context(ContextConfig{}, data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                             {covered_by(new_experiment(1, "exp_no_prior_assignment"), {11})}));

    REQUIRE_NOTHROW(context.set_unit(UNIT_TYPE, UID));
    REQUIRE(context.treatment("exp_no_prior_assignment") == NORMAL_VARIANT);
}

TEST_CASE("Holdouts: set_unit with the same uid again is a no-op", "[holdouts][late-unit]") {
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_set_unit_twice"), {11})}));

    REQUIRE(context.treatment("exp_set_unit_twice") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);

    REQUIRE_NOTHROW(context.set_unit(UNIT_TYPE, UID));

    REQUIRE(context.treatment("exp_set_unit_twice") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a redundant set_unit does not evict a fully resolved unexposed assignment",
          "[holdouts][late-unit]") {
    // A same-iteration split change is invisible to the cache check, so a wrongly evicted
    // assignment would re-resolve to variant 0.
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO);
    Context context(config_with(UID), data_of({holdout}, {covered_by(new_experiment(1, "exp_redundant_set_unit"), {11})}));

    REQUIRE(context.peek("exp_redundant_set_unit") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 0);

    auto refreshed = covered_by(new_experiment(1, "exp_redundant_set_unit"), {11});
    refreshed.split = {1.0, 0.0};
    context.refresh(data_of({holdout}, {refreshed}));

    context.set_unit(UNIT_TYPE, UID);

    REQUIRE(context.treatment("exp_redundant_set_unit") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_redundant_set_unit", NORMAL_VARIANT),
                                holdout_exposure(11, "holdout_a", 1)});
}

TEST_CASE("Holdouts: peek_variable_value, a late unit, then variable_value re-resolves under a suppressing holdout",
          "[holdouts][late-unit][variables]") {
    auto experiment = covered_by(new_experiment(1, "exp_var_late_unit_suppress", "user_id", 1), {11});
    experiment.variants[1].config = R"({"my_var":"value_from_variant"})";
    Context context(ContextConfig{},
                    data_of({new_holdout(11, "holdout_var_late", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO, "user_id")},
                            {experiment}));

    REQUIRE(context.peek_variable_value("my_var", "default") == "value_from_variant");
    REQUIRE(context.pending() == 0);

    context.set_unit("user_id", UID);

    REQUIRE(context.variable_value("my_var", "default") == "default");
    REQUIRE(context.pending() == 1);
    require_published(context, {holdout_exposure(11, "holdout_var_late", 0, "user_id")}, {{"user_id", UID}});
}

TEST_CASE("Holdouts: an override peeked before its unit still publishes the holdout once the unit is set",
          "[holdouts][late-unit]") {
    ContextConfig config;
    config.overrides["exp_override_late_unit"] = 3;
    Context context(config,
                    data_of({new_holdout(11, "holdout_override_late", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_override_late_unit", "user_id", 0), {11})}));

    REQUIRE(context.peek("exp_override_late_unit") == 3);
    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_override_late_unit") == 3);
    REQUIRE(context.pending() == 2);

    Expected own = own_exposure(1, "exp_override_late_unit", 3, "user_id");
    own.assigned = false;
    own.overridden = true;
    require_published(context, {own, holdout_exposure(11, "holdout_override_late", 1, "user_id")}, {{"user_id", UID}});
}

TEST_CASE("Holdouts: a custom assignment applies after a late unit re-resolves the assignment", "[holdouts][late-unit]") {
    ContextConfig config;
    config.custom_assignments["exp_custom_late_unit"] = 3;
    Context context(config,
                    data_of({new_holdout(11, "holdout_custom_late", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO, "user_id")},
                            {covered_by(new_experiment(1, "exp_custom_late_unit", "user_id", 0), {11})}));

    REQUIRE(context.peek("exp_custom_late_unit") == 0);
    context.set_unit("user_id", UID);

    REQUIRE(context.treatment("exp_custom_late_unit") == 3);
    REQUIRE(context.pending() == 2);

    Expected own = own_exposure(1, "exp_custom_late_unit", 3, "user_id");
    own.custom = true;
    require_published(context, {own, holdout_exposure(11, "holdout_custom_late", 1, "user_id")}, {{"user_id", UID}});
}

// --- Cross-SDK verdict vectors (java-sdk, checked against the collector) ----------------------

namespace {

const char* VERDICT_UNIT_TYPE = "verdict_unit_type";

HoldoutData verdict_holdout(int id, int32_t seed_hi, int32_t seed_lo, std::vector<double> split) {
    auto holdout = new_holdout(id, "holdout_" + std::to_string(id), seed_hi, seed_lo, VERDICT_UNIT_TYPE);
    holdout.split = std::move(split);
    return holdout;
}

std::unique_ptr<Context> verdict_context(const std::string& uid, const std::vector<HoldoutData>& holdouts) {
    std::vector<int> ids;
    for (const auto& holdout : holdouts) ids.push_back(holdout.id);
    return std::make_unique<Context>(
        config_with(uid, VERDICT_UNIT_TYPE),
        data_of(holdouts, {covered_by(new_experiment(9, "verdict_vectors_experiment", VERDICT_UNIT_TYPE, 0), ids)}));
}

void require_held_out_by(const std::string& uid, const HoldoutData& holdout) {
    auto context = verdict_context(uid, {holdout});
    REQUIRE(context->treatment("verdict_vectors_experiment") == 0);
    const auto event = context->publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].id == holdout.id);
    REQUIRE(event.exposures[0].name == holdout.name);
    REQUIRE(event.exposures[0].variant == 0);
}

void require_not_held_out(const std::string& uid, const HoldoutData& holdout) {
    auto context = verdict_context(uid, {holdout});
    context->treatment("verdict_vectors_experiment");
    const auto event = context->publish();
    REQUIRE(std::any_of(event.exposures.begin(), event.exposures.end(), [&](const Exposure& e) {
        return e.id == holdout.id && e.name == holdout.name && e.variant == 1;
    }));
}

} // namespace

TEST_CASE("Holdout verdicts: Unicode unit ids", "[holdouts][verdicts]") {
    require_held_out_by("fa\xC3\xA7" "ade", verdict_holdout(107, 0, 42, {0.1, 0.9}));
    require_not_held_out("unicode_emoji_\xF0\x9F\x9A\x80_0", verdict_holdout(107, 0, 42, {0.1, 0.9}));
}

TEST_CASE("Holdout verdicts: seed halves with the sign bit set", "[holdouts][verdicts]") {
    const int64_t seed = static_cast<int64_t>(0x8000000080000000ULL);
    const auto seed_hi = static_cast<int32_t>(seed >> 32);
    const auto seed_lo = static_cast<int32_t>(static_cast<uint32_t>(seed & 0xffffffff));
    require_held_out_by("signed_unit_4", verdict_holdout(106, seed_hi, seed_lo, {0.1, 0.9}));
    require_not_held_out("signed_unit_b_1", verdict_holdout(106, seed_hi, seed_lo, {0.1, 0.9}));
}

TEST_CASE("Holdout verdicts: 1% boundary", "[holdouts][verdicts]") {
    require_held_out_by("pct1_unit_188", verdict_holdout(105, 0, 999999, {0.01, 0.99}));
    require_not_held_out("pct1_unit_0", verdict_holdout(105, 0, 999999, {0.01, 0.99}));
}

TEST_CASE("Holdout verdicts: either side of the 10% boundary", "[holdouts][verdicts]") {
    require_held_out_by("boundary_unit_175653", verdict_holdout(107, 0, 42, {0.1, 0.9}));
    require_not_held_out("boundary_unit_75792", verdict_holdout(107, 0, 42, {0.1, 0.9}));
}

TEST_CASE("Holdout verdicts: a unit held out by two holdouts at once", "[holdouts][verdicts]") {
    auto context = verdict_context("dual_holdout_unit_5", {verdict_holdout(107, 0, 42, {0.1, 0.9}),
                                                           verdict_holdout(108, 0, 55, {0.1, 0.9})});
    REQUIRE(context->treatment("verdict_vectors_experiment") == 0);
    require_published(*context, {holdout_exposure(107, "holdout_107", 0, VERDICT_UNIT_TYPE),
                                 holdout_exposure(108, "holdout_108", 0, VERDICT_UNIT_TYPE)},
                      {{VERDICT_UNIT_TYPE, "dual_holdout_unit_5"}});
}

// --- Variables ----------------------------------------------------------------------------------

TEST_CASE("Holdouts: variable_value publishes the holdout for held-out and not-held-out units",
          "[holdouts][variables]") {
    auto experiment = covered_by(new_experiment(1, "exp_var"), {11});
    experiment.variants[1].config = R"({"var_a":"value_a"})";
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);

    Context held_out(config_with(UID), data_of({holdout}, {experiment}));
    REQUIRE(held_out.variable_value("var_a", "default") == "default");
    REQUIRE(held_out.pending() == 1);

    Context not_held_out(config_with(UID_NOT_HELD_OUT), data_of({holdout}, {experiment}));
    REQUIRE(not_held_out.variable_value("var_a", "default") == "default");
    REQUIRE(not_held_out.pending() == 2);
}

TEST_CASE("Holdouts: a variable key skips a suppressed experiment for an assigned one", "[holdouts][variables]") {
    auto suppressed = covered_by(new_experiment(1, "exp_var_key_suppressed"), {11});
    suppressed.variants[1].config = R"({"shared":"from_suppressed"})";
    auto assigned = new_experiment(2, "exp_var_key_assigned");
    assigned.variants[1].config = R"({"shared":"from_assigned"})";
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {suppressed, assigned}));

    REQUIRE(context.variable_value("shared", "default") == "from_assigned");
    require_published(context, {holdout_exposure(11, "holdout_a", 0),
                                own_exposure(2, "exp_var_key_assigned", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: peek_variable_value publishes nothing", "[holdouts][variables][peek]") {
    auto suppressed = covered_by(new_experiment(1, "exp_var_key_peek_suppressed"), {11});
    suppressed.variants[1].config = R"({"shared_peek":"from_suppressed"})";
    auto assigned = new_experiment(2, "exp_var_key_peek_assigned");
    assigned.variants[1].config = R"({"shared_peek":"from_assigned"})";
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {suppressed, assigned}));

    REQUIRE(context.peek_variable_value("shared_peek", "default") == "from_assigned");
    REQUIRE(context.pending() == 0);
}

TEST_CASE("Holdouts: a suppressed experiment's variable reads its control value, not the default",
          "[holdouts][variables]") {
    // Scenario 226: a held-out unit sees control, as the experiment would show it.
    auto experiment = with_variables(covered_by(new_experiment(1, "exp_held_out"), {11}), "color");
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {experiment}));

    REQUIRE(context.peek_variable_value("color", "default") == "control");
    REQUIRE(context.variable_value("color", "default") == "control");
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

// --- Event logger ---------------------------------------------------------------------------------

TEST_CASE("Holdouts: a throwing logger does not lose a sibling holdout exposure", "[holdouts][logger]") {
    auto logger = std::make_shared<ThrowingLogger>();
    logger->throws_on = [](const json& data) { return data["id"] == 11; };
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_multi_holdout_throwing_logger"), {11, 12})}),
                    logger);

    // The C++ SDK never lets a logger exception reach the caller.
    REQUIRE(context.treatment("exp_multi_holdout_throwing_logger") == 0);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a throwing logger does not make an exposure retryable", "[holdouts][logger]") {
    auto logger = std::make_shared<ThrowingLogger>();
    logger->throws_on = [](const json& data) { return data["id"] == 1; };
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_terminal", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_throwing_logger_terminal"), {11})}),
                    logger);

    REQUIRE(context.treatment("exp_throwing_logger_terminal") == NORMAL_VARIANT);
    logger->throws_on = [](const json&) { return false; };
    REQUIRE(context.treatment("exp_throwing_logger_terminal") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_throwing_logger_terminal", NORMAL_VARIANT),
                                holdout_exposure(11, "holdout_terminal", 1)});
}

TEST_CASE("Holdouts: a throwing logger still queues a suppressed experiment's only holdout exposure",
          "[holdouts][logger]") {
    // Java checks a flush is scheduled; the C++ Context publishes on demand, so the exposure
    // must be pending and published.
    auto logger = std::make_shared<ThrowingLogger>();
    logger->throws_on = [](const json&) { return true; };
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_stranded_flush"), {11})}),
                    logger);

    REQUIRE(context.treatment("exp_stranded_flush") == 0);
    REQUIRE(context.pending() == 1);
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

// --- Malformed holdouts -------------------------------------------------------------------------

TEST_CASE("Holdouts: a null holdouts entry is ignored", "[holdouts][malformed]") {
    const auto data = json::parse(R"({"experiments":[],"holdouts":[null]})").get<ContextData>();
    auto with_experiment = data;
    with_experiment.experiments.push_back(new_experiment(1, "exp_null_holdout_entry"));
    Context context(config_with(UID), with_experiment);

    REQUIRE(context.treatment("exp_null_holdout_entry") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_null_holdout_entry", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: a holdout without a split is ignored", "[holdouts][malformed]") {
    for (const char* split : {"null", "[]", "absent"}) {
        json holdout = {{"id", 11}, {"name", "holdout_no_split"}, {"iteration", 1}, {"unitType", UNIT_TYPE},
                        {"seedHi", HOLDOUT_A_SEED_HI}, {"seedLo", HOLDOUT_A_SEED_LO}};
        if (std::string(split) != "absent") holdout["split"] = json::parse(split);
        auto data = json({{"experiments", json::array()}, {"holdouts", json::array({holdout})}}).get<ContextData>();
        data.experiments.push_back(covered_by(new_experiment(1, "exp_no_split_holdout"), {11}));
        Context context(config_with(UID), data);

        REQUIRE(context.treatment("exp_no_split_holdout") == NORMAL_VARIANT);
        require_published(context, {own_exposure(1, "exp_no_split_holdout", NORMAL_VARIANT)});
    }
}

// --- Refresh with overrides ---------------------------------------------------------------------

TEST_CASE("Holdouts: an epoch already exposed is not exposed again by an older experiment's pin",
          "[holdouts][refresh]") {
    // staleHoldoutReferenceNeverOverwritesNewerPinnedCacheEntry: Java calls a private trigger
    // with a stale holdout; here every reachable path resolves the live definition by id, so
    // re-evaluating the experiment pinned to iteration 1 after G exposed iteration 2 must not
    // expose iteration 2 again.
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_h", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {covered_by(new_experiment(1, "exp_f"), {11})}));

    REQUIRE(context.treatment("exp_f") == 0);
    REQUIRE(context.pending() == 1);

    auto iteration_2 = new_holdout(11, "holdout_h", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO);
    iteration_2.iteration = 2;
    context.refresh(data_of({iteration_2}, {covered_by(new_experiment(1, "exp_f"), {11}),
                                            covered_by(new_experiment(2, "exp_g"), {11})}));

    REQUIRE(context.treatment("exp_g") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 3);

    // exp_f is re-resolved against iteration 2: no longer held out, so it publishes its own
    // exposure, but the holdout's iteration 2 exposure is not repeated.
    REQUIRE(context.treatment("exp_f") == NORMAL_VARIANT);
    require_published(context, {holdout_exposure(11, "holdout_h", 0), own_exposure(2, "exp_g", NORMAL_VARIANT),
                                holdout_exposure(11, "holdout_h", 1), own_exposure(1, "exp_f", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: a holdout added by a refresh fires for an overridden experiment", "[holdouts][refresh]") {
    auto config = config_with(UID);
    config.overrides["exp_override_late_holdout"] = 3;
    Context context(config, data_of({}, {new_experiment(1, "exp_override_late_holdout")}));

    REQUIRE(context.treatment("exp_override_late_holdout") == 3);
    REQUIRE(context.pending() == 1);

    context.refresh(data_of({new_holdout(11, "holdout_late", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_override_late_holdout"), {11})}));

    REQUIRE(context.treatment("exp_override_late_holdout") == 3);
    REQUIRE(context.pending() == 3);

    Expected own = own_exposure(1, "exp_override_late_holdout", 3);
    own.assigned = false;
    own.overridden = true;
    require_published(context, {own, own, holdout_exposure(11, "holdout_late", 0)});
}

TEST_CASE("Holdouts: a holdout exposure from the variable path is published", "[holdouts][variables]") {
    // variablePathHoldoutExposureSchedulesFlushWithoutAnyOtherEvent: no scheduler in C++.
    auto suppressed = covered_by(new_experiment(1, "exp_var_flush_suppressed"), {11});
    suppressed.variants[1].config = R"({"flush_key":"from_suppressed"})";
    auto assigned = new_experiment(2, "exp_var_flush_assigned");
    assigned.variants[1].config = R"({"flush_key":"from_assigned"})";
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_flush", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {suppressed, assigned}));

    REQUIRE(context.treatment("exp_var_flush_assigned") == NORMAL_VARIANT);
    REQUIRE(context.pending() == 1);
    REQUIRE(context.variable_value("flush_key", "default") == "from_assigned");
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: installing data does not change the caller's holdoutIds", "[holdouts]") {
    auto covered = new_experiment(1, "exp_holdout_no_mutate_in");
    covered.holdoutIds = {12, 11};
    const auto data = data_of({new_holdout(11, "holdout_no_mutate", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                              {covered, new_experiment(2, "exp_holdout_no_mutate_not_covered")});
    Context context(config_with(UID), data);

    REQUIRE(context.treatment("exp_holdout_no_mutate_in") == 0);
    REQUIRE(context.treatment("exp_holdout_no_mutate_not_covered") == NORMAL_VARIANT);
    REQUIRE(data.experiments[0].holdoutIds == std::vector<int>{12, 11});
    REQUIRE(context.data().experiments[0].holdoutIds == std::vector<int>{12, 11});
}

// --- Assignment rules (scenario 305) ------------------------------------------------------------

TEST_CASE("Holdouts: a matching assign rule wins over suppression and the holdout still publishes",
          "[holdouts][rules]") {
    auto experiment = covered_by(new_experiment(1, "exp_holdout_rules"), {11});
    experiment.assignmentRules = US_RULE;
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_rules_suppression", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {experiment}));
    context.set_attribute("country", "US");

    REQUIRE(context.treatment("exp_holdout_rules") == 1);

    Expected own = own_exposure(1, "exp_holdout_rules", 1);
    own.assigned = false;
    own.rule_override = true;
    require_published(context, {own, holdout_exposure(11, "holdout_rules_suppression", 0)});
}

TEST_CASE("Holdouts: a matching split rule wins over suppression", "[holdouts][rules]") {
    auto experiment = covered_by(new_experiment(1, "exp_holdout_split_rule"), {11});
    experiment.assignmentRules =
        R"({"rules":[{"name":"Everyone","type":"split","conditions":null,"environments":[],"percentages":"0/100"}]})";
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {experiment}));

    REQUIRE(context.peek("exp_holdout_split_rule") == 1);
    REQUIRE(context.treatment("exp_holdout_split_rule") == 1);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a held-out unit is suppressed until a rule matches", "[holdouts][rules]") {
    auto experiment = covered_by(new_experiment(1, "exp_holdout_rules"), {11});
    experiment.assignmentRules = US_RULE;
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {experiment}));
    context.set_attribute("country", "GB");

    REQUIRE(context.treatment("exp_holdout_rules") == 0);
    REQUIRE(context.pending() == 1);

    context.set_attribute("country", "US");
    REQUIRE(context.treatment("exp_holdout_rules") == 1);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a held-out unit is suppressed again when the rule stops matching", "[holdouts][rules]") {
    auto experiment = covered_by(new_experiment(1, "exp_holdout_rules"), {11});
    experiment.assignmentRules = US_RULE;
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {experiment}));
    context.set_attribute("country", "US");

    REQUIRE(context.treatment("exp_holdout_rules") == 1);
    REQUIRE(context.pending() == 2);

    context.set_attribute("country", "GB");
    REQUIRE(context.treatment("exp_holdout_rules") == 0);
    REQUIRE(context.peek("exp_holdout_rules") == 0);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: variable_value on a held-out, rule-won experiment", "[holdouts][rules][variables]") {
    auto experiment = with_variables(covered_by(new_experiment(1, "exp_holdout_rules"), {11}), "color");
    experiment.assignmentRules = US_RULE;
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_rules_suppression", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {experiment}));
    context.set_attribute("country", "US");

    REQUIRE(context.variable_value("color", "default") == "treatment");

    Expected own = own_exposure(1, "exp_holdout_rules", 1);
    own.assigned = false;
    own.rule_override = true;
    require_published(context, {own, holdout_exposure(11, "holdout_rules_suppression", 0)});
}

TEST_CASE("Holdouts: a shared variable key prefers a rule-won experiment over a suppressed one",
          "[holdouts][rules][variables]") {
    auto held_out = with_variables(covered_by(new_experiment(1, "exp_held_out"), {11}), "color");
    auto rule_won = with_variables(covered_by(new_experiment(2, "exp_rule_won"), {11}), "color");
    rule_won.assignmentRules = US_RULE;
    Context context(config_with(UID), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                              {held_out, rule_won}));

    context.set_attribute("country", "GB");
    REQUIRE(context.peek_variable_value("color", "default") == "control");

    context.set_attribute("country", "US");
    REQUIRE(context.peek_variable_value("color", "default") == "treatment");
}

TEST_CASE("Holdouts: a shared variable key prefers an earlier assigned experiment over a later rule-won one",
          "[holdouts][rules][variables]") {
    auto assigned = with_variables(new_experiment(1, "exp_assigned"), "color");
    assigned.split = {1.0, 0.0};
    auto rule_won = with_variables(new_experiment(2, "exp_rule_won"), "color");
    rule_won.assignmentRules = US_RULE;
    Context context(config_with(UID), data_of({}, {assigned, rule_won}));
    context.set_attribute("country", "US");

    REQUIRE(context.peek_variable_value("color", "default") == "control");
    REQUIRE(context.peek("exp_rule_won") == 1);
}

// --- Shared holdout scenarios H1-H8 (cross-sdk-tests) ---------------------------------------------

TEST_CASE("Holdouts H1: a duplicate holdout id publishes one holdout exposure", "[holdouts][conformance]") {
    // javascript-sdk dedups holdoutIds (insertUniqueSorted).
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_duplicate_ids"), {11, 11})}));

    REQUIRE(context.treatment("exp_duplicate_ids") == 0);
    require_published(context, {own_exposure(1, "exp_duplicate_ids", 0), holdout_exposure(11, "holdout_a", 1)},
                      {{UNIT_TYPE, UID_NOT_HELD_OUT}});
}

TEST_CASE("Holdouts H1: duplicate and unsorted ids resolve once each, in id order", "[holdouts][conformance]") {
    Context context(config_with(UID),
                    data_of({new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO),
                             new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_unsorted_ids"), {12, 11, 12, 999, 11})}));

    REQUIRE(context.treatment("exp_unsorted_ids") == 0);
    require_published(context, {holdout_exposure(11, "holdout_a", 0), holdout_exposure(12, "holdout_b", 1)});
}

TEST_CASE("Holdouts H4: peek on a covered experiment queues nothing", "[holdouts][conformance][peek]") {
    for (const char* uid : {UID, UID_NOT_HELD_OUT}) {
        Context context(config_with(uid), data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                                  {covered_by(new_experiment(1, "exp_peek"), {11})}));
        context.peek("exp_peek");
        context.peek("exp_peek");
        REQUIRE(context.pending() == 0);

        context.treatment("exp_peek");
        REQUIRE(context.pending() == (uid == UID ? 1 : 2));
    }
}

TEST_CASE("Holdouts H7: a strict audience mismatch on a covered, not-held-out experiment still fires the holdout",
          "[holdouts][conformance]") {
    auto experiment = covered_by(new_experiment(1, "exp_strict_mismatch"), {11});
    experiment.audienceStrict = true;
    experiment.audience = AGE_AUDIENCE;
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)}, {experiment}));
    context.set_attribute("age", 5);

    REQUIRE(context.treatment("exp_strict_mismatch") == 0);

    Expected own = own_exposure(1, "exp_strict_mismatch", 0);
    own.assigned = false;
    own.audience_mismatch = true;
    const auto event = context.publish();
    REQUIRE(event.exposures.size() == 2);
    REQUIRE(to_json_actual(event.exposures[0]) == to_json_expected(own));
    REQUIRE(to_json_actual(event.exposures[1]) == to_json_expected(holdout_exposure(11, "holdout_a", 1)));
}

TEST_CASE("Holdouts H8: a shared key whose first candidate is suppressed and second assigned fires both holdouts",
          "[holdouts][conformance][variables]") {
    auto suppressed = with_variables(covered_by(new_experiment(1, "exp_first_suppressed"), {11}), "color");
    auto assigned = with_variables(covered_by(new_experiment(2, "exp_second_assigned"), {12}), "color");
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {suppressed, assigned}));

    REQUIRE(context.variable_value("color", "default") == "treatment");
    require_published(context, {holdout_exposure(11, "holdout_a", 0),
                                own_exposure(2, "exp_second_assigned", NORMAL_VARIANT),
                                holdout_exposure(12, "holdout_b", 1)});
}

// --- Payload decoding and forward compatibility -------------------------------------------------

namespace {

std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(ABSMARTLY_TEST_FIXTURES_DIR) + "/" + name);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

TEST_CASE("Holdouts: the java-sdk holdouts_context.json fixture decodes", "[holdouts][json]") {
    // Vendored byte-identical from java-sdk core-api/src/test/resources/holdouts_context.json
    // (blob 92f1dbf4): one full-shape holdout and one slim one.
    const auto data = json::parse(read_fixture("holdouts_context.json")).get<ContextData>();

    REQUIRE(data.experiments.size() == 1);
    REQUIRE(data.experiments[0].holdoutIds == std::vector<int>{11});
    REQUIRE(data.experiments[0].holdoutType.empty());
    REQUIRE(data.holdouts.size() == 2);

    const auto& full = data.holdouts[0];
    REQUIRE(full.id == 11);
    REQUIRE(full.name == "holdout_a");
    REQUIRE(full.unitType == "session_id");
    REQUIRE(full.iteration == 1);
    REQUIRE(full.seedHi == 13);
    REQUIRE(full.seedLo == 111);
    REQUIRE(full.split == std::vector<double>{0.1, 0.9});
    REQUIRE(full.holdoutType == "full");

    const auto& slim = data.holdouts[1];
    REQUIRE(slim.id == 12);
    REQUIRE(slim.name == "holdout_b");
    REQUIRE(slim.split == std::vector<double>{0.05, 0.05, 0.9});
    REQUIRE(slim.holdoutType.empty());

    Context context(config_with(UID), data);
    REQUIRE(context.treatment("exp_test_holdout") == 0);
    require_published(context, {holdout_exposure(11, "holdout_a", 0)});
}

TEST_CASE("Holdouts H5: unknown keys in holdout objects are ignored", "[holdouts][json][forward-compat]") {
    // Unknown scalar, object and array keys inside each holdout, in the experiments and at the root,
    // across two experiments: both still assign, and the context loads without an error event.
    const auto data = json::parse(read_fixture("holdouts_forward_compat_context.json")).get<ContextData>();
    REQUIRE(data.holdouts.size() == 2);
    REQUIRE(data.experiments.size() == 2);

    auto logger = std::make_shared<ExposureLog>();
    struct Errors : ContextEventLogger {
        int count = 0;
        void handle_event(Context&, const std::string& type, const json&) override {
            if (type == "error") ++count;
        }
    };
    auto errors = std::make_shared<Errors>();
    Context context(config_with(UID), data, errors);

    REQUIRE(context.treatment("exp_covered") == 0);
    REQUIRE(context.treatment("exp_uncovered") == NORMAL_VARIANT);
    REQUIRE(errors->count == 0);
    require_published(context, {holdout_exposure(11, "holdout_future", 0), holdout_exposure(12, "holdout_slim", 1),
                                own_exposure(2, "exp_uncovered", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: malformed holdout entries and ids degrade only themselves", "[holdouts][json][forward-compat]") {
    const auto data = json::parse(R"({
        "experiments": [
            {"id": 1, "name": "exp_a", "unitType": "session_id", "iteration": 1, "seedHi": 100, "seedLo": 200,
             "split": [0.5, 0.5], "trafficSeedHi": 1, "trafficSeedLo": 2, "trafficSplit": [0, 1],
             "fullOnVariant": 0, "variants": [{"name": "A", "config": null}, {"name": "B", "config": null}],
             "holdoutIds": ["11", null, 11.5, 4294967307, 11.0, {"id": 11}, true]},
            {"id": 2, "name": "exp_b", "unitType": "session_id", "iteration": 1, "seedHi": 100, "seedLo": 200,
             "split": [0.5, 0.5], "trafficSeedHi": 1, "trafficSeedLo": 2, "trafficSplit": [0, 1],
             "fullOnVariant": 0, "variants": [{"name": "A", "config": null}, {"name": "B", "config": null}],
             "holdoutIds": "11", "holdoutType": 3}
        ],
        "holdouts": [
            "not an object", 7, [], {"name": "no id"}, {"id": "11", "split": [0.1, 0.9]},
            {"id": 13, "name": "bad split", "split": [0.1, "0.9"], "seedHi": 13, "seedLo": 111},
            {"id": 11, "name": "holdout_a", "iteration": 1, "seedHi": 13, "seedLo": 111, "split": [0.1, 0.9],
             "unitType": null, "holdoutType": null}
        ]
    })").get<ContextData>();

    REQUIRE(data.experiments[0].holdoutIds == std::vector<int>{11});
    REQUIRE(data.experiments[1].holdoutIds.empty());
    REQUIRE(data.experiments[1].holdoutType.empty());
    REQUIRE(data.holdouts.size() == 2);
    REQUIRE(data.holdouts[0].id == 13);
    REQUIRE(data.holdouts[0].split.empty());
    REQUIRE(data.holdouts[1].id == 11);

    Context context(config_with(UID), data);
    REQUIRE(context.treatment("exp_a") == 0);
    REQUIRE(context.treatment("exp_b") == NORMAL_VARIANT);
}

TEST_CASE("Holdouts: a payload without holdouts serialises and publishes as before", "[holdouts][json][compat]") {
    const json experiment = {{"id", 1}, {"name", "exp_plain"}, {"unitType", UNIT_TYPE}, {"iteration", 1},
                             {"seedHi", NORMAL_SEED_HI}, {"seedLo", NORMAL_SEED_LO}, {"split", {0.5, 0.5}},
                             {"trafficSeedHi", 1}, {"trafficSeedLo", 2}, {"trafficSplit", {0, 1}},
                             {"fullOnVariant", 0}, {"variants", {{{"name", "A"}, {"config", nullptr}},
                                                                 {{"name", "B"}, {"config", R"({"k":1})"}}}}};
    std::vector<json> payloads = {{{"experiments", {experiment}}},
                                  {{"experiments", {experiment}}, {"holdouts", json::array()}},
                                  {{"experiments", {experiment}}, {"holdouts", nullptr}}};

    // to_json writes neither holdouts nor holdoutIds/holdoutType when there are none, so the
    // re-serialised data has the keys it always had.
    const json reserialised = payloads[0].get<ContextData>();
    REQUIRE_FALSE(reserialised.contains("holdouts"));
    REQUIRE_FALSE(reserialised["experiments"][0].contains("holdoutIds"));
    REQUIRE_FALSE(reserialised["experiments"][0].contains("holdoutType"));

    std::vector<std::string> bodies;
    for (const auto& payload : payloads) {
        Context context(config_with(UID), payload.get<ContextData>());
        context.set_attribute("attr", "value");
        context.treatment("exp_plain");
        context.variable_value("k", 0);
        context.track("goal", {{"amount", 1}});
        json body = context.publish();
        body["publishedAt"] = 0;
        for (auto& e : body["exposures"]) e["exposedAt"] = 0;
        for (auto& g : body["goals"]) g["achievedAt"] = 0;
        for (auto& a : body["attributes"]) a["setAt"] = 0;
        bodies.push_back(body.dump());
    }
    // Pinned byte for byte: the publish body has no new fields or events.
    REQUIRE(bodies[0] ==
            R"({"attributes":[{"name":"attr","setAt":0,"value":"value"}],)"
            R"("exposures":[{"assigned":true,"audienceMismatch":false,"custom":false,"eligible":true,)"
            R"("exposedAt":0,"fullOn":false,"id":1,"name":"exp_plain","overridden":false,)"
            R"("ruleOverride":false,"unit":"session_id","variant":1}],)"
            R"("goals":[{"achievedAt":0,"name":"goal","properties":{"amount":1}}],"hashed":true,)"
            R"("publishedAt":0,"units":[{"type":"session_id","uid":")" + hash_unit(UID) + R"("}]})");
    REQUIRE(bodies[1] == bodies[0]);
    REQUIRE(bodies[2] == bodies[0]);
}

TEST_CASE("Holdouts: model fields are appended last and default-constructed", "[holdouts][compat]") {
    // Positional aggregate initialisation written against the released models still compiles
    // and leaves the new fields empty.
    ExperimentData experiment{1, "exp", "session_id", 1, 0, 0, {0.5, 0.5}};
    REQUIRE(experiment.holdoutIds.empty());
    REQUIRE(experiment.holdoutType.empty());
    ContextData data{{experiment}};
    REQUIRE(data.holdouts.empty());
    HoldoutData holdout;
    REQUIRE(holdout.id == 0);
    REQUIRE(holdout.split.empty());
}

TEST_CASE("Holdouts H1: a refresh that only drops a duplicate holdout id keeps the cached assignment",
          "[holdouts][conformance][refresh]") {
    // Duplicates collapse at install, so [11, 11] and [11] are the same coverage.
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_duplicate_ids"), {11, 11})}));

    REQUIRE(context.treatment("exp_duplicate_ids") == 0);
    REQUIRE(context.pending() == 2);

    context.refresh(data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_duplicate_ids"), {11})}));

    REQUIRE(context.treatment("exp_duplicate_ids") == 0);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: the first suppressed candidate decides a variable's fallback", "[holdouts][variables]") {
    // javascript-sdk keeps the first suppressed candidate even if its control lacks the key.
    auto first = with_variables(covered_by(new_experiment(1, "exp_first"), {11}), "color");
    first.variants[0].config = R"({"color":"first_control"})";
    auto second = with_variables(covered_by(new_experiment(2, "exp_second"), {11}), "color");
    second.variants[0].config = R"({"color":"second_control"})";
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);

    Context context(config_with(UID), data_of({holdout}, {first, second}));
    REQUIRE(context.peek_variable_value("color", "default") == "first_control");
    REQUIRE(context.variable_value("color", "default") == "first_control");

    auto keyless = first;
    keyless.variants[0].config = R"({"other":1})";
    Context keyless_first(config_with(UID), data_of({holdout}, {keyless, second}));
    REQUIRE(keyless_first.peek_variable_value("color", "default") == "default");
    REQUIRE(keyless_first.variable_value("color", "default") == "default");
}

// --- Cost ---------------------------------------------------------------------------------------

namespace {

template <typename F>
long allocations_of(F&& f) {
    const long before = g_allocations.load();
    f();
    return g_allocations.load() - before;
}

} // namespace

TEST_CASE("Holdouts: cached reads of a held-out assignment with a custom assignment allocate nothing",
          "[holdouts][cache][cost]") {
    // A custom assignment never applies to a held-out unit, so it is not staleness: the cached
    // assignment is reused rather than rebuilt on every read.
    auto config = config_with(UID);
    config.custom_assignments["exp_holdout_custom_cache"] = 3;
    Context context(config, data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                                    {covered_by(new_experiment(1, "exp_holdout_custom_cache"), {11})}));
    const std::string name = "exp_holdout_custom_cache";

    REQUIRE(context.treatment(name) == 0);
    REQUIRE(allocations_of([&] {
                for (int i = 0; i < 10; ++i) {
                    context.treatment(name);
                    context.peek(name);
                }
            }) == 0);
}

TEST_CASE("Holdouts: an uncovered experiment resolves without holdout bookkeeping", "[holdouts][cost]") {
    // Measured against an experiment missing from the payload, which builds an empty assignment.
    // An uncovered experiment adds 2 allocations on main (1.0.0) and here, in a payload with no
    // holdouts and in one whose holdouts cover other experiments.
    const auto extra_cost = [](const ContextData& data) {
        Context context(config_with(UID), data);
        context.peek("warm_up"); // hashes the unit and creates its assigner
        const std::string plain = "exp_plain";
        const std::string missing = "exp_missing";
        return allocations_of([&] { context.peek(plain); }) - allocations_of([&] { context.peek(missing); });
    };
    const auto plain = new_experiment(1, "exp_plain");
    const auto warm_up = new_experiment(2, "warm_up");
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);

    REQUIRE(extra_cost(data_of({}, {plain, warm_up})) == 2);
    REQUIRE(extra_cost(data_of({holdout}, {plain, covered_by(warm_up, {11})})) == 2);
    // Covering it adds its holdout pins.
    REQUIRE(extra_cost(data_of({holdout}, {covered_by(plain, {11}), warm_up})) > 2);
}

TEST_CASE("Holdouts: an out-of-range or non-integral iteration keeps its default; seeds read as JS ToInt32",
          "[holdouts][json]") {
    const auto holdout = json::parse(R"({"id": 11, "iteration": 1e20, "seedHi": 1e20, "seedLo": 4294967297,
                                         "split": [0.1, 0.9]})").get<HoldoutData>();
    REQUIRE(holdout.iteration == 0);
    // javascript-sdk: (1e20 | 0) === 1661992960 and (4294967297 | 0) === 1.
    REQUIRE(holdout.seedHi == 1661992960);
    REQUIRE(holdout.seedLo == 1);

    const auto fractional = json::parse(R"({"id": 11, "seedHi": 1.5, "seedLo": -1.5})").get<HoldoutData>();
    REQUIRE(fractional.seedHi == 1);
    REQUIRE(fractional.seedLo == -1);

    // Negative values wrap rather than clamp: (-4294967297 | 0) === -1, (-2147483649 | 0) === 2147483647.
    const auto negative = json::parse(R"({"id": 11, "seedHi": -4294967297, "seedLo": -2147483649})").get<HoldoutData>();
    REQUIRE(negative.seedHi == -1);
    REQUIRE(negative.seedLo == 2147483647);

    // Beyond 2^53 JS holds the integer as a rounded double first:
    // (9007199254740995 | 0) === 4 and (18446744073709551615 | 0) === 0.
    // Below -2^53 too: (-9007199254740995 | 0) === -4.
    REQUIRE(json::parse(R"({"id": 11, "seedHi": -9007199254740995})").get<HoldoutData>().seedHi == -4);

    const auto huge = json::parse(R"({"id": 11, "seedHi": 9007199254740995, "seedLo": 18446744073709551615})")
                          .get<HoldoutData>();
    REQUIRE(huge.seedHi == 4);
    REQUIRE(huge.seedLo == 0);

    const auto wrapped = json::parse(R"({"id": 11, "seedHi": 4294967295, "seedLo": -1.0, "iteration": 2.5})")
                             .get<HoldoutData>();
    REQUIRE(wrapped.seedHi == -1);
    REQUIRE(wrapped.seedLo == -1);
    REQUIRE(wrapped.iteration == 0);
}

TEST_CASE("Holdout arms: only a 3-arm split gives arm 1 its full-on-only meaning", "[holdouts][arms]") {
    // javascript-sdk, java-sdk and the collector compare the arm count to 3 exactly: in a 4-arm
    // holdout arm 1 holds nothing out.
    auto holdout = new_holdout(21, "holdout_four_arm", 0, 1);
    holdout.split = {0.25, 0.25, 0.25, 0.25};
    Context context(config_with(UID), data_of({holdout}, {covered_by(new_experiment(1, "exp_four_arm"), {21})}));

    REQUIRE(context.treatment("exp_four_arm") == NORMAL_VARIANT);
    require_published(context, {own_exposure(1, "exp_four_arm", NORMAL_VARIANT), holdout_exposure(21, "holdout_four_arm", 1)});
}

// --- Review follow-ups ----------------------------------------------------------------------------

TEST_CASE("Holdouts: a logger that refreshes on every exposure gets the JS variable_value result",
          "[holdouts][variables][refresh]") {
    // JS walks the candidate list it started with, resolving each against the live data. Here
    // every exposure refreshes to the same data, so the walk ends on the second candidate's
    // value with one exposure each, and it stays bounded however often the logger refreshes.
    auto first = with_variables(new_experiment(1, "exp_first"), "color");
    first.split = {1.0, 0.0}; // variant 0, whose config has no "color"
    first.variants = {{"A", R"({"other":1})"}, {"B", R"({"color":"first"})"}};
    auto second = with_variables(new_experiment(2, "exp_second"), "color");
    const auto data = data_of({}, {first, second});

    int refreshes = 0;
    auto logger = std::make_shared<ExposureLog>();
    logger->on_exposure = [&](Context& context, const json&) {
        ++refreshes;
        context.refresh(data);
    };
    Context context(config_with(UID), data, logger);

    REQUIRE(context.variable_value("color", "default") == "treatment");
    REQUIRE(context.pending() == 2);
    REQUIRE(refreshes == 2);
    REQUIRE(context.variable_value("color", "default") == "treatment");
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a logger that refreshes during variable_value does not invalidate the walk",
          "[holdouts][variables][refresh]") {
    // The first candidate is suppressed; its holdout exposure's logger refreshes with data that
    // drops the variable from it, replacing the index variable_value is walking.
    auto suppressed = with_variables(covered_by(new_experiment(1, "exp_first_suppressed"), {11}), "color");
    auto assigned = with_variables(new_experiment(2, "exp_second_assigned"), "color");
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);
    auto without_first = suppressed;
    without_first.variants = {{"A", nullptr}, {"B", nullptr}};
    const auto refreshed = data_of({holdout}, {without_first, assigned});

    auto logger = std::make_shared<ExposureLog>();
    logger->on_exposure = [&](Context& context, const json& data) {
        if (data["id"] == 11) context.refresh(refreshed);
    };
    Context context(config_with(UID), data_of({holdout}, {suppressed, assigned}), logger);

    REQUIRE(context.variable_value("color", "default") == "treatment");
    require_published(context, {holdout_exposure(11, "holdout_a", 0),
                                own_exposure(2, "exp_second_assigned", NORMAL_VARIANT)});
}

TEST_CASE("Holdouts: an uncovered override is kept across a unit type change, as in JS", "[holdouts][refresh]") {
    ContextConfig config;
    config.units = {{UNIT_TYPE, UID}, {"user_id", UID_NOT_HELD_OUT}};
    config.overrides["exp_override_plain"] = 1;
    Context context(config, data_of({}, {new_experiment(1, "exp_override_plain")}));

    REQUIRE(context.treatment("exp_override_plain") == 1);
    context.refresh(data_of({}, {new_experiment(1, "exp_override_plain", "user_id", 0)}));
    REQUIRE(context.treatment("exp_override_plain") == 1);
    REQUIRE(context.pending() == 1);
}

TEST_CASE("Holdouts: a covered override is kept across a unit type change, as in JS", "[holdouts][refresh]") {
    // javascript-sdk and java-sdk revalidate an override by its variant and holdout set only.
    ContextConfig config;
    config.units = {{UNIT_TYPE, UID}, {"user_id", UID_NOT_HELD_OUT}};
    config.overrides["exp_override_unit_type"] = 1;
    const auto holdout = new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO);
    Context context(config, data_of({holdout}, {covered_by(new_experiment(1, "exp_override_unit_type"), {11})}));

    REQUIRE(context.treatment("exp_override_unit_type") == 1);
    REQUIRE(context.pending() == 2);

    context.refresh(data_of({holdout}, {covered_by(new_experiment(1, "exp_override_unit_type", "user_id", 0), {11})}));
    REQUIRE(context.treatment("exp_override_unit_type") == 1);
    REQUIRE(context.pending() == 2);
}

TEST_CASE("Holdouts: a refresh during variable_value does not re-walk the candidate that triggered it",
          "[holdouts][variables][refresh]") {
    // The first candidate's exposure refreshes with a new iteration of that same experiment. JS
    // goes on with the next candidate and does not expose the first one again.
    const auto data_for = [](int iteration) {
        auto first = with_variables(new_experiment(1, "exp_first"), "color");
        first.iteration = iteration;
        first.split = {1.0, 0.0};
        first.variants = {{"A", R"({"other":1})"}, {"B", R"({"color":"first"})"}};
        return data_of({}, {first, with_variables(new_experiment(2, "exp_second"), "color")});
    };
    bool refreshed = false;
    auto logger = std::make_shared<ExposureLog>();
    logger->on_exposure = [&](Context& context, const json&) {
        if (!refreshed) {
            refreshed = true;
            context.refresh(data_for(2));
        }
    };
    Context context(config_with(UID), data_for(1), logger);

    REQUIRE(context.variable_value("color", "default") == "treatment");
    REQUIRE(context.pending() == 2);
    std::vector<std::string> names;
    for (const auto& exposure : logger->exposures) names.push_back(exposure["name"]);
    REQUIRE(names == std::vector<std::string>{"exp_first", "exp_second"});
}

TEST_CASE("Holdouts: decoding into reused models replaces holdouts and holdoutIds", "[holdouts][json]") {
    ContextData data;
    json::parse(R"({"experiments":[],"holdouts":[{"id":11,"split":[0.1,0.9]}]})").get_to(data);
    REQUIRE(data.holdouts.size() == 1);
    json::parse(R"({"experiments":[],"holdouts":[]})").get_to(data);
    REQUIRE(data.holdouts.empty());
    json::parse(R"({"experiments":[],"holdouts":[{"id":11,"split":[0.1,0.9]}]})").get_to(data);
    json::parse(R"({"experiments":[]})").get_to(data);
    REQUIRE(data.holdouts.empty());

    ExperimentData experiment;
    const auto with_ids = json::parse(R"({"id":1,"name":"e","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,
                                          "holdoutIds":[11]})");
    auto without_ids = with_ids;
    without_ids["holdoutIds"] = json::array();
    with_ids.get_to(experiment);
    REQUIRE(experiment.holdoutIds == std::vector<int>{11});
    without_ids.get_to(experiment);
    REQUIRE(experiment.holdoutIds.empty());

    // Omitted, null or not an array: also no coverage, as with ContextData::holdouts.
    for (const char* ids : {"absent", "null", "\"11\""}) {
        auto payload = with_ids;
        payload["holdoutType"] = "full";
        payload.get_to(experiment);
        REQUIRE(experiment.holdoutIds == std::vector<int>{11});
        REQUIRE(experiment.holdoutType == "full");
        auto next = with_ids;
        if (std::string(ids) == "absent") next.erase("holdoutIds"); else next["holdoutIds"] = json::parse(ids);
        next.get_to(experiment);
        REQUIRE(experiment.holdoutIds.empty());
        REQUIRE(experiment.holdoutType.empty());
    }
}

TEST_CASE("Holdouts: integer and float spellings of a large seed read the same", "[holdouts][json]") {
    // (9007199254740993 | 0) === 0 in JS: the integer rounds to 9007199254740992 first.
    const auto integer = json::parse(R"({"id": 11, "seedHi": 9007199254740993})").get<HoldoutData>();
    const auto floating = json::parse(R"({"id": 11, "seedHi": 9007199254740993.0})").get<HoldoutData>();
    REQUIRE(integer.seedHi == 0);
    REQUIRE(floating.seedHi == 0);
}

TEST_CASE("Holdouts: cached variable reads do not capture candidate names", "[holdouts][cost]") {
    auto first = new_experiment(1, "first_experiment_with_a_long_name");
    first.split = {1.0, 0.0};
    first.variants = {{"A", R"({"other":1})"}, {"B", R"({"number":1})"}};
    auto second = new_experiment(2, "second_experiment_with_a_long_name");
    second.variants = {{"A", R"({"number":0})"}, {"B", R"({"number":42})"}};
    Context context(config_with(UID), data_of({}, {first, second}));
    const std::string key = "number";
    const json fallback = 0;
    REQUIRE(context.variable_value(key, fallback) == 42);
    REQUIRE(allocations_of([&] {
        for (int i = 0; i < 10; ++i) context.variable_value(key, fallback);
    }) == 0);
}

// --- A logger that finalizes from an exposure ---------------------------------------------------

namespace {

struct FinalizingLogger : ContextEventLogger {
    bool finalized = false;
    std::vector<PublishEvent> flushed;
    std::vector<std::string> events;
    void handle_event(Context& context, const std::string& type, const json& data) override {
        events.push_back(type == "exposure" ? "exposure:" + data["name"].get<std::string>() : type);
        if (type == "exposure" && !finalized) {
            finalized = true;
            flushed.push_back(context.finalize());
        }
    }
};

} // namespace

TEST_CASE("Holdouts: a logger that finalizes on the first exposure still flushes the holdout exposures",
          "[holdouts][logger][finalize]") {
    // treatment() queues the experiment's own exposure and its holdouts' exposures before any
    // callback runs, so finalize() from the first callback publishes all of them.
    auto logger = std::make_shared<FinalizingLogger>();
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_finalize"), {11, 12})}),
                    logger);

    REQUIRE(context.treatment("exp_finalize") == 0);
    REQUIRE(context.is_finalized());
    REQUIRE(context.pending() == 0);
    REQUIRE(logger->flushed.size() == 1);
    std::vector<std::string> names;
    for (const auto& exposure : logger->flushed[0].exposures) names.push_back(exposure.name);
    REQUIRE(names == std::vector<std::string>{"exp_finalize", "holdout_a", "holdout_b"});
}

TEST_CASE("Holdouts: a held-out unit's holdout exposures all flush when a logger finalizes",
          "[holdouts][logger][finalize]") {
    // Suppressed: no own exposure, so the first callback is the first holdout's.
    auto logger = std::make_shared<FinalizingLogger>();
    Context context(config_with(UID),
                    data_of({new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO),
                             new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_finalize"), {11, 12})}),
                    logger);

    REQUIRE(context.treatment("exp_finalize") == 0);
    REQUIRE(context.pending() == 0);
    REQUIRE(logger->flushed.at(0).exposures.size() == 2);
}

TEST_CASE("Holdouts: variable_value after a logger finalizes behaves as in JS and 1.0.0", "[holdouts][logger][finalize]") {
    // The first candidate's exposure finalizes the context. As in JS (and 1.0.0) the walk goes
    // on: the second candidate wins the key and its exposure is queued (and logged) after the
    // finalize, so it stays pending. Only the first candidate was published.
    auto first = with_variables(new_experiment(1, "exp_first"), "color");
    first.split = {1.0, 0.0};
    first.variants = {{"A", R"({"other":1})"}, {"B", R"({"color":"first"})"}};
    auto logger = std::make_shared<FinalizingLogger>();
    Context context(config_with(UID), data_of({}, {first, with_variables(new_experiment(2, "exp_second"), "color")}),
                    logger);

    REQUIRE(context.variable_value("color", "default") == "treatment");
    REQUIRE(context.is_finalized());
    REQUIRE(context.pending() == 1);
    REQUIRE(logger->flushed.at(0).exposures.size() == 1);
    REQUIRE(logger->events ==
            std::vector<std::string>{"ready", "exposure:exp_first", "publish", "finalize", "exposure:exp_second"});
}

TEST_CASE("Holdouts: a covered call logs its own exposure, then its holdouts in id order", "[holdouts][logger]") {
    // All exposures are queued first, then delivered to the logger in queue order.
    auto logger = std::make_shared<ExposureLog>();
    Context context(config_with(UID_NOT_HELD_OUT),
                    data_of({new_holdout(12, "holdout_b", HOLDOUT_B_SEED_HI, HOLDOUT_B_SEED_LO),
                             new_holdout(11, "holdout_a", HOLDOUT_A_SEED_HI, HOLDOUT_A_SEED_LO)},
                            {covered_by(new_experiment(1, "exp_order"), {12, 11})}),
                    logger);

    std::vector<size_t> pending_at_callback;
    logger->on_exposure = [&](Context& c, const json&) { pending_at_callback.push_back(static_cast<size_t>(c.pending())); };
    context.treatment("exp_order");

    std::vector<std::string> names;
    for (const auto& exposure : logger->exposures) names.push_back(exposure["name"]);
    REQUIRE(names == std::vector<std::string>{"exp_order", "holdout_a", "holdout_b"});
    REQUIRE(pending_at_callback == std::vector<size_t>{3, 3, 3});
}

// Measured with this exact fixture and logger against main (1.0.0): 69. The test binary's own
// operator new adds nothing per call, so the count is the SDK's.
constexpr long UNCOVERED_FIRST_TREATMENT_ALLOCATIONS = 69;

TEST_CASE("Holdouts: an uncovered first exposure allocates as on 1.0.0", "[holdouts][cost]") {
    // Measured against main (1.0.0) with the same logger: the first treatment() of an
    // uncovered experiment costs the same, so the uncovered path takes no batch.
    struct CountingLogger : ContextEventLogger {
        int exposures = 0;
        void handle_event(Context&, const std::string& type, const json&) override {
            if (type == "exposure") ++exposures;
        }
    };
    auto logger = std::make_shared<CountingLogger>();
    Context context(config_with(UID), data_of({}, {new_experiment(1, "exp_plain"), new_experiment(2, "warm_up")}),
                    logger);
    context.treatment("warm_up"); // hashes the unit and creates its assigner
    const std::string name = "exp_plain";
    REQUIRE(allocations_of([&] { context.treatment(name); }) == UNCOVERED_FIRST_TREATMENT_ALLOCATIONS);
    REQUIRE(logger->exposures == 2);
}

TEST_CASE("Switchback cost: a cached read revalidates once after an unrelated attribute, then allocates nothing",
          "[switchback][cost]") {
    // The slot records the attribute sequence its rules were checked against, so later reads skip
    // the rule check until another attribute is set.
    ExperimentData experiment = new_experiment(1, "exp_sb");
    experiment.randomizationType = "switchback";
    experiment.switchbackClusters = nlohmann::ordered_json{{UNIT_TYPE, "city"}};
    experiment.switchbackStartAt = 0;
    experiment.switchbackPeriodMs = int64_t{1} << 50;
    // A split rule's action allocates its percentages, so a re-evaluation shows as an allocation.
    experiment.assignmentRules = R"({"rules":[{"type":"split","environments":[],"percentages":"50/50"}]})";
    Context context(config_with(UID), data_of({}, {experiment}));
    context.set_attribute("city", "Lisboa");
    const int variant = context.treatment("exp_sb");
    context.set_attribute("unrelated", 1);
    REQUIRE(context.treatment("exp_sb") == variant);
    for (int i = 0; i < 3; ++i) {
        int read = -1;
        REQUIRE(allocations_of([&] { read = context.treatment("exp_sb"); }) == 0);
        REQUIRE(read == variant);
    }
}
