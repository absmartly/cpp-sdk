// Ports of java-sdk ContextSwitchbackClusterTest, ContextSwitchbackClusterRuleTest and
// SwitchbackServerParityTest (core-api/src/test/java/com/absmartly/sdk/ at 07807b5): cluster key
// selection through the public Context API. Expected variants come from the raw cluster key and
// assign_switchback, pinned by the platform golden vectors, plus a few independent schedules.
#include "switchback_support.h"

#include <fstream>

using namespace sbtest;

namespace {

const char* STORE = "store_id";
const char* UID = "e791e240fcd3df7d238cfc285f475e8152fcc0ec";
const int32_t SEED_HI = -5;
const int32_t SEED_LO = -7;
const int64_t SEED = seed_of(SEED_HI, SEED_LO);
const int ARMS = 3;

ExperimentData attribute_switchback(const json& path, const char* name = "exp_sb") {
    ExperimentData e;
    e.id = 1;
    e.name = name;
    e.unitType = STORE;
    e.iteration = 1;
    e.seedHi = SEED_HI;
    e.seedLo = SEED_LO;
    for (int i = 0; i < ARMS; ++i) {
        e.split.push_back(1.0 / ARMS);
        e.variants.push_back({std::string(1, static_cast<char>('A' + i)), "{\"sb.arm\":" + std::to_string(i) + "}"});
    }
    e.trafficSeedHi = 1;
    e.trafficSeedLo = 2;
    e.trafficSplit = {0.0, 1.0};
    e.randomizationType = "switchback";
    e.switchbackClusters = clusters({{STORE, path}});
    e.switchbackStartAt = ANCHOR;
    e.switchbackPeriodMs = HOUR;
    e.switchbackDesignType = "balanced_block";
    return e;
}

int expected(const std::string& key, int64_t period) { return assign_switchback(key, period, SEED, ARMS); }

std::vector<int> expected_schedule(const std::string& key) {
    std::vector<int> result;
    for (int i = 0; i < 12; ++i) result.push_back(expected(key, i));
    return result;
}

std::vector<int> peek_schedule(Context& context) {
    std::vector<int> result;
    for (int i = 0; i < 12; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        result.push_back(context.peek("exp_sb"));
    }
    return result;
}

std::unique_ptr<Context> create(const ExperimentData& experiment, ContextConfig config = units({{STORE, UID}})) {
    return std::make_unique<Context>(config, data_of({experiment}));
}

json store_exposure(int variant, int64_t at, bool eligible = true, bool audience_mismatch = false) {
    return exposure(1, "exp_sb", STORE, variant, at, {true, eligible, false, false, false, audience_mismatch});
}

}

// --- ContextSwitchbackClusterTest -------------------------------------------------------------

TEST_CASE("Switchback cluster: the unit uid is never the cluster key", "[switchback][cluster]") {
    ClockScope clock;
    auto context = create(attribute_switchback("city"));
    for (int i = 0; i < 3; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == 0);
    }
    REQUIRE(context->pending() == 0);
}

TEST_CASE("Switchback cluster: a numeric uid clusters like its canonical string", "[switchback][cluster]") {
    ClockScope clock;
    const std::vector<std::pair<json, std::string>> cases = {{123456789, "123456789"}, {123456789.0, "123456789"},
                                                             {9007199254740993LL, "9007199254740992"},
                                                             {125.75, "126"}};
    for (const auto& [numeric, text] : cases) {
        INFO(numeric.dump());
        std::vector<PublishEvent> events;
        std::vector<std::vector<int>> schedules;
        for (const json& uid : {numeric, json(text)}) {
            TestClock::now = ANCHOR;
            auto context = create(attribute_switchback("city"), units({{STORE, uid}}));
            context->set_attribute("city", "Lisboa");
            context->treatment("exp_sb");
            events.push_back(context->publish());
            schedules.push_back(peek_schedule(*context));
        }
        REQUIRE(schedules[0] == expected_schedule("Lisboa"));
        REQUIRE(schedules[0] == schedules[1]);
        REQUIRE(events[0].units[0].uid == hash_unit(text));
        REQUIRE(json(events[0]) == json(events[1]));
    }
}

TEST_CASE("Switchback cluster: numeric uids match the independent reference", "[switchback][cluster]") {
    ClockScope clock;
    const std::vector<int> schedule = {0, 2, 1, 0, 1, 2, 0, 1, 2, 2, 1, 0};
    for (const auto& [uid, hash] : std::vector<std::pair<json, std::string>>{
             {123456789, "JfnnlDI7RTiF9RgfG2JNCw"}, {9007199254740993LL, "OngwCmjeKhIQyeNybDy4eg"}}) {
        TestClock::now = ANCHOR;
        auto context = create(attribute_switchback("region"), units({{STORE, uid}}));
        context->set_attribute("region", 123456789);
        context->treatment("exp_sb");
        const auto event = context->publish();
        REQUIRE(event.units[0].uid == hash);
        REQUIRE(wire(event.exposures[0]) == store_exposure(schedule[0], ANCHOR));
        REQUIRE(peek_schedule(*context) == schedule);
    }
}

TEST_CASE("Switchback cluster: the raw attribute value is the cluster key", "[switchback][cluster]") {
    ClockScope clock;
    auto context = create(attribute_switchback("city"));
    context->set_attribute("city", "Lisboa");
    REQUIRE(peek_schedule(*context) == std::vector<int>{0, 2, 1, 2, 1, 0, 1, 0, 2, 0, 1, 2});
    REQUIRE(peek_schedule(*context) == expected_schedule("Lisboa"));
}

TEST_CASE("Switchback cluster: units sharing a value share the schedule", "[switchback][cluster]") {
    ClockScope clock;
    auto lisboa = create(attribute_switchback("city"));
    lisboa->set_attribute("city", "Lisboa");
    auto porto = create(attribute_switchback("city"));
    porto->set_attribute("city", "Porto");
    auto other = create(attribute_switchback("city"), units({{STORE, "another-store"}}));
    other->set_attribute("city", "Lisboa");
    REQUIRE(peek_schedule(*lisboa) == expected_schedule("Lisboa"));
    REQUIRE(peek_schedule(*other) == expected_schedule("Lisboa"));
    REQUIRE(peek_schedule(*porto) == expected_schedule("Porto"));
    REQUIRE(expected_schedule("Lisboa") != expected_schedule("Porto"));
}

TEST_CASE("Switchback cluster: nested, array and integer values resolve", "[switchback][cluster][values]") {
    ClockScope clock;
    auto nested = create(attribute_switchback("geo/city"));
    nested->set_attribute("geo", {{"city", "Porto"}});
    REQUIRE(peek_schedule(*nested) == expected_schedule("Porto"));

    TestClock::now = ANCHOR;
    auto array = create(attribute_switchback("items/1/city"));
    array->set_attribute("items", json::array({{{"city", "Lisbon"}}, {{"city", "Faro"}}}));
    REQUIRE(peek_schedule(*array) == expected_schedule("Faro"));

    TestClock::now = ANCHOR;
    auto integer = create(attribute_switchback("region"));
    integer->set_attribute("region", 42);
    REQUIRE(peek_schedule(*integer) == expected_schedule("42"));

    // A C++ int64_t and uint64_t are integers; a double never is, even when integral.
    TestClock::now = ANCHOR;
    auto big = create(attribute_switchback("geo/city"));
    big->set_attribute("geo", {{"city", 18446744073709551615ULL}});
    REQUIRE(peek_schedule(*big) == expected_schedule("18446744073709551615"));

    TestClock::now = ANCHOR + 10;
    auto floating = create(attribute_switchback("city"));
    floating->set_attribute("city", 2.0);
    REQUIRE(floating->treatment("exp_sb") == 0);
    REQUIRE(floating->pending() == 0);
}

TEST_CASE("Switchback cluster: values without a cluster key are ineligible", "[switchback][cluster][values]") {
    ClockScope clock;
    for (const json& value : {json(nullptr), json(""), json(true), json(1.5), json(2.0), json(3.0),
                              json({{"a", "b"}}), json::array({"x"})}) {
        INFO(value.dump());
        auto context = create(attribute_switchback("city"));
        context->set_attribute("city", value);
        TestClock::now = ANCHOR + 10;
        REQUIRE(context->treatment("exp_sb") == 0);
        // Variant 0's own config, like any ineligible assignment.
        REQUIRE(context->variable_value("sb.arm", -1) == 0);
        REQUIRE(context->pending() == 0);
    }
    auto missing = create(attribute_switchback("geo/city"));
    missing->set_attribute("geo", {{"country", "PT"}});
    TestClock::now = ANCHOR + 10;
    REQUIRE(missing->treatment("exp_sb") == 0);
    REQUIRE(missing->pending() == 0);
}

TEST_CASE("Switchback cluster: an attribute decoded from JSON text keeps 3.0 apart from 3", "[switchback][cluster][values]") {
    ClockScope clock;
    const auto attrs = json::parse(R"({"float": 3.0, "int": 3, "exp": 3e0})");
    for (const auto& [name, keyed] : std::vector<std::pair<std::string, bool>>{{"float", false}, {"int", true},
                                                                               {"exp", false}}) {
        INFO(name);
        auto context = create(attribute_switchback("v"));
        context->set_attribute("v", attrs[name]);
        TestClock::now = ANCHOR + HOUR + 10;
        REQUIRE(context->treatment("exp_sb") == (keyed ? expected("3", 1) : 0));
        REQUIRE(context->pending() == (keyed ? 1 : 0));
    }
    REQUIRE(expected("3", 1) != 0);
}

TEST_CASE("Switchback cluster: unservable configurations are ineligible", "[switchback][cluster][legacy]") {
    ClockScope clock;
    std::vector<ExperimentData> experiments;
    for (const char* path : {"items/0", "items/-1/city", "", "geo.city", "'city", "\"city"}) {
        experiments.push_back(attribute_switchback(path));
    }
    auto no_period = attribute_switchback("city");
    no_period.switchbackPeriodMs.reset();
    experiments.push_back(no_period);
    auto zero = attribute_switchback("city");
    zero.switchbackPeriodMs = 0;
    experiments.push_back(zero);
    auto negative = attribute_switchback("city");
    negative.switchbackPeriodMs = -HOUR;
    experiments.push_back(negative);
    auto one_arm = attribute_switchback("city");
    one_arm.split = {1.0};
    experiments.push_back(one_arm);
    auto pathless = attribute_switchback("city");
    pathless.switchbackClusters = clusters({{STORE, nullptr}});
    experiments.push_back(pathless);
    auto numeric_path = attribute_switchback("city");
    numeric_path.switchbackClusters = clusters({{STORE, 5}});
    experiments.push_back(numeric_path);
    for (const auto& experiment : experiments) {
        INFO(json(experiment).dump());
        auto context = create(experiment);
        context->set_attribute("city", "Lisboa");
        context->set_attribute("items", json::array({{{"city", "Lisbon"}}}));
        for (int i = 0; i < 3; ++i) {
            TestClock::now = ANCHOR + i * HOUR;
            REQUIRE(context->treatment("exp_sb") == 0);
        }
        REQUIRE(context->pending() == 0);
    }
}

TEST_CASE("Switchback cluster: a missing or empty cluster map is only a legacy ineligible control",
          "[switchback][cluster][legacy]") {
    ClockScope clock;
    for (const ordered_json& map : {ordered_json(), ordered_json::object(), ordered_json::array()}) {
        INFO(map.dump());
        auto experiment = attribute_switchback("city");
        experiment.switchbackClusters = map;
        auto context = create(experiment);
        context->set_attribute("city", "Lisboa");
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(context->variable_value("sb.arm", -1) == 0);
        REQUIRE(context->pending() == 0);

        auto other = create(experiment, units({{"user_id", "u1"}}));
        other->set_attribute("city", "Lisboa");
        REQUIRE(other->treatment("exp_sb") == 0);
        REQUIRE(other->variable_value("sb.arm", -1) == -1);
        REQUIRE(other->pending() == 0);
    }
}

TEST_CASE("Switchback cluster: the latest attribute set at or before the resolution is used", "[switchback][cluster]") {
    ClockScope clock;
    auto context = create(attribute_switchback("city"));
    TestClock::now = ANCHOR + 2 * HOUR;
    context->set_attribute("city", "Porto");
    TestClock::now = ANCHOR + 4 * HOUR;
    context->set_attribute("city", "Lisboa");
    TestClock::now = ANCHOR + HOUR;
    REQUIRE(context->peek("exp_sb") == 0);
    TestClock::now = ANCHOR + 3 * HOUR;
    REQUIRE(context->peek("exp_sb") == expected("Porto", 3));
    TestClock::now = ANCHOR + 4 * HOUR;
    REQUIRE(context->peek("exp_sb") == expected("Lisboa", 4));
}

TEST_CASE("Switchback cluster: setAttribute mid-period does not reassign within the period", "[switchback][cluster][period]") {
    ClockScope clock(ANCHOR + 10);
    auto context = create(attribute_switchback("city"));
    context->set_attribute("city", "Lisboa");
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    TestClock::now = ANCHOR + 20;
    context->set_attribute("city", "Porto");
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    REQUIRE(context->peek("exp_sb") == expected("Lisboa", 0));
    REQUIRE(context->pending() == 1);
    int differing = 0;
    for (int i = 1; i < 12; ++i) {
        TestClock::now = ANCHOR + i * HOUR;
        REQUIRE(context->treatment("exp_sb") == expected("Porto", i));
        differing += expected("Porto", i) != expected("Lisboa", i);
    }
    REQUIRE(differing != 0);
    REQUIRE(context->pending() == 12);
}

TEST_CASE("Switchback cluster: a mid-period cluster change before the exposure or a requeue keeps the cached arm",
          "[switchback][cluster][period]") {
    // The arm cached at the period's first read stays for the period; a cluster change applies
    // from the next one.
    REQUIRE(expected("Lisboa", 0) != expected("Faro", 0));
    REQUIRE(expected("Lisboa", 1) != expected("Faro", 1));
    SECTION("changed after a peek, before the first exposure") {
        ClockScope clock(ANCHOR + 10);
        auto context = create(attribute_switchback("city"));
        context->set_attribute("city", "Lisboa");
        REQUIRE(context->peek("exp_sb") == expected("Lisboa", 0));
        context->set_attribute("city", "Faro");
        REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
        REQUIRE(published(*context) == json::array({store_exposure(expected("Lisboa", 0), ANCHOR + 10)}));
        TestClock::now = ANCHOR + HOUR;
        REQUIRE(context->treatment("exp_sb") == expected("Faro", 1));
    }
    SECTION("changed before a requeue in the same period") {
        ClockScope clock(ANCHOR + 10);
        auto experiment = attribute_switchback("city");
        experiment.audience = R"({"filter":[{"eq":[{"var":"tier"},{"value":"gold"}]}]})";
        auto context = create(experiment);
        context->set_attribute("city", "Lisboa");
        context->set_attribute("tier", "gold");
        REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
        TestClock::now = ANCHOR + 20;
        // The audience change requeues the slot; the cluster change does not move it.
        context->set_attribute("city", "Faro");
        context->set_attribute("tier", "silver");
        REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
        const auto exposures = published(*context);
        REQUIRE(exposures.size() == 2);
        REQUIRE(exposures[1]["variant"] == expected("Lisboa", 0));
        TestClock::now = ANCHOR + HOUR;
        REQUIRE(context->treatment("exp_sb") == expected("Faro", 1));
    }
}

TEST_CASE("Switchback cluster: an attribute set after an ineligible resolution waits for the next period",
          "[switchback][cluster][period]") {
    ClockScope clock(ANCHOR + 10);
    auto context = create(attribute_switchback("city"));
    REQUIRE(context->treatment("exp_sb") == 0);
    context->set_attribute("city", "Lisboa");
    TestClock::now = ANCHOR + 20;
    REQUIRE(context->treatment("exp_sb") == 0);
    REQUIRE(context->pending() == 0);
    TestClock::now = ANCHOR + HOUR;
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 1));
    REQUIRE(published(*context) == json::array({store_exposure(expected("Lisboa", 1), ANCHOR + HOUR)}));
}

TEST_CASE("Switchback cluster: traffic changes within the period keep its key", "[switchback][cluster][period]") {
    ClockScope clock(ANCHOR + 10);
    auto context = create(attribute_switchback("city"));
    context->set_attribute("city", "Lisboa");
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    TestClock::now = ANCHOR + 20;
    context->set_attribute("city", "Porto");
    auto no_traffic = attribute_switchback("city");
    no_traffic.trafficSplit = {1.0, 0.0};
    context->refresh(data_of({no_traffic}));
    REQUIRE(context->treatment("exp_sb") == 0);
    context->refresh(data_of({attribute_switchback("city")}));
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    REQUIRE(context->pending() == 3);
    TestClock::now = ANCHOR + HOUR;
    REQUIRE(context->treatment("exp_sb") == expected("Porto", 1));
}

TEST_CASE("Switchback cluster: traffic restored within the period keeps the key", "[switchback][cluster][traffic]") {
    ClockScope clock(ANCHOR + 10);
    auto no_traffic = attribute_switchback("city");
    no_traffic.trafficSplit = {1.0, 0.0};
    auto context = create(no_traffic);
    context->set_attribute("city", "Faro");
    REQUIRE(context->treatment("exp_sb") == 0);
    TestClock::now = ANCHOR + 20;
    context->set_attribute("city", "Lisboa");
    context->refresh(data_of({attribute_switchback("city")}));
    REQUIRE(expected("Faro", 0) == 1);
    REQUIRE(context->treatment("exp_sb") == expected("Faro", 0));
    REQUIRE(published(*context) ==
            json::array({store_exposure(0, ANCHOR + 10, false), store_exposure(expected("Faro", 0), ANCHOR + 20)}));
}

TEST_CASE("Switchback cluster: an audience change re-resolves the slot but keeps its key", "[switchback][cluster][audience]") {
    ClockScope clock(ANCHOR + 10);
    auto experiment = attribute_switchback("city");
    experiment.audience = R"({"filter":[{"eq":[{"var":"tier"},{"value":"gold"}]}]})";
    SECTION("keyed slot") {
        auto context = create(experiment);
        context->set_attribute("city", "Lisboa");
        context->set_attribute("tier", "gold");
        REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
        TestClock::now = ANCHOR + 20;
        context->set_attribute("city", "Porto");
        context->set_attribute("tier", "silver");
        REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
        REQUIRE(published(*context) == json::array({store_exposure(expected("Lisboa", 0), ANCHOR + 10),
                                                   store_exposure(expected("Lisboa", 0), ANCHOR + 20, true, true)}));
    }
    SECTION("slot first resolved without a key") {
        auto context = create(experiment);
        context->set_attribute("tier", "gold");
        REQUIRE(context->treatment("exp_sb") == 0);
        TestClock::now = ANCHOR + 20;
        context->set_attribute("city", "Faro");
        context->set_attribute("tier", "silver");
        REQUIRE(expected("Faro", 0) == 1);
        REQUIRE(context->treatment("exp_sb") == 0);
        REQUIRE(context->pending() == 0);
        TestClock::now = ANCHOR + HOUR;
        REQUIRE(context->peek("exp_sb") == expected("Faro", 1));
        TestClock::now = ANCHOR + 2 * HOUR;
        REQUIRE(expected("Faro", 2) == 2);
        REQUIRE(context->peek("exp_sb") == expected("Faro", 2));
    }
}

TEST_CASE("Switchback cluster: a cluster path change invalidates the period", "[switchback][cluster][refresh]") {
    ClockScope clock(ANCHOR + 10);
    auto context = create(attribute_switchback("city"));
    context->set_attribute("city", "Lisboa");
    context->set_attribute("geo", {{"city", "Porto"}});
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    context->refresh(data_of({attribute_switchback("city")}));
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    REQUIRE(context->pending() == 1);
    context->refresh(data_of({attribute_switchback("geo/city")}));
    REQUIRE(context->treatment("exp_sb") == expected("Porto", 0));
    REQUIRE(context->pending() == 2);
    auto filtered = attribute_switchback("geo/city");
    filtered.switchbackClusterValues = std::vector<std::string>{"Porto", "Faro"};
    context->refresh(data_of({filtered}));
    REQUIRE(context->treatment("exp_sb") == expected("Porto", 0));
    REQUIRE(context->pending() == 3);
}

TEST_CASE("Switchback cluster: re-linking to another unit type re-keys the period", "[switchback][cluster][refresh]") {
    ClockScope clock(ANCHOR + 1);
    auto context = create(attribute_switchback("city"), units({{STORE, UID}, {"user_id", "user-0002"}}));
    context->set_attribute("city", "Lisboa");
    context->set_attribute("ucity", "Faro");
    REQUIRE(context->treatment("exp_sb") == expected("Lisboa", 0));
    auto by_user = attribute_switchback("ucity");
    by_user.switchbackClusters = clusters({{"user_id", "ucity"}});
    context->refresh(data_of({by_user}));
    REQUIRE(context->treatment("exp_sb") == expected("Faro", 0));
    const auto exposures = context->publish().exposures;
    REQUIRE(exposures.size() == 2);
    REQUIRE(exposures[0].unit == STORE);
    REQUIRE(exposures[1].unit == "user_id");
}

TEST_CASE("Switchback cluster: a custom assignment applies only with a cluster key", "[switchback][cluster][precedence]") {
    ClockScope clock;
    auto keyed = create(attribute_switchback("city"));
    keyed->set_attribute("city", "Lisboa");
    keyed->set_custom_assignment("exp_sb", 2);
    REQUIRE(keyed->treatment("exp_sb") == 2);
    auto unkeyed = create(attribute_switchback("city"));
    unkeyed->set_custom_assignment("exp_sb", 2);
    REQUIRE(unkeyed->treatment("exp_sb") == 0);
}

TEST_CASE("Switchback cluster: unit experiments ignore cluster fields", "[switchback][cluster]") {
    ClockScope clock;
    for (const char* type : {"unit", ""}) {
        INFO(type);
        auto unit = attribute_switchback("city");
        unit.randomizationType = type;
        unit.switchbackClusters = clusters({{"user_id", "city"}, {STORE, "'x"}});
        unit.switchbackClusterValues = std::vector<std::string>{"nothing"};
        unit.switchbackPeriodMs = 0;
        // A split rule randomises over unitType for a unit experiment, never a cluster unit.
        unit.assignmentRules = R"({"rules":[{"type":"split","environments":[],"percentages":"34/33/33"}]})";
        ExperimentData reference;
        reference.id = unit.id;
        reference.name = unit.name;
        reference.unitType = unit.unitType;
        reference.iteration = unit.iteration;
        reference.seedHi = unit.seedHi;
        reference.seedLo = unit.seedLo;
        reference.split = unit.split;
        reference.trafficSeedHi = unit.trafficSeedHi;
        reference.trafficSeedLo = unit.trafficSeedLo;
        reference.trafficSplit = unit.trafficSplit;
        reference.variants = unit.variants;
        reference.randomizationType = type;
        reference.assignmentRules = unit.assignmentRules;

        std::vector<json> published_events;
        for (const auto& experiment : {unit, reference}) {
            auto context = create(experiment, units({{STORE, UID}, {"user_id", "u"}}));
            context->set_attribute("city", "Lisboa");
            TestClock::now = ANCHOR - HOUR;
            const int first = context->treatment("exp_sb");
            for (int i = 1; i < 6; ++i) {
                TestClock::now = ANCHOR - HOUR + i * HOUR;
                REQUIRE(context->treatment("exp_sb") == first);
            }
            published_events.push_back(published(*context));
        }
        REQUIRE(published_events[0].size() == 1);
        REQUIRE(published_events[0][0]["eligible"] == true);
        REQUIRE(published_events[0][0]["ruleOverride"] == true);
        REQUIRE(published_events[0][0]["exposedAt"] == ANCHOR - HOUR);
        REQUIRE(published_events[0] == published_events[1]);
    }
}

// --- ContextSwitchbackClusterRuleTest -----------------------------------------------------------
// The Collector's ContextAPISwitchbackTest ClusterRule section: switchback "sbc" (id 900,
// "34/33/33", 1h) linked to user_id at "profile/store" and to session_id at "session_store",
// declared session_id first (unit type id order).

namespace {

const char* SESSION = "session_id";
const char* USER = "user_id";
const int64_t PERIOD = 5;
const int64_t NOW = ANCHOR + PERIOD * HOUR + 17 * 60000;
const int32_t RULE_SEED_HI = 0x1234;
const int32_t RULE_SEED_LO = 0x5678abcd;
const int64_t RULE_SEED = seed_of(RULE_SEED_HI, RULE_SEED_LO);

ExperimentData sbc() {
    ExperimentData e;
    e.id = 900;
    e.name = "sbc";
    e.unitType = USER;
    e.iteration = 1;
    e.seedHi = RULE_SEED_HI;
    e.seedLo = RULE_SEED_LO;
    e.split = {0.34, 0.33, 0.33};
    for (int i = 0; i < 3; ++i) {
        e.variants.push_back({std::to_string(i), json({{"sbc.config", "test config " + std::to_string(i)}}).dump()});
    }
    e.trafficSeedHi = 7;
    e.trafficSeedLo = 11;
    e.trafficSplit = {0.0, 1.0};
    e.randomizationType = "switchback";
    e.switchbackClusters = clusters({{SESSION, "session_store"}, {USER, "profile/store"}});
    e.switchbackStartAt = ANCHOR;
    e.switchbackPeriodMs = HOUR;
    e.switchbackDesignType = "balanced_block";
    return e;
}

int scheduled(const std::string& key) { return assign_switchback(key, PERIOD, RULE_SEED, 3); }

struct Assigned {
    std::string unit;
    bool eligible = false;
    bool silent = false;
    int variant = 0;
    bool full_on = false;
    bool rule_override = false;
    json config;
    std::vector<Exposure> exposures;
};

// The Collector's ExperimentAssignment for "sbc", or nullopt when it lists none. Every other
// experiment is resolved and exposed first.
std::optional<Assigned> resolve_sbc(const json& attributes, std::vector<ExperimentData> experiments,
                                    std::initializer_list<std::pair<const char*, json>> unit_list) {
    ClockScope clock(NOW);
    auto log = std::make_shared<ExposureLog>();
    Context context(units(unit_list), data_of(experiments), log);
    for (auto it = attributes.begin(); it != attributes.end(); ++it) context.set_attribute(it.key(), it.value());
    for (const auto& other : experiments) {
        if (other.name != "sbc") context.treatment(other.name);
    }
    const int variant = context.treatment("sbc");
    const auto resolution = ContextTestAccess::switchback(context, "sbc");
    REQUIRE(resolution.has_value());
    const json config = context.variable_value("sbc.config", nullptr);
    std::vector<Exposure> exposures;
    for (const auto& e : log->exposures) {
        if (e.name == "sbc") exposures.push_back(e);
    }
    if (!resolution->applies) {
        REQUIRE(variant == 0);
        REQUIRE(config.is_null());
        REQUIRE(exposures.empty());
        return std::nullopt;
    }
    return Assigned{resolution->unit_type, resolution->eligible, resolution->silent, variant, resolution->full_on,
                    resolution->rule_override, config, exposures};
}

std::optional<Assigned> resolve_sbc(const json& attributes, std::initializer_list<std::pair<const char*, json>> unit_list,
                                    ExperimentData experiment = sbc()) {
    return resolve_sbc(attributes, {experiment}, unit_list);
}

void require_ineligible(const std::optional<Assigned>& a, const std::string& unit) {
    REQUIRE(a.has_value());
    REQUIRE(a->unit == unit);
    REQUIRE_FALSE(a->eligible);
    REQUIRE(a->silent);
    REQUIRE(a->variant == 0);
    REQUIRE_FALSE(a->full_on);
    REQUIRE_FALSE(a->rule_override);
    REQUIRE(a->config == "test config 0");
    REQUIRE(a->exposures.empty());
}

json store(const json& value) { return {{"store", value}}; }

}

TEST_CASE("Switchback rule: a missing or unreadable cluster configuration is only a legacy ineligible control",
          "[switchback][rule][legacy]") {
    const json attributes = {{"geo", {{"city", "Porto"}}}, {"geo.city", "Faro"}, {"session_store", "s-1"}};
    for (const char* broken : {"null-cluster", "missing-links", "unreadable-path", "geo.city", "'geo.city'",
                               "\"geo.city\"", "unknown-unit"}) {
        INFO(broken);
        auto experiment = sbc();
        const std::string b = broken;
        if (b == "null-cluster") {
            experiment.switchbackClusters = nullptr;
        } else if (b == "missing-links") {
            experiment.switchbackClusters = ordered_json::object();
        } else if (b == "unreadable-path") {
            experiment.switchbackClusters = clusters({{SESSION, "'geo"}});
        } else if (b == "unknown-unit") {
            // A readable map whose only linked type (device_id) is absent from the context.
            experiment.switchbackClusters = clusters({{"device_id", "session_store"}});
            REQUIRE_FALSE(resolve_sbc({{"session_store", "s-1"}}, {{SESSION, "x1"}, {USER, "u1"}}, experiment));
            continue;
        } else {
            experiment.switchbackClusters = clusters({{SESSION, broken}});
        }
        // The declared unit (user_id) only identifies an ineligible control response.
        require_ineligible(resolve_sbc(attributes, {{SESSION, "x1"}, {USER, "u1"}}, experiment), USER);
        // Without a unit of the declared type, there is no response at all.
        REQUIRE_FALSE(resolve_sbc(attributes, {{SESSION, "x1"}}, experiment));
    }
}

TEST_CASE("Switchback rule: traffic uses the serving uid, not the experiment's unit", "[switchback][rule][traffic]") {
    auto experiment = sbc();
    experiment.trafficSplit = {0.6, 0.4};
    ExperimentData ordinary = sbc();
    ordinary.id = 901;
    ordinary.name = "ordinary";
    ordinary.split = {0.5, 0.5};
    ordinary.variants = {{"0", nullptr}, {"1", nullptr}};
    ordinary.randomizationType.clear();
    ordinary.switchbackClusters = nullptr;
    ordinary.switchbackStartAt.reset();
    ordinary.switchbackPeriodMs.reset();
    ordinary.switchbackDesignType.clear();
    std::string witnesses[2];
    for (int i = 0; i < 1000 && (witnesses[0].empty() || witnesses[1].empty()); ++i) {
        const std::string uid = "traffic-" + std::to_string(i);
        witnesses[VariantAssigner(hash_unit(uid)).assign({0.6, 0.4}, 7, 11) == 0 ? 0 : 1] = uid;
    }
    REQUIRE_FALSE(witnesses[0].empty());
    REQUIRE_FALSE(witnesses[1].empty());
    const std::string key = "s-18";
    REQUIRE(scheduled(key) != 0);
    for (bool serving_eligible : {false, true}) {
        const auto a = resolve_sbc({{"session_store", key}, {"profile", store(key)}}, {experiment, ordinary},
                                   {{SESSION, witnesses[serving_eligible ? 1 : 0]},
                                    {USER, witnesses[serving_eligible ? 0 : 1]}});
        REQUIRE(a->unit == SESSION);
        REQUIRE(a->eligible == serving_eligible);
        REQUIRE(a->variant == (serving_eligible ? scheduled(key) : 0));
        // A traffic miss on an eligible cluster is still recorded, under the serving unit.
        REQUIRE(a->exposures.size() == 1);
        REQUIRE(a->exposures[0].unit == SESSION);
        REQUIRE(a->exposures[0].eligible == serving_eligible);
    }
}

TEST_CASE("Switchback rule: one matching unit serves its value", "[switchback][rule]") {
    const auto a = resolve_sbc({{"profile", store("s-17")}}, {{USER, "u1"}});
    REQUIRE(a->eligible);
    REQUIRE(a->unit == USER);
    REQUIRE(a->variant == scheduled("s-17"));
}

TEST_CASE("Switchback rule: a strict audience mismatch on an eligible cluster is listed as control and recorded",
          "[switchback][rule][audience]") {
    auto experiment = sbc();
    experiment.audience = R"({"filter":[{"eq":[{"var":"tier"},{"value":"gold"}]}]})";
    experiment.audienceStrict = true;
    const auto a = resolve_sbc({{"session_store", "s-17"}}, {{SESSION, "x1"}}, experiment);
    REQUIRE(a.has_value());
    REQUIRE(a->unit == SESSION);
    REQUIRE(a->eligible);
    REQUIRE_FALSE(a->silent);
    REQUIRE(a->variant == 0);
    REQUIRE(a->exposures.size() == 1);
    REQUIRE(a->exposures[0].audienceMismatch);
    REQUIRE_FALSE(a->exposures[0].assigned);
}

TEST_CASE("Switchback rule: two agreeing units serve the value, two disagreeing ones are ineligible", "[switchback][rule]") {
    const auto agree = resolve_sbc({{"profile", store("s-17")}, {"session_store", "s-17"}}, {{USER, "u1"}, {SESSION, "x1"}});
    REQUIRE(agree->eligible);
    REQUIRE(agree->variant == scheduled("s-17"));
    // session_id is first in declared order whatever the order the units were set in.
    require_ineligible(resolve_sbc({{"profile", store("s-17")}, {"session_store", "s-18"}}, {{USER, "u1"}, {SESSION, "x1"}}),
                       SESSION);
}

TEST_CASE("Switchback rule: a second uid for one unit type is rejected", "[switchback][rule]") {
    ClockScope clock(NOW);
    Context context(units({{SESSION, "x1"}}), data_of({sbc()}));
    REQUIRE_THROWS(context.set_unit(SESSION, "x2"));
    const auto a = resolve_sbc({{"session_store", "s-17"}}, {{SESSION, "x1"}});
    REQUIRE(a->eligible);
    REQUIRE(a->variant == scheduled("s-17"));
}

TEST_CASE("Switchback rule: no matching unit does not apply; a matching unit without a value is ineligible",
          "[switchback][rule]") {
    REQUIRE_FALSE(resolve_sbc({{"session_store", "s-1"}}, {{"device_id", "d1"}}));
    REQUIRE_FALSE(resolve_sbc({{"session_store", "s-1"}}, {}));
    require_ineligible(resolve_sbc({{"other", "s-1"}}, {{USER, "u1"}}), USER);
    require_ineligible(resolve_sbc({{"other", "s-1"}}, {{USER, "u1"}, {SESSION, "x1"}}), SESSION);
}

TEST_CASE("Switchback rule: non-string non-integer values are ineligible", "[switchback][rule][values]") {
    for (const json& value : {json(1.5), json(3.0), json(true), json({{"a", 1}}), json::array({"a"}), json("")}) {
        INFO(value.dump());
        require_ineligible(resolve_sbc({{"session_store", value}}, {{SESSION, "x1"}}), SESSION);
    }
}

TEST_CASE("Switchback rule: whole numbers are keyed by their decimal text", "[switchback][rule][values]") {
    REQUIRE(scheduled("17") != 0);
    const auto a = resolve_sbc({{"session_store", 17}}, {{SESSION, "x1"}});
    REQUIRE(a->eligible);
    REQUIRE(a->variant == scheduled("17"));
    REQUIRE(resolve_sbc({{"session_store", 17}, {"profile", store("17")}}, {{SESSION, "x1"}, {USER, "u1"}})->eligible);
}

TEST_CASE("Switchback rule: nested and array paths are read", "[switchback][rule][path]") {
    auto experiment = sbc();
    experiment.switchbackClusters = clusters({{SESSION, "items/1/store"}});
    const auto a = resolve_sbc({{"items", json::array({store("a"), store("b")})}}, {{SESSION, "x1"}}, experiment);
    REQUIRE(a->variant == scheduled("b"));
    REQUIRE(scheduled("a") != scheduled("b"));
}

TEST_CASE("Switchback rule: the exposure unit is the first matching unit in declared order", "[switchback][rule][order]") {
    const std::string key = "s-18";
    REQUIRE(scheduled(key) != 0);
    const auto a = resolve_sbc({{"profile", store(key)}, {"session_store", key}}, {{USER, "u1"}, {SESSION, "x1"}});
    REQUIRE(a->unit == SESSION);
    REQUIRE(a->exposures[0].unit == SESSION);
    REQUIRE(a->variant == scheduled(key));
}

TEST_CASE("Switchback rule: an ineligible cluster never publishes an exposure", "[switchback][rule]") {
    ClockScope clock(NOW);
    auto log = std::make_shared<ExposureLog>();
    Context context(units({{USER, "u1"}, {SESSION, "x1"}}), data_of({sbc()}), log);
    context.set_attribute("session_store", "a");
    context.set_attribute("profile", store("b"));
    REQUIRE(context.treatment("sbc") == 0);
    REQUIRE(context.pending() == 0);
    REQUIRE(context.publish().exposures.empty());
    REQUIRE(log->exposures.empty());
}

TEST_CASE("Switchback rule: a unit without a value does not serve even if declared first", "[switchback][rule][order]") {
    const std::string key = "s-17";
    REQUIRE(scheduled(key) != 0);
    const auto a = resolve_sbc({{"profile", store(key)}}, {{SESSION, "x1"}, {USER, "u1"}});
    REQUIRE(a->unit == USER);
    REQUIRE(a->exposures[0].unit == USER);
    REQUIRE(a->variant == scheduled(key));
}

TEST_CASE("Switchback rule: the allow-list filters values; a null list admits every value", "[switchback][rule][values]") {
    auto filtered = sbc();
    filtered.switchbackClusterValues = std::vector<std::string>{"s-1", "s-2"};
    require_ineligible(resolve_sbc({{"session_store", "s-3"}}, {{SESSION, "x1"}}, filtered), SESSION);
    const auto allowed = resolve_sbc({{"session_store", "s-2"}}, {{SESSION, "x1"}}, filtered);
    REQUIRE(allowed->eligible);
    REQUIRE(allowed->variant == scheduled("s-2"));
    for (int i = 0; i < 20; ++i) {
        const std::string key = "s-" + std::to_string(i);
        const auto a = resolve_sbc({{"session_store", key}}, {{SESSION, "x1"}});
        REQUIRE(a->eligible);
        REQUIRE(a->variant == scheduled(key));
    }
}

TEST_CASE("Switchback rule: a valid cluster without a linked unit does not fall back to unitType", "[switchback][rule]") {
    auto experiment = sbc();
    experiment.switchbackClusters = clusters({{SESSION, "session_store"}});
    const std::string key = "s-17";
    REQUIRE(scheduled(key) != 0);
    REQUIRE_FALSE(resolve_sbc({{"session_store", key}, {"profile", store(key)}}, {{USER, "u1"}}, experiment));
    const auto linked = resolve_sbc({{"session_store", key}}, {{USER, "u1"}, {SESSION, "x1"}}, experiment);
    REQUIRE(linked->unit == SESSION);
    REQUIRE(linked->variant == scheduled(key));
}

namespace {

// The Collector's wire-rule oracle SdkSwitchbackRule, transcribed: the period from the anchor
// re-ceiled to the hour, traffic on the serving uid's hash, then the draw over the cluster key.
struct Oracle {
    static constexpr int64_t NO_PERIOD = INT64_MIN;
    static int64_t period(const ExperimentData& e, int64_t now) {
        if (!e.switchbackStartAt || !e.switchbackPeriodMs || *e.switchbackPeriodMs <= 0 || e.split.size() < 2) {
            return NO_PERIOD;
        }
        const int64_t anchor = -internal::floor_div(-*e.switchbackStartAt, HOUR) * HOUR;
        return now < anchor ? NO_PERIOD : internal::floor_div(now - anchor, *e.switchbackPeriodMs);
    }
    // {variant, eligible}
    static std::pair<int, bool> evaluate(const ExperimentData& e, const std::string& uid,
                                         const std::optional<std::string>& key, int64_t now) {
        const int64_t p = period(e, now);
        if (!key || p == NO_PERIOD) return {0, false};
        if (VariantAssigner(hash_unit(uid)).assign(e.trafficSplit, e.trafficSeedHi, e.trafficSeedLo) != 1) {
            return {0, false};
        }
        return {assign_switchback(*key, p, seed_of(e.seedHi, e.seedLo), static_cast<int>(e.split.size())), true};
    }
};

enum class ClusterCase { Single, DisagreeingSecondSource, MissingValue, FilteredValue };

}

TEST_CASE("Switchback rule: the context matches the SDK switchback wire-rule parity grid", "[switchback][rule][grid]") {
    int eligible = 0;
    int ineligible = 0;
    int traffic_misses = 0;
    for (auto cluster : {ClusterCase::Single, ClusterCase::DisagreeingSecondSource, ClusterCase::MissingValue,
                         ClusterCase::FilteredValue}) {
        for (int64_t anchor : {ANCHOR, ANCHOR + 7 * HOUR}) {
            for (int64_t period_ms : {HOUR, 2 * HOUR, 24 * HOUR}) {
                for (int u = 0; u < 6; ++u) {
                    const std::string uid = "uid-" + std::to_string(u);
                    for (const std::string key : {"s-1", "s-2", "42", "Lisboa"}) {
                        for (int64_t now : {anchor - HOUR, anchor - 1, anchor, anchor + period_ms - 1, anchor + period_ms,
                                            anchor + 3 * period_ms + 5, anchor + 11 * period_ms}) {
                            auto e = sbc();
                            e.trafficSplit = {0.3, 0.7};
                            e.switchbackStartAt = anchor;
                            e.switchbackPeriodMs = period_ms;
                            e.switchbackClusters = clusters({{SESSION, "session_store"}});
                            json attrs = {{"session_store", key}};
                            ContextConfig config = units({{SESSION, uid}});
                            switch (cluster) {
                            case ClusterCase::DisagreeingSecondSource:
                                e.switchbackClusters = clusters({{SESSION, "session_store"}, {USER, "profile/store"}});
                                attrs["profile"] = store(key + "-other");
                                config.units[USER] = "u-" + uid;
                                break;
                            case ClusterCase::MissingValue:
                                attrs = {{"other", key}};
                                break;
                            case ClusterCase::FilteredValue:
                                e.switchbackClusterValues = std::vector<std::string>{key + "-allowed"};
                                break;
                            default:
                                break;
                            }
                            const auto oracle_key = cluster == ClusterCase::Single ? std::optional<std::string>(key)
                                                                                   : std::nullopt;
                            const auto oracle = Oracle::evaluate(e, uid, oracle_key, now);
                            ClockScope clock(now);
                            auto log = std::make_shared<ExposureLog>();
                            Context context(config, data_of({e}), log);
                            for (auto it = attrs.begin(); it != attrs.end(); ++it) context.set_attribute(it.key(), it.value());
                            INFO(static_cast<int>(cluster) << "/" << anchor << "/" << period_ms << "/" << uid << "/" << key
                                                           << "/" << now);
                            REQUIRE(context.treatment("sbc") == oracle.first);
                            REQUIRE(ContextTestAccess::switchback(context, "sbc")->eligible == oracle.second);
                            // Before the anchor nothing is recorded; from it, a keyed cluster records one
                            // exposure whose eligibility is the oracle's (a traffic miss included).
                            const size_t expected_exposures = (oracle_key && now >= anchor) ? 1 : 0;
                            REQUIRE(log->exposures.size() == expected_exposures);
                            if (expected_exposures == 1) REQUIRE(log->exposures[0].eligible == oracle.second);
                            if (cluster == ClusterCase::Single) {
                                if (oracle.second) {
                                    ++eligible;
                                } else {
                                    ++ineligible;
                                    if (now >= anchor) ++traffic_misses;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    REQUIRE(eligible > 100);
    REQUIRE(ineligible > 100);
    REQUIRE(traffic_misses > 0);

    // The deliberate divergence: an anchor off the hour is unservable in the SDK (as in the
    // Collector's Registry), while the transcribed oracle would re-ceil it and draw.
    auto e = sbc();
    e.switchbackStartAt = ANCHOR + 7 * HOUR + 1;
    e.switchbackClusters = clusters({{SESSION, "session_store"}});
    const int64_t now = ANCHOR + 8 * HOUR + 5;
    REQUIRE(Oracle::period(e, now) != Oracle::NO_PERIOD);
    REQUIRE(Oracle::evaluate(e, "uid-0", std::string("s-1"), now).second);
    ClockScope clock(now);
    auto log = std::make_shared<ExposureLog>();
    Context context(units({{SESSION, "uid-0"}}), data_of({e}), log);
    context.set_attribute("session_store", "s-1");
    REQUIRE(context.treatment("sbc") == 0);
    REQUIRE_FALSE(ContextTestAccess::switchback(context, "sbc")->eligible);
    REQUIRE(log->exposures.empty());
}

// --- SwitchbackServerParityTest -------------------------------------------------------------------

namespace {

const char* DEVICE = "device_id";
const int64_t PARITY_SEED = 1234567890123LL;
const int PERIODS = 4;

ExperimentData parity_experiment(const ordered_json& map, std::optional<std::vector<std::string>> values = std::nullopt) {
    ExperimentData e;
    e.id = 1;
    e.name = "exp_sb";
    // R33 payloads may still carry unitType; it is never used for switchback assignment.
    e.unitType = USER;
    e.iteration = 1;
    e.seedHi = static_cast<int32_t>(PARITY_SEED >> 32);
    e.seedLo = static_cast<int32_t>(static_cast<uint32_t>(PARITY_SEED));
    for (int i = 0; i < ARMS; ++i) {
        e.split.push_back(1.0 / ARMS);
        e.variants.push_back({std::to_string(i), nullptr});
    }
    e.trafficSplit = {0.0, 1.0};
    e.randomizationType = "switchback";
    e.switchbackClusters = map;
    e.switchbackClusterValues = std::move(values);
    e.switchbackStartAt = ANCHOR;
    e.switchbackPeriodMs = HOUR;
    e.switchbackDesignType = "balanced_block";
    return e;
}

struct Served {
    std::vector<int> variants;
    std::vector<Exposure> exposures;
};

// Serves PERIODS periods with treatment(), one call per period. Attributes are set in the order given.
Served serve(const ExperimentData& experiment, std::initializer_list<std::pair<const char*, json>> unit_list,
             const ordered_json& attributes) {
    ClockScope clock(ANCHOR);
    auto log = std::make_shared<ExposureLog>();
    Context context(units(unit_list), data_of({experiment}), log);
    for (auto it = attributes.begin(); it != attributes.end(); ++it) context.set_attribute(it.key(), json(it.value()));
    Served served;
    for (int p = 0; p < PERIODS; ++p) {
        TestClock::now = ANCHOR + p * HOUR + 1;
        served.variants.push_back(context.treatment("exp_sb"));
    }
    served.exposures = log->exposures;
    return served;
}

int collector_variant(const std::string& key, int64_t period) { return assign_switchback(key, period, PARITY_SEED, ARMS); }

void require_eligible(const Served& served, const std::string& serving_type, const std::string& key) {
    REQUIRE(served.exposures.size() == PERIODS);
    for (int p = 0; p < PERIODS; ++p) {
        const int expected_variant = collector_variant(key, p);
        REQUIRE(served.variants[static_cast<size_t>(p)] == expected_variant);
        const auto& e = served.exposures[static_cast<size_t>(p)];
        REQUIRE(e.unit == serving_type);
        REQUIRE(e.variant == expected_variant);
        REQUIRE(e.assigned);
        REQUIRE(e.eligible);
        REQUIRE(e.exposedAt == ANCHOR + p * HOUR + 1);
    }
}

void require_silent(const Served& served) {
    REQUIRE(served.variants == std::vector<int>(PERIODS, 0));
    REQUIRE(served.exposures.empty());
}

ordered_json two_sources() { return clusters({{STORE, "store_city"}, {DEVICE, "geo/city"}}); }

ordered_json attrs(std::initializer_list<std::pair<const char*, json>> entries) {
    ordered_json map = ordered_json::object();
    for (const auto& [name, value] : entries) map[name] = value;
    return map;
}

}

TEST_CASE("Switchback parity: raw key schedules match the independent reference", "[switchback][parity]") {
    require_eligible(serve(parity_experiment(clusters({{STORE, "city"}})), {{STORE, "s1"}}, attrs({{"city", "Lisboa"}})),
                     STORE, "Lisboa");
    for (int p = 0; p < PERIODS; ++p) {
        REQUIRE(collector_variant("Lisboa", p) == std::vector<int>{0, 1, 2, 2}[static_cast<size_t>(p)]);
        REQUIRE(collector_variant("Porto", p) == std::vector<int>{2, 1, 0, 1}[static_cast<size_t>(p)]);
        REQUIRE(collector_variant("42", p) == std::vector<int>{0, 2, 1, 2}[static_cast<size_t>(p)]);
    }
}

TEST_CASE("Switchback parity: agreement serves the first source unit; disagreement records nothing", "[switchback][parity]") {
    require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                           attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Lisboa"}}}})),
                     STORE, "Lisboa");
    require_silent(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                         attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Porto"}}}})));
}

TEST_CASE("Switchback parity: a missing value is skipped and the next value serves", "[switchback][parity]") {
    require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                           attrs({{"geo", {{"city", "Porto"}}}})),
                     DEVICE, "Porto");
    require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                           attrs({{"store_city", "Porto"}})),
                     STORE, "Porto");
    require_silent(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}}, attrs({{"other", "x"}})));
    require_silent(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}}, attrs({})));
}

TEST_CASE("Switchback parity: only linked units present in the context take part", "[switchback][parity]") {
    require_eligible(serve(parity_experiment(two_sources()), {{DEVICE, "d1"}},
                           attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Porto"}}}})),
                     DEVICE, "Porto");
}

TEST_CASE("Switchback parity: the allow-list", "[switchback][parity][values]") {
    const auto lisboa = attrs({{"store_city", "Lisboa"}});
    using V = std::vector<std::string>;
    require_silent(serve(parity_experiment(two_sources(), V{"Porto", "Faro"}), {{STORE, "s1"}, {DEVICE, "d1"}}, lisboa));
    require_eligible(serve(parity_experiment(two_sources(), V{"Porto", "Lisboa"}), {{STORE, "s1"}, {DEVICE, "d1"}}, lisboa),
                     STORE, "Lisboa");
    // A null or empty allow-list admits every value.
    require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}}, lisboa), STORE, "Lisboa");
    require_eligible(serve(parity_experiment(two_sources(), V{}), {{STORE, "s1"}, {DEVICE, "d1"}}, lisboa), STORE, "Lisboa");
    // The filter compares the normalised text of numeric values.
    require_eligible(serve(parity_experiment(two_sources(), V{"42"}), {{STORE, "s1"}, {DEVICE, "d1"}},
                           attrs({{"store_city", 42}})),
                     STORE, "42");
    // A filtered first value is not replaced by a later allowed one: they disagree.
    require_silent(serve(parity_experiment(two_sources(), V{"Porto"}), {{STORE, "s1"}, {DEVICE, "d1"}},
                         attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Porto"}}}})));
}

TEST_CASE("Switchback parity: the declared order decides, not the order units were set", "[switchback][parity][order]") {
    const auto agreeing = attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Lisboa"}}}});
    require_eligible(serve(parity_experiment(clusters({{DEVICE, "geo/city"}, {STORE, "store_city"}})),
                           {{STORE, "s1"}, {DEVICE, "d1"}}, agreeing),
                     DEVICE, "Lisboa");
    require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}}, agreeing), STORE, "Lisboa");
    // Neither alphabetical nor reverse alphabetical: the kept order of three sources.
    const auto three = attrs({{"a", "Faro"}, {"b", "Faro"}, {"c", "Faro"}});
    require_eligible(serve(parity_experiment(clusters({{USER, "a"}, {DEVICE, "b"}, {STORE, "c"}})),
                           {{STORE, "s1"}, {DEVICE, "d1"}, {USER, "u1"}}, three),
                     USER, "Faro");
    require_eligible(serve(parity_experiment(clusters({{STORE, "c"}, {USER, "a"}, {DEVICE, "b"}})),
                           {{STORE, "s1"}, {DEVICE, "d1"}, {USER, "u1"}}, three),
                     STORE, "Faro");
}

TEST_CASE("Switchback parity: non-key values are skipped, never a disagreement", "[switchback][parity][values]") {
    for (const json& value : {json(true), json(false), json(1.5), json(2.0), json({{"a", "b"}}), json::array({"x"}),
                              json::array(), json(nullptr), json("")}) {
        INFO(value.dump());
        require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                               attrs({{"store_city", value}, {"geo", {{"city", "Porto"}}}})),
                         DEVICE, "Porto");
        require_silent(serve(parity_experiment(clusters({{STORE, "store_city"}})), {{STORE, "s1"}},
                             attrs({{"store_city", value}})));
    }
}

TEST_CASE("Switchback parity: integral numbers are keyed as text", "[switchback][parity][values]") {
    for (const json& s : {json(42), json(int64_t{42}), json(uint64_t{42}), json("42")}) {
        for (const json& d : {json(42), json(int64_t{42}), json(uint64_t{42}), json("42")}) {
            require_eligible(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                                   attrs({{"store_city", s}, {"geo", {{"city", d}}}})),
                             STORE, "42");
        }
    }
    require_eligible(serve(parity_experiment(clusters({{STORE, "n"}})), {{STORE, "s1"}}, attrs({{"n", -7}})), STORE, "-7");
    require_eligible(serve(parity_experiment(clusters({{STORE, "n"}})), {{STORE, "s1"}}, attrs({{"n", 4000000002LL}})),
                     STORE, "4000000002");
    require_silent(serve(parity_experiment(two_sources()), {{STORE, "s1"}, {DEVICE, "d1"}},
                         attrs({{"store_city", 42}, {"geo", {{"city", "042"}}}})));
}

TEST_CASE("Switchback parity: a malformed path makes the experiment unservable", "[switchback][parity][path]") {
    const auto attributes = attrs({{"store_city", "Lisboa"}, {"geo", {{"city", "Porto"}}}, {"geo.city", "Faro"}});
    for (const char* path : {"'geo/city", "\"geo", "geo/'city", "geo.city", "geo/a.b", "'geo.city'", "\"geo.city\""}) {
        INFO(path);
        require_silent(serve(parity_experiment(clusters({{STORE, "store_city"}, {DEVICE, path}})),
                             {{STORE, "s1"}, {DEVICE, "d1"}}, attributes));
        require_silent(serve(parity_experiment(clusters({{DEVICE, path}})), {{STORE, "s1"}, {DEVICE, "d1"}}, attributes));
    }
}

TEST_CASE("Switchback parity: no linked unit does not apply", "[switchback][parity]") {
    require_silent(serve(parity_experiment(two_sources()), {{USER, "u1"}}, attrs({{"store_city", "Lisboa"}})));
}

TEST_CASE("Switchback parity: traffic follows the serving unit hash", "[switchback][parity][traffic]") {
    int eligible = 0;
    int ineligible = 0;
    for (int u = 0; u < 24; ++u) {
        auto sb = parity_experiment(two_sources());
        sb.trafficSplit = {0.5, 0.5};
        sb.trafficSeedHi = 3;
        sb.trafficSeedLo = 4;
        auto twin = sb;
        twin.name = "exp_unit";
        twin.id = 2;
        twin.randomizationType.clear();
        twin.unitType = DEVICE;
        ClockScope clock(ANCHOR);
        auto log = std::make_shared<ExposureLog>();
        Context context(units({{STORE, "s1"}, {DEVICE, "device-" + std::to_string(u)}}), data_of({sb, twin}), log);
        context.set_attribute("geo", {{"city", "Porto"}});
        context.treatment("exp_unit");
        const bool twin_eligible = log->exposures[0].eligible;
        for (int p = 0; p < PERIODS; ++p) {
            TestClock::now = ANCHOR + p * HOUR;
            const int variant = context.treatment("exp_sb");
            const auto& e = log->exposures[static_cast<size_t>(p + 1)];
            REQUIRE(e.unit == DEVICE);
            REQUIRE(e.eligible == twin_eligible);
            REQUIRE(variant == (twin_eligible ? collector_variant("Porto", p) : 0));
        }
        (twin_eligible ? eligible : ineligible)++;
    }
    REQUIRE(eligible > 0);
    REQUIRE(ineligible > 0);
}

TEST_CASE("Switchback parity: every attribute path case served as a cluster path", "[switchback][parity][path]") {
    std::ifstream stream(std::string(ABSMARTLY_TEST_FIXTURES_DIR) + "/switchback/attribute-path-cases.json");
    const auto fixture = ordered_json::parse(stream);
    int eligible_cases = 0;
    for (const auto& c : fixture["cases"]) {
        INFO(c.dump());
        const auto served = serve(parity_experiment(clusters({{STORE, c["path"]}})), {{STORE, "s1"}}, fixture["attributes"]);
        if (!c["supported"].get<bool>() || c["value"].is_null()) {
            require_silent(served);
            continue;
        }
        require_eligible(served, STORE, c["value"].get<std::string>());
        ++eligible_cases;
    }
    REQUIRE(eligible_cases == 3);
}

TEST_CASE("Switchback parity: repeated attribute names resolve to the latest, matched trimmed", "[switchback][parity]") {
    require_eligible(serve(parity_experiment(clusters({{STORE, "cluster"}})), {{STORE, "s1"}}, attrs({{" cluster ", "Porto"}})),
                     STORE, "Porto");
    ClockScope clock(ANCHOR);
    auto log = std::make_shared<ExposureLog>();
    Context context(units({{STORE, "s1"}}), data_of({parity_experiment(clusters({{STORE, "cluster"}}))}), log);
    context.set_attribute("cluster", "Lisboa");
    context.set_attribute("cluster", "Porto");
    TestClock::now = ANCHOR + 1;
    REQUIRE(context.treatment("exp_sb") == collector_variant("Porto", 0));
    REQUIRE(log->exposures[0].eligible);
}

TEST_CASE("Switchback parity: system attributes are not visible to cluster paths", "[switchback][parity]") {
    ClockScope clock(ANCHOR + 1);
    ContextConfig config = units({{STORE, "s1"}});
    config.application = "website";
    config.environment = "production";
    Context context(config, data_of({parity_experiment(clusters({{STORE, "application"}}))}));
    REQUIRE(context.treatment("exp_sb") == 0);
    REQUIRE(context.pending() == 0);
}

TEST_CASE("Switchback parity: the shared fixtures are the Collector's blobs", "[switchback][parity]") {
    // git hash-object of each fixture equals the Collector blob it was copied from.
    const auto blob = [](const char* name) {
        std::ifstream stream(std::string(ABSMARTLY_TEST_FIXTURES_DIR) + "/switchback/" + name, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        return bytes;
    };
    const auto golden = blob("golden-vectors.json");
    const auto paths = blob("attribute-path-cases.json");
    REQUIRE(md5_hex(golden) == "a0aee91f37268e249a9276ea96d771cd");
    REQUIRE(md5_hex(paths) == "a311fc7d32f940c11763546e0855497d");
}
