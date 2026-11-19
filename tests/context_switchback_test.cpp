// Port of java-sdk ContextSwitchbackTest (core-api/src/test/java/com/absmartly/sdk/
// ContextSwitchbackTest.java at 07807b5) through the C++ Context. Same units, seeds, anchors and
// expected schedules. The Java tests that need an injectable AudienceMatcher or threads have no C++
// counterpart (a Context is single-threaded and its matcher is fixed); their behaviour is pinned
// through the public API below where it is observable.
#include "switchback_support.h"

#include <algorithm>

using namespace sbtest;

namespace {

const char* UNIT_TYPE = "store_id";
const char* UID = "e791e240fcd3df7d238cfc285f475e8152fcc0ec";
// hash_unit(UID): the MD5/base64url string the Collector's UnitHasher also produces.
const char* UID_HASH = "pAE3a1i5Drs5mKRNq56adA";

const int32_t SEED_HI = -5;
const int32_t SEED_LO = -7;
const int64_t SEED = seed_of(SEED_HI, SEED_LO);

// The store unit's cluster value: attribute CLUSTER_PATH, set on every default context.
const char* CLUSTER_PATH = "store_key";
const char* CLUSTER_KEY = "store-42";
// Periods 0..11 of the raw key "store-42" under (SEED_HI, SEED_LO), from an independent Python
// reference (murmur3 over the platform formulas) that reproduces every golden vector.
const std::vector<int> EXPECTED_2_ARMS = {0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1};
const std::vector<int> EXPECTED_3_ARMS = {0, 2, 1, 2, 1, 0, 1, 2, 0, 1, 0, 2};
const std::vector<int> EXPECTED_10_ARMS = {8, 3, 7, 4, 2, 0, 5, 1, 9, 6, 3, 7};
// The same reference fed UID_HASH and the raw UID: what keying by the unit would serve.
const std::vector<int> UID_HASH_3_ARMS = {1, 2, 0, 2, 1, 0, 2, 0, 1, 1, 2, 0};
const std::vector<int> UID_3_ARMS = {2, 0, 1, 0, 1, 2, 1, 2, 0, 0, 1, 2};

const char* STRICT_GOLD_AUDIENCE = R"({"filter":[{"eq":[{"var":"tier"},{"value":"gold"}]}]})";
const char* ASSIGN_VARIANT_1 = R"({"rules":[{"type":"assign","environments":[],"variant":1}]})";
const char* ASSIGN_VARIANT_2 = R"({"rules":[{"type":"assign","environments":[],"variant":2}]})";

ExperimentData switchback(int id, const std::string& name, int arms, int64_t period_ms) {
    ExperimentData e;
    e.id = id;
    e.name = name;
    e.unitType = UNIT_TYPE;
    e.iteration = 1;
    e.seedHi = SEED_HI;
    e.seedLo = SEED_LO;
    for (int i = 0; i < arms; ++i) {
        e.split.push_back(1.0 / arms);
        e.variants.push_back({std::string(1, static_cast<char>('A' + i)), "{\"sb.arm\":" + std::to_string(i) + "}"});
    }
    e.trafficSeedHi = 1;
    e.trafficSeedLo = 2;
    e.trafficSplit = {0.0, 1.0};
    e.application = "website";
    e.randomizationType = "switchback";
    e.switchbackClusters = clusters({{UNIT_TYPE, CLUSTER_PATH}});
    e.switchbackStartAt = ANCHOR;
    e.switchbackPeriodMs = period_ms;
    e.switchbackCarryoverDurationMs = 0;
    e.switchbackBurnInDurationMs = 0;
    e.switchbackBurnOutDurationMs = 0;
    e.switchbackDesignType = "balanced_block";
    e.switchbackEstimand = "global_average_treatment_effect";
    return e;
}

ExperimentData switchback(int arms, int64_t period_ms) { return switchback(1, "exp_sb", arms, period_ms); }

ExperimentData as_unit(ExperimentData e) {
    e.randomizationType.clear();
    e.switchbackClusters = nullptr;
    e.switchbackStartAt.reset();
    e.switchbackPeriodMs.reset();
    return e;
}

// Holdout 11 for UID under seed (13, 111); split decides whether UID is held out.
HoldoutData holdout(std::vector<double> split) {
    HoldoutData h;
    h.id = 11;
    h.name = "holdout_a";
    h.unitType = UNIT_TYPE;
    h.iteration = 1;
    h.seedHi = 13;
    h.seedLo = 111;
    h.split = std::move(split);
    return h;
}

// A context with the store unit and the cluster attribute set at the current clock reading.
std::unique_ptr<Context> create(ContextData data, ContextConfig config = units({{UNIT_TYPE, UID}}),
                                std::shared_ptr<ContextEventLogger> logger = nullptr) {
    auto context = std::make_unique<Context>(config, std::move(data), std::move(logger));
    context->set_attribute(CLUSTER_PATH, CLUSTER_KEY);
    return context;
}

json store_exposure(int id, const std::string& name, int variant, int64_t at, Flags f = {}) {
    return exposure(id, name, UNIT_TYPE, variant, at, f);
}

std::vector<int> peek_schedule(Context& context, const std::string& name, int64_t period_ms, int periods) {
    std::vector<int> result;
    for (int i = 0; i < periods; ++i) {
        TestClock::now = ANCHOR + i * period_ms;
        result.push_back(context.peek(name));
    }
    return result;
}

const Flags RULE{false, true, false, false, false, false, true};
const Flags STRICT_MISMATCH{false, true, false, false, false, true, false};
const Flags OVERRIDDEN{false, true, true, false, false, false, false};

}

TEST_CASE("Switchback: the cluster identity is the raw cluster value", "[switchback][context]") {
    ClockScope clock;
    REQUIRE(hash_unit(UID) == UID_HASH);
    auto context = create(data_of({switchback(3, HOUR)}));
    const auto served = peek_schedule(*context, "exp_sb", HOUR, 12);

    REQUIRE(served == EXPECTED_3_ARMS);
    REQUIRE(served != UID_HASH_3_ARMS);
    REQUIRE(served != UID_3_ARMS);
    bool hashed_differs = false;
    for (int i = 0; i < 12; ++i) {
        REQUIRE(assign_switchback(CLUSTER_KEY, i, SEED, 3) == served[static_cast<size_t>(i)]);
        hashed_differs |= assign_switchback(hash_unit(CLUSTER_KEY), i, SEED, 3) != served[static_cast<size_t>(i)];
    }
    REQUIRE(hashed_differs);
}

TEST_CASE("Switchback: serves two, three and ten arm permutations", "[switchback][context]") {
    ClockScope clock;
    auto context = create(data_of({switchback(1, "exp_2", 2, HOUR), switchback(2, "exp_3", 3, HOUR),
                                   switchback(3, "exp_10", 10, HOUR)}));
    REQUIRE(peek_schedule(*context, "exp_2", HOUR, 12) == EXPECTED_2_ARMS);
    REQUIRE(peek_schedule(*context, "exp_3", HOUR, 12) == EXPECTED_3_ARMS);
    REQUIRE(peek_schedule(*context, "exp_10", HOUR, 12) == EXPECTED_10_ARMS);
}

TEST_CASE("Switchback: one hour boundary at the exact edge milliseconds", "[switchback][context][period]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    const std::vector<std::pair<int64_t, int>> edges = {
        {ANCHOR - 1, 0}, {ANCHOR, EXPECTED_2_ARMS[0]}, {ANCHOR + HOUR - 1, EXPECTED_2_ARMS[0]},
        {ANCHOR + HOUR, EXPECTED_2_ARMS[1]}, {ANCHOR + 3 * HOUR - 1, EXPECTED_2_ARMS[2]},
        {ANCHOR + 3 * HOUR, EXPECTED_2_ARMS[3]}};
    for (const auto& [now, variant] : edges) {
        TestClock::now = now;
        INFO(now - ANCHOR);
        REQUIRE(context->peek("exp_sb") == variant);
    }
}

TEST_CASE("Switchback: twenty-four hour boundary at the exact edge milliseconds", "[switchback][context][period]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, DAY)}));
    const std::vector<std::pair<int64_t, int>> edges = {
        {ANCHOR - 1, 0}, {ANCHOR, EXPECTED_2_ARMS[0]}, {ANCHOR + HOUR, EXPECTED_2_ARMS[0]},
        {ANCHOR + DAY - 1, EXPECTED_2_ARMS[0]}, {ANCHOR + DAY, EXPECTED_2_ARMS[1]},
        {ANCHOR + 4 * DAY - 1, EXPECTED_2_ARMS[3]}, {ANCHOR + 4 * DAY, EXPECTED_2_ARMS[4]}};
    for (const auto& [now, variant] : edges) {
        TestClock::now = now;
        INFO(now - ANCHOR);
        REQUIRE(context->peek("exp_sb") == variant);
    }
}

TEST_CASE("Switchback: an unaligned start is unservable and before the anchor is ineligible",
          "[switchback][context][period]") {
    ClockScope clock;
    auto aligned = switchback(1, "exp_aligned", 2, HOUR);
    aligned.switchbackStartAt = ANCHOR + HOUR;
    auto unaligned = switchback(2, "exp_unaligned", 2, HOUR);
    const int64_t start = ANCHOR + 1234567;
    unaligned.switchbackStartAt = start;
    auto context = create(data_of({aligned, unaligned}));

    for (int64_t now : {ANCHOR, start - 1, start, ANCHOR + HOUR - 1}) {
        TestClock::now = now;
        REQUIRE(context->treatment("exp_aligned") == 0);
        REQUIRE(context->treatment("exp_unaligned") == 0);
    }
    REQUIRE(context->pending() == 0);
    for (int i = 1; i < 4; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_aligned") == EXPECTED_2_ARMS[static_cast<size_t>(i - 1)]);
        REQUIRE(context->treatment("exp_unaligned") == 0);
        TestClock::now = start + i * HOUR;
        REQUIRE(context->treatment("exp_unaligned") == 0);
    }
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_aligned", EXPECTED_2_ARMS[0], ANCHOR + HOUR),
                                               store_exposure(1, "exp_aligned", EXPECTED_2_ARMS[1], ANCHOR + 2 * HOUR),
                                               store_exposure(1, "exp_aligned", EXPECTED_2_ARMS[2], ANCHOR + 3 * HOUR)}));
}

TEST_CASE("Switchback: an anchor off the hour or out of range is unservable", "[switchback][context][period]") {
    ClockScope clock;
    const int64_t max_anchor = 253402300800000;
    for (int64_t start_at : {ANCHOR + 1, ANCHOR - 1, ANCHOR + 30 * 60000, ANCHOR + HOUR - 1, -HOUR, int64_t{-1},
                             max_anchor, max_anchor + HOUR, INT64_MAX - (INT64_MAX % HOUR)}) {
        INFO("startAt " << start_at);
        auto experiment = switchback(2, HOUR);
        experiment.switchbackStartAt = start_at;
        TestClock::now = 0;
        auto context = create(data_of({experiment}));
        // start_at + HOUR saturates: INT64_MAX - (INT64_MAX % HOUR) + HOUR would overflow.
        const int64_t after = start_at > INT64_MAX - HOUR ? INT64_MAX : start_at + HOUR;
        for (int64_t now : {int64_t{0}, ANCHOR, ANCHOR + 2 * HOUR + 1, start_at, after, max_anchor + DAY}) {
            TestClock::now = now;
            REQUIRE(context->treatment("exp_sb") == 0);
            REQUIRE(context->variable_value("sb.arm", -1) == 0);
        }
        REQUIRE(context->pending() == 0);
    }
    // In-range hour-aligned anchors, including 0 and the last hour before the bound, serve.
    for (int64_t start_at : {int64_t{0}, ANCHOR, max_anchor - HOUR}) {
        auto experiment = switchback(2, HOUR);
        experiment.switchbackStartAt = start_at;
        TestClock::now = start_at + HOUR;
        auto context = create(data_of({experiment}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[1]);
        REQUIRE(context->pending() == 1);
    }
}

TEST_CASE("Switchback: a null start is ineligible and never falls back to unit assignment",
          "[switchback][context][period]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.switchbackStartAt.reset();
    auto context = create(data_of({experiment}));
    for (int i = 0; i < 4; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == 0);
        // Variant 0's own config, like any ineligible assignment.
        REQUIRE(context->variable_value("sb.arm", -1) == 0);
    }
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback: the same period is cached and the next one rolls over with one exposure each",
          "[switchback][context][exposure]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    TestClock::now = ANCHOR + 10;
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);
    TestClock::now = ANCHOR + HOUR - 1;
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->peek("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);

    TestClock::now = ANCHOR + HOUR;
    REQUIRE(context->peek("exp_sb") == EXPECTED_2_ARMS[1]);
    REQUIRE(context->pending() == 1);
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[1]);
    REQUIRE(context->pending() == 2);
    TestClock::now = ANCHOR + HOUR + 30 * 60000;
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[1]);
    REQUIRE(context->pending() == 2);

    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + 10),
                                               store_exposure(1, "exp_sb", EXPECTED_2_ARMS[1], ANCHOR + HOUR)}));
}

TEST_CASE("Switchback: variable lookups follow the period and expose once per period", "[switchback][context][variables]") {
    ClockScope clock;
    auto context = create(data_of({switchback(3, HOUR)}));
    REQUIRE(context->variable_keys() == std::map<std::string, std::vector<std::string>>{{"sb.arm", {"exp_sb"}}});
    REQUIRE(context->experiments() == std::vector<std::string>{"exp_sb"});
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        const int expected = EXPECTED_3_ARMS[static_cast<size_t>(i)];
        REQUIRE(context->peek_variable_value("sb.arm", -1) == expected);
        REQUIRE(context->variable_value("sb.arm", -1) == expected);
        REQUIRE(context->variable_value("sb.arm", -1) == expected);
        REQUIRE(context->treatment("exp_sb") == expected);
    }
    REQUIRE(context->pending() == 3);
}

TEST_CASE("Switchback: no linked unit is unassigned and never falls back", "[switchback][context]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.unitType = "user_id";
    auto context = create(data_of({experiment}), units({{"user_id", UID}}));
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->peek("exp_sb") == 0);
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(context->variable_value("sb.arm", -1) == -1);
    }
    REQUIRE(context->pending() == 0);

    context->set_unit(UNIT_TYPE, UID);
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[2]);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[2], ANCHOR + 2 * HOUR)}));
}

TEST_CASE("Switchback: full-on serves the full-on variant in every period", "[switchback][context][precedence]") {
    ClockScope clock;
    auto experiment = switchback(3, HOUR);
    experiment.fullOnVariant = 2;
    auto context = create(data_of({experiment}));
    json expected = json::array();
    for (int i = 0; i < 4; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == 2);
        expected.push_back(store_exposure(1, "exp_sb", 2, ANCHOR + i * HOUR, {true, true, false, true}));
    }
    REQUIRE(published(*context) == expected);
}

TEST_CASE("Switchback: override and custom assignment win in every period", "[switchback][context][precedence]") {
    ClockScope clock;
    auto context = create(data_of({switchback(1, "exp_override", 3, HOUR), switchback(2, "exp_custom", 3, HOUR)}));
    context->set_override("exp_override", 7);
    context->set_custom_assignment("exp_custom", 2);
    json expected = json::array();
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_override") == 7);
        REQUIRE(context->treatment("exp_custom") == 2);
        expected.push_back(store_exposure(1, "exp_override", 7, TestClock::now, OVERRIDDEN));
        expected.push_back(store_exposure(2, "exp_custom", 2, TestClock::now, {true, true, false, false, true}));
    }
    REQUIRE(published(*context) == expected);
}

TEST_CASE("Switchback: traffic eligibility is sticky on the serving unit", "[switchback][context][traffic]") {
    ClockScope clock;
    int eligible_count = 0;
    int ineligible_count = 0;
    for (int u = 0; u < 24; ++u) {
        auto sb = switchback(1, "exp_sb", 2, HOUR);
        sb.trafficSplit = {0.5, 0.5};
        auto twin = switchback(2, "exp_unit", 2, HOUR);
        twin.trafficSplit = {0.5, 0.5};
        twin.randomizationType.clear();
        twin.switchbackStartAt.reset();

        TestClock::now = ANCHOR;
        auto context = create(data_of({sb, twin}), units({{UNIT_TYPE, "store_" + std::to_string(u)}}));
        context->treatment("exp_unit");
        for (int i = 0; i < 4; ++i) {
            TestClock::now = ANCHOR + i * HOUR;
            context->treatment("exp_sb");
        }
        const auto exposures = context->publish().exposures;
        REQUIRE(exposures.size() == 5);
        const bool twin_eligible = exposures[0].eligible;
        for (size_t i = 1; i < exposures.size(); ++i) {
            REQUIRE(exposures[i].name == "exp_sb");
            REQUIRE(exposures[i].eligible == twin_eligible);
            if (!twin_eligible) REQUIRE(exposures[i].variant == 0);
        }
        (twin_eligible ? eligible_count : ineligible_count)++;
    }
    REQUIRE(eligible_count > 0);
    REQUIRE(ineligible_count > 0);
}

TEST_CASE("Switchback: unit experiments are unaffected", "[switchback][context]") {
    ClockScope clock;
    auto unit = switchback(2, HOUR);
    unit.randomizationType = "unit";
    auto absent = switchback(2, "exp_absent", 2, HOUR);
    absent.randomizationType.clear();
    auto context = create(data_of({unit, absent}));
    const int variant = context->treatment("exp_sb");
    const int absent_variant = context->treatment("exp_absent");
    for (int i = 1; i < 6; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == variant);
        REQUIRE(context->treatment("exp_absent") == absent_variant);
    }
    REQUIRE(context->pending() == 2);
}

TEST_CASE("Switchback: refresh invalidates on grid fields but not on analysis-only fields", "[switchback][context][refresh]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    TestClock::now = ANCHOR + 10;
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);

    auto burns = switchback(2, HOUR);
    burns.switchbackBurnInDurationMs = 600000;
    burns.switchbackBurnOutDurationMs = 300000;
    burns.switchbackCarryoverDurationMs = 900000;
    burns.switchbackEstimand = "other";
    context->refresh(data_of({burns}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);

    // Same period index 0 under both grids, so only the identity comparison can invalidate.
    context->refresh(data_of({switchback(2, 2 * HOUR)}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 2);

    auto design = switchback(2, 2 * HOUR);
    design.switchbackDesignType = "other_design";
    context->refresh(data_of({design}));
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 3);

    auto to_unit = switchback(2, 2 * HOUR);
    to_unit.randomizationType = "unit";
    context->refresh(data_of({to_unit}));
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 4);

    context->refresh(data_of({switchback(2, 2 * HOUR)}));
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 5);

    // The cluster map (types, paths and their order) and the allow-list are identity.
    auto path = switchback(2, 2 * HOUR);
    path.switchbackClusters = clusters({{UNIT_TYPE, "'store_key'"}});
    context->refresh(data_of({path}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 6);

    auto added = switchback(2, 2 * HOUR);
    added.switchbackClusters = clusters({{UNIT_TYPE, "'store_key'"}, {"device_id", "city"}});
    context->refresh(data_of({added}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 7);

    auto reordered = switchback(2, 2 * HOUR);
    reordered.switchbackClusters = clusters({{"device_id", "city"}, {UNIT_TYPE, "'store_key'"}});
    context->refresh(data_of({reordered}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 8);

    auto values = reordered;
    values.switchbackClusterValues = std::vector<std::string>{CLUSTER_KEY};
    context->refresh(data_of({values}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 9);

    // An unrelated unitType edit is not identity: switchbacks never read it.
    auto unit_type = values;
    unit_type.unitType = "anything";
    context->refresh(data_of({unit_type}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 9);
}

TEST_CASE("Switchback: a refresh that sets the start starts the grid", "[switchback][context][refresh]") {
    ClockScope clock;
    auto pending = switchback(2, HOUR);
    pending.switchbackStartAt.reset();
    auto context = create(data_of({pending}));
    TestClock::now = ANCHOR + 10;
    REQUIRE(context->treatment("exp_sb") == 0);
    context->refresh(data_of({switchback(2, HOUR)}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);
}

TEST_CASE("Switchback: holdouts never apply to a switchback", "[switchback][context][holdouts]") {
    ClockScope clock;
    // split [0.1, 0.9] holds UID out (arm 0).
    auto covered = switchback(2, HOUR);
    covered.holdoutIds = {11};
    auto context = create(data_of({covered}, {holdout({0.1, 0.9})}));
    json expected = json::array();
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[static_cast<size_t>(i)]);
        REQUIRE(context->variable_value("sb.arm", -1) == EXPECTED_2_ARMS[static_cast<size_t>(i)]);
        expected.push_back(store_exposure(1, "exp_sb", EXPECTED_2_ARMS[static_cast<size_t>(i)], TestClock::now));
    }
    REQUIRE(published(*context) == expected);
}

TEST_CASE("Switchback: an assignment rule overrides the schedule, before the anchor too", "[switchback][context][rules]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.assignmentRules = ASSIGN_VARIANT_1;
    for (int u = 0; u < 10; ++u) {
        TestClock::now = ANCHOR - 1;
        auto context = create(data_of({experiment}), units({{UNIT_TYPE, "user-" + std::to_string(u)}}));
        REQUIRE(context->treatment("exp_sb") == 1);
        REQUIRE(context->variable_value("sb.arm", -1) == 1);
        json expected = json::array({store_exposure(1, "exp_sb", 1, ANCHOR - 1, RULE)});
        for (int i = 0; i < 4; ++i) {
            TestClock::now = ANCHOR + i * HOUR;
            REQUIRE(context->treatment("exp_sb") == 1);
            expected.push_back(store_exposure(1, "exp_sb", 1, TestClock::now, RULE));
        }
        REQUIRE(published(*context) == expected);
    }
}

TEST_CASE("Switchback: an unservable schedule only replaces the draw, not full-on or rules", "[switchback][context][rules]") {
    ClockScope clock;
    auto full_on = switchback(1, "sb_full_on", 2, HOUR);
    full_on.switchbackStartAt.reset();
    full_on.fullOnVariant = 1;
    auto rules = switchback(2, "sb_rules", 2, HOUR);
    rules.switchbackPeriodMs = 0;
    rules.assignmentRules = ASSIGN_VARIANT_1;
    auto unaligned = switchback(3, "sb_unaligned_rules", 2, HOUR);
    unaligned.switchbackStartAt = ANCHOR + 1;
    unaligned.assignmentRules = ASSIGN_VARIANT_1;
    auto context = create(data_of({full_on, rules, unaligned}), units({{UNIT_TYPE, "15"}}));
    REQUIRE(context->treatment("sb_full_on") == 1);
    REQUIRE(context->treatment("sb_rules") == 1);
    REQUIRE(context->treatment("sb_unaligned_rules") == 1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "sb_full_on", 1, ANCHOR, {true, true, false, true}),
                                               store_exposure(2, "sb_rules", 1, ANCHOR, RULE),
                                               store_exposure(3, "sb_unaligned_rules", 1, ANCHOR, RULE)}));
}

TEST_CASE("Switchback: assignment rules do not apply to an ineligible cluster", "[switchback][context][rules]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.assignmentRules = ASSIGN_VARIANT_1;
    experiment.switchbackClusterValues = std::vector<std::string>{"other-store"};
    auto context = create(data_of({experiment}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback: a split rule randomises over the serving unit", "[switchback][context][rules]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.unitType = "user_id";
    experiment.assignmentRules = R"({"rules":[{"type":"split","environments":[],"percentages":"50/50"}]})";
    int ones = 0;
    for (int u = 0; u < 32; ++u) {
        const std::string uid = "store-" + std::to_string(u);
        const int expected = VariantAssigner(hash_unit(uid)).assign({0.5, 0.5}, SEED_HI, SEED_LO);
        auto context = create(data_of({experiment}), units({{UNIT_TYPE, uid}, {"user_id", "fixed-user"}}));
        REQUIRE(context->treatment("exp_sb") == expected);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", expected, ANCHOR, RULE)}));
        ones += expected;
    }
    REQUIRE((ones > 0 && ones < 32));
}

TEST_CASE("Switchback: a strict audience mismatch is recorded as control", "[switchback][context][audience]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.audience = STRICT_GOLD_AUDIENCE;
    experiment.audienceStrict = true;
    SECTION("in a period") {
        auto context = create(data_of({experiment}));
        TestClock::now = ANCHOR + HOUR;
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR + HOUR, STRICT_MISMATCH)}));
    }
    SECTION("before the anchor") {
        TestClock::now = ANCHOR - 1;
        auto context = create(data_of({experiment}));
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR - 1, STRICT_MISMATCH)}));
    }
    SECTION("with an unservable schedule") {
        experiment.switchbackStartAt.reset();
        TestClock::now = ANCHOR + HOUR + 3;
        auto context = create(data_of({experiment}));
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR + HOUR + 3, STRICT_MISMATCH)}));
    }
}

TEST_CASE("Switchback: a strict audience mismatch exposure serializes exactly", "[switchback][context][audience]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.audience = STRICT_GOLD_AUDIENCE;
    experiment.audienceStrict = true;
    auto context = create(data_of({experiment}));
    // Period 1 draws 1, so the 0 below is the strict mismatch's control, not a draw of arm 0.
    REQUIRE(EXPECTED_2_ARMS[1] == 1);
    TestClock::now = ANCHOR + HOUR + 5;
    context->treatment("exp_sb");
    TestClock::now = ANCHOR + HOUR + 9;
    const json event = context->publish();
    REQUIRE(event.dump() ==
            json::parse(std::string(R"({"attributes":[{"name":"store_key","setAt":)") + std::to_string(ANCHOR) +
                        R"(,"value":"store-42"}],"exposures":[{"assigned":false,"audienceMismatch":true,"custom":false,)"
                        R"("eligible":true,"exposedAt":)" + std::to_string(ANCHOR + HOUR + 5) +
                        R"(,"fullOn":false,"id":1,"name":"exp_sb","overridden":false,"ruleOverride":false,"unit":"store_id",)"
                        R"("variant":0}],"goals":[],"hashed":true,"publishedAt":)" + std::to_string(ANCHOR + HOUR + 9) +
                        R"(,"units":[{"type":"store_id","uid":"pAE3a1i5Drs5mKRNq56adA"}]})").dump());
}

TEST_CASE("Switchback: a strict audience mismatch on a missing or unreadable cluster map records nothing",
          "[switchback][context][audience]") {
    ClockScope clock;
    for (const ordered_json& map : {ordered_json(), clusters({{UNIT_TYPE, "'geo"}})}) {
        INFO(map.dump());
        auto experiment = switchback(2, HOUR);
        experiment.audience = STRICT_GOLD_AUDIENCE;
        experiment.audienceStrict = true;
        experiment.switchbackClusters = map;
        TestClock::now = ANCHOR + HOUR + 3;
        auto context = create(data_of({experiment}));
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(context->pending() == 0);
    }
}

TEST_CASE("Switchback: a strict-mismatched switchback gives way on a shared variable key", "[switchback][context][variables]") {
    ClockScope clock;
    auto sb = switchback(2, HOUR);
    sb.audience = STRICT_GOLD_AUDIENCE;
    sb.audienceStrict = true;
    auto unit = as_unit(switchback(2, "exp_unit", 2, HOUR));
    unit.variants = {{"A", R"({"sb.arm":10})"}, {"B", R"({"sb.arm":11})"}};
    unit.fullOnVariant = 1;
    TestClock::now = ANCHOR + HOUR + 3;
    auto context = create(data_of({sb, unit}));
    REQUIRE(context->variable_keys() ==
            std::map<std::string, std::vector<std::string>>{{"sb.arm", {"exp_sb", "exp_unit"}}});
    REQUIRE(context->peek_variable_value("sb.arm", -1) == 11);
    REQUIRE(context->pending() == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == 11);
    REQUIRE(published(*context) ==
            json::array({store_exposure(2, "exp_unit", 1, ANCHOR + HOUR + 3, {true, true, false, true})}));
}

TEST_CASE("Switchback: a strict-mismatched switchback alone on a variable key returns the default",
          "[switchback][context][variables]") {
    ClockScope clock;
    auto sb = switchback(2, HOUR);
    sb.audience = STRICT_GOLD_AUDIENCE;
    sb.audienceStrict = true;
    TestClock::now = ANCHOR + HOUR + 3;
    auto context = create(data_of({sb}));
    REQUIRE(context->peek_variable_value("sb.arm", -1) == -1);
    REQUIRE(context->variable_value("sb.arm", -1) == -1);
    REQUIRE(context->pending() == 0);
}

namespace {

// The SWITCHBACK.md "Exposure flags" table: assigned, eligible, overridden, fullOn, custom,
// audienceMismatch, ruleOverride, and the variant served at ANCHOR + 2h + 3 (period 2, whose
// 3-arm draw is 1).
struct ParityBranch {
    const char* name;
    Flags flags;
    int variant;
    std::function<void(ExperimentData&)> configure_experiment;
    std::function<void(Context&)> configure_context;
};

void strict(ExperimentData& e) {
    e.audience = STRICT_GOLD_AUDIENCE;
    e.audienceStrict = true;
}

std::vector<ParityBranch> parity_branches() {
    const auto none = [](ExperimentData&) {};
    const auto no_context = [](Context&) {};
    const auto override2 = [](Context& c) { c.set_override("exp_parity", 2); };
    const int draw = EXPECTED_3_ARMS[2];
    return {
        {"override", OVERRIDDEN, 2, none, override2},
        {"override over strict mismatch", OVERRIDDEN, 2, strict, override2},
        {"rule", RULE, 2, [](ExperimentData& e) { e.assignmentRules = ASSIGN_VARIANT_2; }, no_context},
        {"rule over strict mismatch", RULE, 2,
         [](ExperimentData& e) { strict(e); e.assignmentRules = ASSIGN_VARIANT_2; }, no_context},
        {"strict mismatch", STRICT_MISMATCH, 0, strict, no_context},
        {"strict mismatch over full-on", STRICT_MISMATCH, 0, [](ExperimentData& e) { strict(e); e.fullOnVariant = 2; },
         no_context},
        {"strict mismatch over traffic miss", STRICT_MISMATCH, 0,
         [](ExperimentData& e) { strict(e); e.trafficSplit = {1.0, 0.0}; }, no_context},
        {"non-strict mismatch", {true, true, false, false, false, true, false}, draw,
         [](ExperimentData& e) { e.audience = STRICT_GOLD_AUDIENCE; }, no_context},
        {"full-on", {true, true, false, true}, 2, [](ExperimentData& e) { e.fullOnVariant = 2; }, no_context},
        {"traffic miss", {true, false}, 0, [](ExperimentData& e) { e.trafficSplit = {1.0, 0.0}; }, no_context},
        {"custom", {true, true, false, false, true}, 2, none,
         [](Context& c) { c.set_custom_assignment("exp_parity", 2); }},
        {"draw", {}, draw, none, no_context},
    };
}

}

TEST_CASE("Switchback: exposure flags match a unit experiment for every branch", "[switchback][context][exposure]") {
    ClockScope clock(ANCHOR + 2 * HOUR + 3);
    REQUIRE(EXPECTED_3_ARMS[2] == 1);
    for (const auto& branch : parity_branches()) {
        INFO(branch.name);
        std::vector<Exposure> exposures;
        for (bool is_switchback : {false, true}) {
            auto experiment = switchback(1, "exp_parity", 3, HOUR);
            if (!is_switchback) experiment = as_unit(experiment);
            branch.configure_experiment(experiment);
            auto context = create(data_of({experiment}));
            branch.configure_context(*context);
            context->treatment("exp_parity");
            const auto event = context->publish();
            REQUIRE(event.exposures.size() == 1);
            exposures.push_back(event.exposures[0]);
        }
        const auto& unit = exposures[0];
        const auto& sb = exposures[1];
        auto flags = [](const Exposure& e) {
            return std::vector<bool>{e.assigned, e.eligible, e.overridden, e.fullOn, e.custom, e.audienceMismatch,
                                     e.ruleOverride};
        };
        const auto& f = branch.flags;
        const std::vector<bool> expected = {f.assigned, f.eligible, f.overridden, f.full_on, f.custom,
                                            f.audience_mismatch, f.rule_override};
        REQUIRE(flags(unit) == expected);
        REQUIRE(flags(sb) == expected);
        REQUIRE(sb.variant == branch.variant);
        if (std::string(branch.name) != "draw" && std::string(branch.name) != "non-strict mismatch") {
            REQUIRE(unit.variant == branch.variant);
        }
        REQUIRE(unit.id == sb.id);
        REQUIRE(unit.name == sb.name);
        REQUIRE(unit.unit == sb.unit);
        REQUIRE(unit.exposedAt == sb.exposedAt);
    }
}

TEST_CASE("Switchback: a traffic miss on an eligible cluster publishes an ineligible exposure", "[switchback][context][traffic]") {
    ClockScope clock;
    auto miss = switchback(1, "exp_miss", 2, HOUR);
    miss.trafficSplit = {1.0, 0.0};
    auto cluster = switchback(2, "exp_cluster_ineligible", 2, HOUR);
    cluster.trafficSplit = {1.0, 0.0};
    cluster.switchbackClusterValues = std::vector<std::string>{"other-store"};
    auto context = create(data_of({miss, cluster}));
    TestClock::now = ANCHOR + 2 * HOUR + 3;
    REQUIRE(context->treatment("exp_miss") == 0);
    REQUIRE(context->treatment("exp_cluster_ineligible") == 0);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_miss", 0, ANCHOR + 2 * HOUR + 3, {true, false})}));
}

TEST_CASE("Switchback: custom fields and unit lookups work", "[switchback][context]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    REQUIRE(context->get_unit(UNIT_TYPE) == json(UID));
    REQUIRE(context->custom_field_keys().empty());
    REQUIRE(context->custom_field_value("exp_sb", "missing").is_null());
}

TEST_CASE("Switchback: each experiment keeps at most two periods", "[switchback][context][cache]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    for (int i = 0; i < 50; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        context->treatment("exp_sb");
    }
    REQUIRE(ContextTestAccess::switchback_slots(*context) == 2);
    REQUIRE(context->pending() == 50);
}

TEST_CASE("Switchback: unitType is never used for switchback assignment", "[switchback][context]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.unitType = "user_id";
    auto context = create(data_of({experiment}), units({{UNIT_TYPE, UID}, {"user_id", "u1"}}));
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[static_cast<size_t>(i)]);
    }
    const auto exposures = context->publish().exposures;
    REQUIRE(exposures.size() == 3);
    for (const auto& e : exposures) REQUIRE(e.unit == UNIT_TYPE);
}

TEST_CASE("Switchback: the publish payload has no new fields", "[switchback][context][exposure]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    TestClock::now = ANCHOR + 5;
    context->treatment("exp_sb");
    TestClock::now = ANCHOR + HOUR + 7;
    context->treatment("exp_sb");
    TestClock::now = ANCHOR + HOUR + 9;
    REQUIRE(EXPECTED_2_ARMS[0] != EXPECTED_2_ARMS[1]);
    const json event = context->publish();
    // The cluster attribute is published timestamped, as before: the Collector re-resolves the
    // cluster at exposedAt from it.
    REQUIRE(event == json{{"hashed", true},
                          {"units", {{{"type", "store_id"}, {"uid", UID_HASH}}}},
                          {"publishedAt", ANCHOR + HOUR + 9},
                          {"exposures", {store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + 5),
                                         store_exposure(1, "exp_sb", EXPECTED_2_ARMS[1], ANCHOR + HOUR + 7)}},
                          {"goals", json::array()},
                          {"attributes", {{{"name", CLUSTER_PATH}, {"value", CLUSTER_KEY}, {"setAt", ANCHOR}}}}});
}

TEST_CASE("Switchback: periods published across flushes stay one per period", "[switchback][context][exposure]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    std::vector<int> variants;
    context->treatment("exp_sb");
    for (const auto& e : context->publish().exposures) variants.push_back(e.variant);
    TestClock::now = ANCHOR + 1;
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 0);
    TestClock::now = ANCHOR + HOUR;
    context->treatment("exp_sb");
    for (const auto& e : context->publish().exposures) variants.push_back(e.variant);
    REQUIRE(variants == std::vector<int>{EXPECTED_2_ARMS[0], EXPECTED_2_ARMS[1]});
}

TEST_CASE("Switchback: an exposure is stamped with the reading that chose its period", "[switchback][context][exposure]") {
    ClockScope clock;
    SECTION("treatment") {
        auto context = create(data_of({switchback(2, HOUR)}));
        TestClock::now = ANCHOR + HOUR;
        TestClock::next_read = ANCHOR + HOUR - 1;
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + HOUR - 1)}));
    }
    SECTION("variable value, with a holdout that never fires") {
        auto covered = switchback(2, HOUR);
        covered.holdoutIds = {11};
        auto context = create(data_of({covered}, {holdout({0.0, 1.0})}));
        TestClock::now = ANCHOR + HOUR;
        TestClock::next_read = ANCHOR + HOUR - 1;
        REQUIRE(context->variable_value("sb.arm", -1) == EXPECTED_2_ARMS[0]);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + HOUR - 1)}));
    }
    SECTION("after a peek, the exposing call's reading") {
        auto context = create(data_of({switchback(2, HOUR)}));
        TestClock::now = ANCHOR + 10;
        REQUIRE(context->peek("exp_sb") == EXPECTED_2_ARMS[0]);
        TestClock::now = ANCHOR + 20 * 60000;
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + 20 * 60000)}));
    }
    SECTION("a unit experiment keeps stamping at enqueue time") {
        // A unit experiment reads no clock to resolve, so its one reading is the enqueue's: the
        // switchback's reading in the same call is not reused for it.
        auto unit = switchback(2, "exp_unit", 2, HOUR);
        unit.randomizationType = "unit";
        auto context = create(data_of({switchback(2, HOUR), unit}));
        TestClock::now = ANCHOR + HOUR;
        TestClock::next_read = ANCHOR + HOUR - 1;
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        const int variant = context->treatment("exp_unit");
        REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR + HOUR - 1),
                                                   store_exposure(2, "exp_unit", variant, ANCHOR + HOUR)}));
    }
}

TEST_CASE("Switchback: a refresh to a lower period index exposes once", "[switchback][context][cache]") {
    ClockScope clock;
    SECTION("restarted grid") {
        auto context = create(data_of({switchback(2, HOUR)}));
        TestClock::now = ANCHOR + 99 * HOUR;
        context->treatment("exp_sb");
        TestClock::now = ANCHOR + 100 * HOUR;
        context->treatment("exp_sb");
        REQUIRE(context->pending() == 2);
        auto restarted = switchback(2, HOUR);
        restarted.switchbackStartAt = ANCHOR + 100 * HOUR;
        context->refresh(data_of({restarted}));
        for (int i = 0; i < 3; ++i) REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 3);
    }
    SECTION("longer period") {
        auto context = create(data_of({switchback(2, HOUR)}));
        TestClock::now = ANCHOR + 100 * HOUR;
        context->treatment("exp_sb");
        TestClock::now = ANCHOR + 101 * HOUR;
        context->treatment("exp_sb");
        context->refresh(data_of({switchback(2, 200 * HOUR)}));
        context->treatment("exp_sb");
        context->treatment("exp_sb");
        REQUIRE(context->pending() == 3);
    }
}

TEST_CASE("Switchback: a clock rewind re-exposes an evicted period at most once", "[switchback][context][cache]") {
    ClockScope clock;
    auto context = create(data_of({switchback(2, HOUR)}));
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        context->treatment("exp_sb");
    }
    REQUIRE(context->pending() == 3);
    TestClock::now = ANCHOR;
    for (int i = 0; i < 3; ++i) REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 4);
    // Retained {0, 2}: period 2 is still deduplicated, period 1 was evicted.
    TestClock::now = ANCHOR + 2 * HOUR;
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 4);
    TestClock::now = ANCHOR + HOUR;
    for (int i = 0; i < 3; ++i) REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[1]);
    REQUIRE(context->pending() == 5);
    const auto exposures = published(*context);
    REQUIRE(exposures[3] == store_exposure(1, "exp_sb", EXPECTED_2_ARMS[0], ANCHOR));
    REQUIRE(exposures[4] == store_exposure(1, "exp_sb", EXPECTED_2_ARMS[1], ANCHOR + HOUR));
}

TEST_CASE("Switchback: an override is re-resolved when the grid changes", "[switchback][context][refresh]") {
    ClockScope clock;
    auto original = switchback(3, HOUR);
    original.holdoutIds = {11};
    auto context = create(data_of({original}, {holdout({0.0, 1.0})}));
    context->set_override("exp_sb", 2);
    TestClock::now = ANCHOR + 10;
    REQUIRE(context->treatment("exp_sb") == 2);
    REQUIRE(context->variable_value("sb.arm", -1) == 2);
    REQUIRE(context->pending() == 1);

    // Same index 0 under the new period length; variant 2 now carries another value.
    auto regridded = switchback(3, 2 * HOUR);
    regridded.holdoutIds = {11};
    regridded.variants[2] = {"C", R"({"sb.arm":22})"};
    context->refresh(data_of({regridded}, {holdout({0.0, 1.0})}));
    REQUIRE(context->treatment("exp_sb") == 2);
    REQUIRE(context->variable_value("sb.arm", -1) == 22);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 2, ANCHOR + 10, OVERRIDDEN),
                                               store_exposure(1, "exp_sb", 2, ANCHOR + 10, OVERRIDDEN)}));
}

TEST_CASE("Switchback: every identity transition is a new epoch", "[switchback][context][refresh]") {
    ClockScope clock(ANCHOR + 10);
    SECTION("switchback, unit, identical switchback") {
        auto context = create(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        auto to_unit = switchback(2, HOUR);
        to_unit.randomizationType = "unit";
        context->refresh(data_of({to_unit}));
        context->treatment("exp_sb");
        REQUIRE(context->pending() == 2);
        context->refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 3);
    }
    SECTION("removed and restored") {
        auto context = create(data_of({switchback(2, HOUR)}));
        context->treatment("exp_sb");
        context->refresh(data_of({}));
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(context->pending() == 2);
        context->refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 3);
    }
    SECTION("transitions without an intervening lookup") {
        auto context = create(data_of({switchback(2, HOUR)}));
        context->treatment("exp_sb");
        auto to_unit = switchback(2, HOUR);
        to_unit.randomizationType = "unit";
        context->refresh(data_of({to_unit}));
        context->refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 2);

        context->refresh(data_of({}));
        context->refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 3);

        auto regridded = switchback(2, HOUR);
        regridded.switchbackDesignType = "other";
        context->refresh(data_of({regridded}));
        context->refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 4);

        context->refresh(data_of({switchback(2, HOUR)}));
        context->treatment("exp_sb");
        REQUIRE(context->pending() == 4);
    }
}

TEST_CASE("Switchback: the slot before the anchor is dropped when the grid changes", "[switchback][context][refresh]") {
    ClockScope clock;
    auto grid_a = switchback(2, HOUR);
    grid_a.switchbackStartAt = ANCHOR + 10 * HOUR;
    auto context = create(data_of({grid_a}));
    TestClock::now = ANCHOR + 10;
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->pending() == 0);
    context->refresh(data_of({switchback(2, HOUR)}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    REQUIRE(context->pending() == 1);
    context->refresh(data_of({grid_a}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == 0);
    REQUIRE(context->pending() == 1);
}

TEST_CASE("Switchback: a legacy switchback's unitType change is identity", "[switchback][context][legacy]") {
    ClockScope clock(ANCHOR + 10);
    auto legacy = switchback(2, HOUR);
    legacy.switchbackClusters = nullptr;
    auto context = create(data_of({legacy}), units({{UNIT_TYPE, UID}, {"user_id", "u1"}}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == 0);
    REQUIRE(ContextTestAccess::switchback(*context, "exp_sb")->unit_type == UNIT_TYPE);

    auto other = legacy;
    other.unitType = "other";
    context->refresh(data_of({other}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == -1);

    auto user = legacy;
    user.unitType = "user_id";
    context->refresh(data_of({user}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == 0);
    const auto resolution = ContextTestAccess::switchback(*context, "exp_sb");
    REQUIRE(resolution->unit_type == "user_id");
    REQUIRE_FALSE(resolution->eligible);
    REQUIRE(resolution->silent);
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback: rules are not re-evaluated for an ineligible or absent slot", "[switchback][context][rules]") {
    ClockScope clock(ANCHOR + 10);
    auto ineligible = switchback(1, "exp_ineligible", 2, HOUR);
    ineligible.assignmentRules = ASSIGN_VARIANT_1;
    ineligible.switchbackClusterValues = std::vector<std::string>{"other-store"};
    auto absent = switchback(2, "exp_absent", 2, HOUR);
    absent.assignmentRules = ASSIGN_VARIANT_1;
    absent.switchbackClusters = clusters({{"device_id", CLUSTER_PATH}});
    auto context = create(data_of({ineligible, absent}));
    REQUIRE(context->treatment("exp_ineligible") == 0);
    REQUIRE(context->treatment("exp_absent") == 0);

    context->set_attribute("x", 1);
    const char* changed = R"({"rules":[{"type":"assign","environments":[],"variant":1,"name":"changed"}]})";
    ineligible.assignmentRules = changed;
    absent.assignmentRules = changed;
    context->refresh(data_of({ineligible, absent}));
    REQUIRE(context->treatment("exp_ineligible") == 0);
    REQUIRE(context->treatment("exp_absent") == 0);
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback: an in-period rule refresh keeps the variant and exposes once", "[switchback][context][rules]") {
    ClockScope clock;
    auto experiment = switchback(2, HOUR);
    experiment.assignmentRules = ASSIGN_VARIANT_1;
    auto context = create(data_of({experiment}));
    TestClock::now = ANCHOR + 5;
    REQUIRE(context->treatment("exp_sb") == 1);
    context->set_attribute("tier", "x");
    experiment.assignmentRules = R"({"rules":[{"type":"assign","environments":[],"variant":1,)"
                                 R"("conditions":{"and":[{"eq":[{"var":"tier"},{"value":"x"}]}]}}]})";
    context->refresh(data_of({experiment}));
    TestClock::now = ANCHOR + 6;
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 1, ANCHOR + 5, RULE)}));
}

TEST_CASE("Switchback: a traffic miss before the anchor is silent", "[switchback][context][traffic]") {
    ClockScope clock(ANCHOR - 1);
    auto experiment = switchback(2, HOUR);
    experiment.trafficSplit = {1.0, 0.0};
    auto context = create(data_of({experiment}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->pending() == 0);
    // From the anchor the same miss is recorded, as eligible=false.
    TestClock::now = ANCHOR;
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR, {true, false})}));
}

TEST_CASE("Switchback: an override without a serving unit on a legacy switchback uses the declared unitType",
          "[switchback][context][legacy]") {
    ClockScope clock(ANCHOR + 5);
    auto missing = switchback(1, "exp_missing_map", 2, HOUR);
    missing.unitType = "user_id";
    missing.switchbackClusters = nullptr;
    auto unreadable = switchback(2, "exp_unreadable_map", 2, HOUR);
    unreadable.unitType = "user_id";
    unreadable.switchbackClusters = clusters({{UNIT_TYPE, "'unterminated"}});
    auto context = create(data_of({missing, unreadable}));
    context->set_override("exp_missing_map", 1);
    context->set_override("exp_unreadable_map", 1);
    REQUIRE(context->treatment("exp_missing_map") == 1);
    REQUIRE(context->treatment("exp_unreadable_map") == 1);
    REQUIRE(published(*context) == json::array({exposure(1, "exp_missing_map", "user_id", 1, ANCHOR + 5, OVERRIDDEN),
                                               exposure(2, "exp_unreadable_map", "user_id", 1, ANCHOR + 5, OVERRIDDEN)}));
}

TEST_CASE("Switchback: rule-won and traffic-miss exposures serialize exactly", "[switchback][context][exposure]") {
    ClockScope clock(ANCHOR + 5);
    auto rule = switchback(1, "exp_rule", 2, HOUR);
    rule.assignmentRules = ASSIGN_VARIANT_1;
    auto miss = switchback(2, "exp_miss", 2, HOUR);
    miss.trafficSplit = {1.0, 0.0};
    TestClock::now = ANCHOR;
    auto context = create(data_of({rule, miss}));
    TestClock::now = ANCHOR + 5;
    context->treatment("exp_rule");
    context->treatment("exp_miss");
    TestClock::now = ANCHOR + 9;
    const json event = context->publish();
    REQUIRE(event["exposures"] == json::array({store_exposure(1, "exp_rule", 1, ANCHOR + 5, RULE),
                                              store_exposure(2, "exp_miss", 0, ANCHOR + 5, {true, false})}));
}

// The C++ counterpart of java-sdk's refresh-from-audience and concurrent-resolution tests: an
// event logger that refreshes, sets attributes or reads again from inside the exposure callback.
TEST_CASE("Switchback: an event logger that re-enters the context settles on one exposure", "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + HOUR + 10);
    auto logger = std::make_shared<ExposureLog>();
    auto experiment = switchback(2, HOUR);
    experiment.switchbackClusters = clusters({{UNIT_TYPE, "city"}});
    int calls = 0;
    logger->on_exposure = [&](Context& context) {
        if (calls++ > 0) return;
        context.set_attribute("city", "Faro");
        context.refresh(data_of({experiment}));
        REQUIRE(context.treatment("exp_sb") == 1);
        auto regridded = experiment;
        regridded.switchbackDesignType = "other";
        context.refresh(data_of({regridded}));
    };
    auto context = create(data_of({experiment}), units({{UNIT_TYPE, UID}}), logger);
    context->set_attribute("city", "Lisboa");
    REQUIRE(assign_switchback("Lisboa", 1, SEED, 2) == 1);
    REQUIRE(context->treatment("exp_sb") == 1);
    // The regridded epoch reads the cluster again: Faro now.
    REQUIRE(context->variable_value("sb.arm", -1) == assign_switchback("Faro", 1, SEED, 2));
    REQUIRE(context->pending() == 2);
}

TEST_CASE("Switchback: a payload without switchbacks creates no switchback state", "[switchback][context][cost]") {
    ClockScope clock;
    auto context = create(data_of({as_unit(switchback(2, HOUR))}));
    context->treatment("exp_sb");
    context->variable_value("sb.arm", -1);
    context->refresh(data_of({as_unit(switchback(2, HOUR))}));
    REQUIRE_FALSE(ContextTestAccess::has_switchback_state(*context));
    // A unit experiment carrying every switchback field but randomizationType "unit" is inert too.
    auto carrying = switchback(2, HOUR);
    carrying.randomizationType = "unit";
    context->refresh(data_of({carrying}));
    REQUIRE_FALSE(ContextTestAccess::has_switchback_state(*context));
}

TEST_CASE("Switchback: an unchanged switchback without a start keeps its slot over a refresh", "[switchback][context][refresh]") {
    ClockScope clock(ANCHOR + 10);
    auto pending = switchback(2, HOUR);
    pending.switchbackStartAt.reset();
    auto context = create(data_of({pending}));
    // An override is the one thing such a slot records.
    context->set_override("exp_sb", 1);
    REQUIRE(context->treatment("exp_sb") == 1);
    context->refresh(data_of({pending}));
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(context->pending() == 1);
}

TEST_CASE("Switchback: each analysis-only field alone keeps the epoch", "[switchback][context][refresh]") {
    ClockScope clock(ANCHOR + 10);
    auto context = create(data_of({switchback(2, HOUR)}));
    context->treatment("exp_sb");
    for (int field = 0; field < 4; ++field) {
        auto edited = switchback(2, HOUR);
        if (field == 0) edited.switchbackCarryoverDurationMs = 900000;
        if (field == 1) edited.switchbackBurnInDurationMs = 600000;
        if (field == 2) edited.switchbackBurnOutDurationMs = 300000;
        if (field == 3) edited.switchbackEstimand = "other";
        context->refresh(data_of({edited}));
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(context->pending() == 1);
    }
}

TEST_CASE("Switchback: a traffic miss beats a custom assignment", "[switchback][context][precedence]") {
    ClockScope clock(ANCHOR + 5);
    auto experiment = switchback(2, HOUR);
    experiment.trafficSplit = {1.0, 0.0};
    auto context = create(data_of({experiment}));
    context->set_custom_assignment("exp_sb", 1);
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR + 5, {true, false})}));
}

TEST_CASE("Switchback: a rule that starts matching mid-period applies in that period", "[switchback][context][rules]") {
    ClockScope clock(ANCHOR + 5);
    auto experiment = switchback(2, HOUR);
    experiment.assignmentRules = R"({"rules":[{"type":"assign","environments":[],"variant":1,)"
                                 R"("conditions":{"and":[{"eq":[{"var":"tier"},{"value":"gold"}]}]}}]})";
    auto context = create(data_of({experiment}));
    REQUIRE(EXPECTED_2_ARMS[0] == 0);
    REQUIRE(context->treatment("exp_sb") == 0);
    context->set_attribute("tier", "gold");
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR + 5),
                                               store_exposure(1, "exp_sb", 1, ANCHOR + 5, RULE)}));
}

TEST_CASE("Switchback: a custom assignment set mid-period applies in that period", "[switchback][context][precedence]") {
    ClockScope clock(ANCHOR + 5);
    auto context = create(data_of({switchback(2, HOUR)}));
    REQUIRE(context->treatment("exp_sb") == 0);
    context->set_custom_assignment("exp_sb", 1);
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(context->pending() == 2);
}

TEST_CASE("Switchback: an exposed override without a linked unit survives set_unit", "[switchback][context]") {
    ClockScope clock(ANCHOR + 5);
    auto context = create(data_of({switchback(2, HOUR)}), units({{"user_id", "u1"}}));
    context->set_override("exp_sb", 1);
    REQUIRE(context->treatment("exp_sb") == 1);
    context->set_unit(UNIT_TYPE, UID);
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(context->pending() == 1);
}

TEST_CASE("Switchback: a full-on refresh in the same period applies", "[switchback][context][refresh]") {
    ClockScope clock(ANCHOR + 5);
    auto context = create(data_of({switchback(2, HOUR)}));
    REQUIRE(context->treatment("exp_sb") == 0);
    auto full_on = switchback(2, HOUR);
    full_on.fullOnVariant = 1;
    context->refresh(data_of({full_on}));
    REQUIRE(context->treatment("exp_sb") == 1);
    REQUIRE(context->pending() == 2);
}

TEST_CASE("Switchback: unit, switchback, unit again re-resolves every time", "[switchback][context][refresh]") {
    ClockScope clock(ANCHOR + 5);
    auto unit = switchback(2, HOUR);
    unit.randomizationType = "unit";
    auto context = create(data_of({unit}));
    context->treatment("exp_sb");
    context->refresh(data_of({switchback(2, HOUR)}));
    context->treatment("exp_sb");
    REQUIRE(ContextTestAccess::switchback_slots(*context) == 1);
    context->refresh(data_of({unit}));
    REQUIRE(ContextTestAccess::switchback_slots(*context) == 0);
    context->treatment("exp_sb");
    REQUIRE(context->pending() == 3);
}

TEST_CASE("Switchback: an integer allow-list entry admits the integer cluster", "[switchback][context][values]") {
    ClockScope clock(ANCHOR + HOUR + 5);
    auto payload = json(switchback(2, HOUR));
    payload["switchbackClusterValues"] = json::array({42});
    auto experiment = payload.get<ExperimentData>();
    experiment.switchbackClusters = clusters({{UNIT_TYPE, "n"}});
    REQUIRE(experiment.switchbackClusterValues == std::vector<std::string>{"42"});
    auto context = create(data_of({experiment}));
    context->set_attribute("n", 42);
    REQUIRE(context->treatment("exp_sb") == assign_switchback("42", 1, SEED, 2));
    REQUIRE(context->pending() == 1);
}

TEST_CASE("Switchback: a logger that refreshes in a first switchback mid-call does not free the call's assignment",
          "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + 5);
    auto unit = as_unit(switchback(2, HOUR));
    auto logger = std::make_shared<ExposureLog>();
    bool refreshed = false;
    logger->on_exposure = [&](Context& context) {
        if (refreshed) return;
        refreshed = true;
        context.refresh(data_of({switchback(2, HOUR)}));
        REQUIRE(context.treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    };
    SECTION("treatment") {
        auto context = create(data_of({unit}), units({{UNIT_TYPE, UID}}), logger);
        const int variant = context->treatment("exp_sb");
        REQUIRE((variant == 0 || variant == 1));
        REQUIRE(context->pending() == 2);
    }
    SECTION("variable_value") {
        auto context = create(data_of({unit}), units({{UNIT_TYPE, UID}}), logger);
        const json value = context->variable_value("sb.arm", -1);
        REQUIRE((value == 0 || value == 1));
        REQUIRE(context->pending() == 2);
    }
}

TEST_CASE("Switchback: a slot a logger drops mid-call is parked until the call returns", "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + 5);
    auto unit = as_unit(switchback(2, HOUR));
    auto logger = std::make_shared<ExposureLog>();
    size_t parked_during_call = 0;
    bool refreshed = false;
    logger->on_exposure = [&](Context& context) {
        if (refreshed) return;
        refreshed = true;
        context.refresh(data_of({unit}));
        parked_during_call = ContextTestAccess::retired_slots(context);
    };
    SECTION("treatment") {
        auto context = create(data_of({switchback(2, HOUR)}), units({{UNIT_TYPE, UID}}), logger);
        REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
        REQUIRE(parked_during_call == 1);
        REQUIRE(ContextTestAccess::retired_slots(*context) == 0);
    }
    SECTION("variable_value") {
        auto context = create(data_of({switchback(2, HOUR)}), units({{UNIT_TYPE, UID}}), logger);
        REQUIRE(context->variable_value("sb.arm", -1) == EXPECTED_2_ARMS[0]);
        REQUIRE(parked_during_call == 1);
        REQUIRE(ContextTestAccess::retired_slots(*context) == 0);
    }
}

TEST_CASE("Switchback: a slot dropped outside any call is freed at once", "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + 5);
    auto context = create(data_of({switchback(2, HOUR)}));
    REQUIRE(context->treatment("exp_sb") == EXPECTED_2_ARMS[0]);
    context->refresh(data_of({as_unit(switchback(2, HOUR))}));
    REQUIRE(ContextTestAccess::switchback_slots(*context) == 0);
    REQUIRE(ContextTestAccess::retired_slots(*context) == 0);
}

TEST_CASE("Switchback: an assigned winner whose arm omits a shared key returns the default", "[switchback][context][variables]") {
    ClockScope clock(ANCHOR);
    auto sb = switchback(2, HOUR);
    sb.variants[0].config = "{}";
    auto later = as_unit(switchback(2, "later", 2, HOUR));
    later.fullOnVariant = 1;
    auto context = create(data_of({sb, later}));
    REQUIRE(context->peek_variable_value("sb.arm", -1) == -1);
    REQUIRE(context->pending() == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == -1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 0, ANCHOR)}));
}

TEST_CASE("Switchback: a split rule revalidates over the serving unit, not unitType", "[switchback][context][rules]") {
    ClockScope clock(ANCHOR + 5);
    for (int u = 0; u < 16; ++u) {
        auto experiment = switchback(2, HOUR);
        experiment.unitType = "user_id";
        experiment.assignmentRules = R"({"rules":[{"type":"split","environments":[],"percentages":"50/50"}]})";
        const std::string store = "store-" + std::to_string(u);
        auto context = create(data_of({experiment}), units({{UNIT_TYPE, store}, {"user_id", "user-" + std::to_string(u)}}));
        const int variant = context->treatment("exp_sb");
        context->set_attribute("unrelated", u);
        REQUIRE(context->treatment("exp_sb") == variant);
        REQUIRE(context->pending() == 1);
    }
}

TEST_CASE("Switchback: a unit assignment a logger drops mid-exposure is parked until the call returns",
          "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + 5);
    auto unit = as_unit(switchback(2, HOUR));
    auto logger = std::make_shared<ExposureLog>();
    size_t parked_during_call = 0;
    bool refreshed = false;
    logger->on_exposure = [&](Context& context) {
        if (refreshed) return;
        refreshed = true;
        context.refresh(data_of({switchback(2, HOUR)}));
        parked_during_call = ContextTestAccess::retired_assignments(context);
    };
    auto context = create(data_of({switchback(1, "other", 2, HOUR), unit}), units({{UNIT_TYPE, UID}}), logger);
    const int variant = context->treatment("exp_sb");
    REQUIRE((variant == 0 || variant == 1));
    REQUIRE(parked_during_call == 1);
    REQUIRE(ContextTestAccess::retired_assignments(*context) == 0);
}

TEST_CASE("Switchback: a malformed allow-list entry admits nothing", "[switchback][context][values]") {
    ClockScope clock(ANCHOR + HOUR + 5);
    auto payload = json(switchback(2, HOUR));
    payload["switchbackClusterValues"] = json::array({true});
    auto experiment = payload.get<ExperimentData>();
    experiment.switchbackClusters = clusters({{UNIT_TYPE, CLUSTER_PATH}});
    REQUIRE(EXPECTED_2_ARMS[1] == 1);
    auto context = create(data_of({experiment}));
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback: a refresh error logger sees the switchback, not a unit experiment", "[switchback][context][reentrancy]") {
    ClockScope clock(ANCHOR + HOUR + 5);
    auto full_on = switchback(1, "sb", 2, HOUR);
    full_on.unitType = "u";
    full_on.fullOnVariant = 1;
    auto broken = as_unit(switchback(2, "broken", 2, HOUR));
    broken.variants[0].config = "{";
    struct ErrorLogger : ContextEventLogger {
        int reads = 0;
        void handle_event(Context& context, const std::string& type, const json&) override {
            if (type == "error" && reads++ == 0) context.treatment("sb");
        }
    };
    auto logger = std::make_shared<ErrorLogger>();
    auto context = create(data_of({}), units({{UNIT_TYPE, UID}, {"u", "x"}}), logger);
    context->refresh(data_of({full_on, broken}));
    REQUIRE(logger->reads == 1);
    REQUIRE(context->treatment("sb") == 1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "sb", 1, ANCHOR + HOUR + 5, {true, true, false, true})}));
}

TEST_CASE("Switchback: an overridden winner without a config slot keeps a shared key", "[switchback][context][variables]") {
    ClockScope clock(ANCHOR + 5);
    auto sb = switchback(2, HOUR);
    auto later = as_unit(switchback(2, "later", 2, HOUR));
    later.variants = {{"A", R"({"sb.arm":10})"}, {"B", R"({"sb.arm":11})"}};
    later.fullOnVariant = 1;
    auto context = create(data_of({sb, later}));
    context->set_override("exp_sb", 9);
    REQUIRE(context->peek_variable_value("sb.arm", -1) == -1);
    REQUIRE(context->pending() == 0);
    REQUIRE(context->variable_value("sb.arm", -1) == -1);
    REQUIRE(published(*context) == json::array({store_exposure(1, "exp_sb", 9, ANCHOR + 5, OVERRIDDEN)}));
}
