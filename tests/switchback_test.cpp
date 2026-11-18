#include <catch2/catch_test_macros.hpp>
#include <absmartly/models.h>
#include <absmartly/switchback.h>

#include "../src/switchback_internal.h"

#include <fstream>
#include <set>

using namespace absmartly;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
using internal::SwitchbackPath;

namespace {

json fixture(const char* name) {
    std::ifstream stream(std::string(ABSMARTLY_TEST_FIXTURES_DIR) + "/switchback/" + name);
    REQUIRE(stream.good());
    return json::parse(stream);
}

std::string cluster_unit(const json& value) {
    REQUIRE((value.is_string() || value.is_number_integer()));
    return value.is_string() ? value.get<std::string>() : value.dump();
}

int64_t exact_long(const json& value) {
    REQUIRE(value.is_string());
    return std::stoll(value.get<std::string>());
}

} // namespace

// Fixtures copied byte-for-byte from absmartly/abs 9bd2b5ca35 (via java-sdk 07807b5):
// lib/utils/src/test/resources/switchback/golden-vectors.json, git blob b314abeb9e97b35f911c7caacec90b8ff73b9d77
// collector/src/test/resources/switchback/attribute-path-cases.json, git blob 7b1d3fed59856fcbc296ed68a820b43359ae1932
TEST_CASE("Switchback assigner matches every platform golden vector", "[switchback][golden]") {
    const auto vectors = fixture("golden-vectors.json");
    REQUIRE(vectors["assignSwitchback"].size() == 882);
    REQUIRE(vectors["fisherYatesPermutation"].size() == 504);
    std::set<int> arm_counts;
    int boundary_cases = 0;
    for (const auto& c : vectors["assignSwitchback"]) {
        INFO(c.dump());
        const auto period = exact_long(c["periodIndex"]);
        const auto seed = exact_long(c["seed"]);
        const int arms = c["armCount"];
        arm_counts.insert(arms);
        if (period == INT64_MIN || period == INT64_MAX || seed == INT64_MIN || seed == INT64_MAX) {
            ++boundary_cases;
        }
        REQUIRE(assign_switchback(cluster_unit(c["clusterUnit"]), period, seed, arms) == c["expectedVariant"]);
    }
    REQUIRE(arm_counts == std::set<int>{2, 3, 10});
    REQUIRE(boundary_cases > 0);
    for (const auto& c : vectors["fisherYatesPermutation"]) {
        INFO(c.dump());
        REQUIRE(switchback_permutation(cluster_unit(c["clusterUnit"]), exact_long(c["cycleIndex"]),
                                      exact_long(c["seed"]), c["armCount"]) ==
                c["expectedPermutation"].get<std::vector<int>>());
    }
}

TEST_CASE("Switchback assigner covers every arm once per cycle and rejects fewer than two arms", "[switchback]") {
    for (int arms : {2, 3, 10}) {
        for (int64_t cycle = -3; cycle < 20; ++cycle) {
            std::set<int> seen;
            for (int slot = 0; slot < arms; ++slot) {
                REQUIRE(seen.insert(assign_switchback("store_42", cycle * arms + slot, 0x123456789abcLL, arms)).second);
            }
            REQUIRE(seen.size() == static_cast<size_t>(arms));
        }
    }
    REQUIRE_THROWS_AS(assign_switchback("u", 0, 0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(switchback_permutation("u", 0, 0, 0), std::invalid_argument);
}

TEST_CASE("Switchback floorDiv includes signed 64 bit edges", "[switchback]") {
    REQUIRE(internal::floor_div(0, 3) == 0);
    REQUIRE(internal::floor_div(2, 3) == 0);
    REQUIRE(internal::floor_div(3, 3) == 1);
    REQUIRE(internal::floor_div(-1, 3) == -1);
    REQUIRE(internal::floor_div(-3, 3) == -1);
    REQUIRE(internal::floor_div(-4, 3) == -2);
    REQUIRE(internal::floor_div(INT64_MIN, 3) == -3074457345618258603LL);
    REQUIRE(internal::floor_div(INT64_MAX, 10) == 922337203685477580LL);
}

TEST_CASE("Switchback paths match every platform attribute path case", "[switchback][path]") {
    const auto cases = fixture("attribute-path-cases.json");
    REQUIRE(cases["cases"].size() == 7);
    for (const auto& c : cases["cases"]) {
        INFO(c.dump());
        const auto path = SwitchbackPath::parse_supported(c["path"].get<std::string>());
        REQUIRE(path.has_value() == c["supported"].get<bool>());
        if (path) {
            const auto* value = path->resolve(cases["attributes"]);
            REQUIRE((value ? *value : json()) == c["value"]);
        }
    }
}

TEST_CASE("Switchback paths quote literal keys, collapse separators and read only the right container", "[switchback][path]") {
    const json attrs = {{"geo", {{"city", "Porto"}, {"0", "zero"}, {"a/b", "slash"}, {"+1", "plus"}}},
                        {"items", json::array({{{"city", "Lisbon"}}})}, {"name", "scalar"}};
    for (const auto& c : std::vector<std::pair<std::string, json>>{
             {"geo/'0'", "zero"}, {"geo/\"a/b\"", "slash"}, {"/geo//city/", "Porto"},
             {"items/-0/city", "Lisbon"}, {"geo/+1", "plus"}, {"geo", attrs["geo"]},
             {"geo/0/city", nullptr}, {"items/1/city", nullptr}, {"name/x", nullptr}, {"missing/x", nullptr}}) {
        INFO(c.first);
        const auto path = SwitchbackPath::parse_supported(c.first);
        REQUIRE(path.has_value());
        const auto* value = path->resolve(attrs);
        REQUIRE((value ? *value : json()) == c.second);
    }
    for (const char* path : {"geo/'0", "'geo", "\"geo/city", "geo/\"", "geo.city", "geo/a.b", "a./b", ".",
                             "items/0.5/city", "'geo.city'", "\"geo.city\"", "geo/'a.b'", "", "/", "//",
                             "0/city", "items/0", "items/0/1/city", "items/-1/city", "'"}) {
        INFO(path);
        REQUIRE_FALSE(SwitchbackPath::parse_supported(path).has_value());
    }
}

TEST_CASE("Switchback path integers follow Guava Ints.tryParse", "[switchback][path]") {
    REQUIRE(internal::try_parse_int("0") == 0);
    REQUIRE(internal::try_parse_int("-0") == 0);
    REQUIRE(internal::try_parse_int("007") == 7);
    REQUIRE(internal::try_parse_int("2147483647") == INT32_MAX);
    REQUIRE(internal::try_parse_int("-2147483648") == INT32_MIN);
    for (const char* s : {"", "-", "+1", "1a", " 1", "2147483648", "-2147483649", "99999999999999999999", "٠", "1.0"}) {
        INFO(s);
        REQUIRE_FALSE(internal::try_parse_int(s).has_value());
    }
}

TEST_CASE("Switchback cluster values distinguish every float from an integer", "[switchback][values]") {
    const auto values = json::parse(R"([null,"",true,false,1.5,3.0,{},[],"  ","042",0,-0,-7,4000000002,-9223372036854775808,18446744073709551615])");
    const std::vector<std::optional<std::string>> expected = {
        {}, {}, {}, {}, {}, {}, {}, {}, "  ", "042", "0", "0", "-7", "4000000002", "-9223372036854775808", "18446744073709551615"};
    REQUIRE(values.size() == expected.size());
    for (size_t i = 0; i < values.size(); ++i) {
        INFO(values[i].dump());
        REQUIRE(internal::normalize_cluster_key(values[i]) == expected[i]);
    }
}

TEST_CASE("Switchback ordered JSON model keeps declared source order and defaults on reuse", "[switchback][models]") {
    const auto payload = ordered_json::parse(R"({"experiments":[{"id":1,"name":"s","unitType":"legacy","iteration":1,"seedHi":-5,"seedLo":-7,"randomizationType":"switchback","switchbackClusters":{"user_id":"profile/city","device_id":"city","store_id":"city"},"switchbackClusterValues":[],"switchbackStartAt":1704067200000,"switchbackPeriodMs":1440000000000,"switchbackCarryoverDurationMs":3,"switchbackBurnInDurationMs":4,"switchbackBurnOutDurationMs":5,"switchbackDesignType":"balanced_block","switchbackEstimand":"global","future":{"a":[1,2]}}],"holdouts":[],"future":true})");
    auto data = payload.get<ContextData>();
    auto& e = data.experiments[0];
    REQUIRE(e.switchbackClusters.begin().key() == "user_id");
    REQUIRE((ordered_json(data)["experiments"][0]["switchbackClusters"]) == payload["experiments"][0]["switchbackClusters"]);
    REQUIRE(e.switchbackStartAt == 1704067200000LL);
    REQUIRE(e.switchbackPeriodMs == 1440000000000LL);
    REQUIRE(e.switchbackCarryoverDurationMs == 3);
    REQUIRE(e.switchbackBurnInDurationMs == 4);
    REQUIRE(e.switchbackBurnOutDurationMs == 5);
    REQUIRE(e.switchbackDesignType == "balanced_block");
    REQUIRE(e.switchbackEstimand == "global");
    REQUIRE(e.switchbackClusterValues.has_value());
    REQUIRE(e.switchbackClusterValues->empty());
    auto plain = payload["experiments"][0];
    for (const char* key : {"randomizationType", "switchbackClusters", "switchbackClusterValues", "switchbackStartAt",
                             "switchbackPeriodMs", "switchbackCarryoverDurationMs", "switchbackBurnInDurationMs",
                             "switchbackBurnOutDurationMs", "switchbackDesignType", "switchbackEstimand"}) {
        plain.erase(key);
    }
    plain.get_to(e);
    REQUIRE(e.randomizationType.empty());
    REQUIRE(e.switchbackClusters.is_null());
    REQUIRE_FALSE(e.switchbackClusterValues.has_value());
    REQUIRE_FALSE(e.switchbackStartAt.has_value());
    REQUIRE_FALSE(e.switchbackPeriodMs.has_value());
    REQUIRE_FALSE(e.switchbackCarryoverDurationMs.has_value());
    REQUIRE_FALSE(e.switchbackBurnInDurationMs.has_value());
    REQUIRE_FALSE(e.switchbackBurnOutDurationMs.has_value());
    REQUIRE(e.switchbackDesignType.empty());
    REQUIRE(e.switchbackEstimand.empty());
}

TEST_CASE("Switchback ordered JSON model decodes a lone experiment in served order", "[switchback][models]") {
    const auto experiment = ordered_json::parse(
        R"({"id":1,"name":"s","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,"switchbackClusters":{"z_unit":"a","a_unit":"b","m_unit":"c"}})");
    const auto decoded = experiment.get<ExperimentData>();
    std::vector<std::string> keys;
    for (auto it = decoded.switchbackClusters.begin(); it != decoded.switchbackClusters.end(); ++it) keys.push_back(it.key());
    REQUIRE(keys == std::vector<std::string>{"z_unit", "a_unit", "m_unit"});
    REQUIRE(ordered_json(decoded)["switchbackClusters"].dump() == R"({"z_unit":"a","a_unit":"b","m_unit":"c"})");
    // A nlohmann::json decode still works; it sorts the map, as nlohmann::json sorts every object.
    REQUIRE(json(experiment).get<ExperimentData>().switchbackClusters.begin().key() == "a_unit");
}

TEST_CASE("Switchback period indices floor below the anchor's integer division edge", "[switchback]") {
    // A period index is floorDiv(now - startAt, periodMs). Within the servable grid now >= startAt,
    // so only the index helper itself sees a negative dividend; it must floor, not truncate.
    ExperimentData e;
    e.split = {0.5, 0.5};
    e.switchbackClusters = nlohmann::ordered_json{{"u", "a"}};
    e.switchbackStartAt = 3600000;
    e.switchbackPeriodMs = 3600000;
    const auto index = internal::make_switchback_index(e);
    REQUIRE(index.period(3599999) == internal::NO_SWITCHBACK_PERIOD);
    REQUIRE(index.period(3600000) == 0);
    REQUIRE(index.period(7199999) == 0);
    REQUIRE(index.period(7200000) == 1);
    REQUIRE(index.period(INT64_MAX) == internal::floor_div(INT64_MAX - 3600000, 3600000));
}
