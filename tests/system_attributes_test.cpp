#include <catch2/catch_test_macros.hpp>
#include <absmartly/context.h>
#include <absmartly/errors.h>
#include <absmartly/sdk.h>
#include <absmartly/sdk_config.h>
#include <absmartly/client.h>
#include <absmartly/client_config.h>
#include <absmartly/context_data_provider.h>
#include <absmartly/http_client.h>

using namespace absmartly;

// A single-experiment context whose audience filter targets a system attribute
// (application/environment). The experiment is fully in traffic (trafficSplit
// {0,1}) with a 0/100 split to variant 1 and audienceStrict on, so a match gives
// variant 1 and a mismatch gives the strict control variant 0. This mirrors the
// regression in the brief: a flag at 100% B serving A to everyone because the
// SDK never saw `environment`/`application`.
static ContextData make_system_audience_data(const std::string& audience) {
    ContextData data;

    ExperimentData exp;
    exp.id = 1;
    exp.name = "exp_system";
    exp.unitType = "session_id";
    exp.iteration = 1;
    exp.seedHi = 3603515;
    exp.seedLo = 233373850;
    exp.split = {0.0, 1.0};
    exp.trafficSeedHi = 449867249;
    exp.trafficSeedLo = 455443629;
    exp.trafficSplit = {0.0, 1.0};
    exp.fullOnVariant = 0;
    exp.audienceStrict = true;
    exp.audience = nlohmann::json(audience);

    ExperimentVariant v0;
    v0.name = "A";
    v0.config = nullptr;

    ExperimentVariant v1;
    v1.name = "B";
    v1.config = nlohmann::json(R"({"color":"blue"})");

    exp.variants = {v0, v1};
    data.experiments.push_back(exp);

    return data;
}

static ContextConfig make_system_config(const std::string& application,
                                        const std::string& environment) {
    ContextConfig config;
    config.publish_delay = -1;
    config.refresh_period = 0;
    config.units = {
        {"session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec"},
        {"user_id", "123456789"}
    };
    config.application = application;
    config.environment = environment;
    return config;
}

TEST_CASE("System attributes: strict audience on environment matches", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"environment"},{"value":"production"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    REQUIRE(ctx.treatment("exp_system") == 1);

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].audienceMismatch == false);
}

TEST_CASE("System attributes: strict audience on application matches", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"application"},{"value":"website"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    REQUIRE(ctx.treatment("exp_system") == 1);

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].audienceMismatch == false);
}

TEST_CASE("System attributes: no match gives strict control with mismatch", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"environment"},{"value":"staging"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    REQUIRE(ctx.treatment("exp_system") == 0);

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].audienceMismatch == true);
}

TEST_CASE("System attributes: user-set value overrides the system value", "[system-attributes]") {
    SECTION("a staging filter matches once the user sets environment=staging") {
        auto data = make_system_audience_data(
            R"({"filter":[{"eq":[{"var":"environment"},{"value":"staging"}]}]})");
        auto config = make_system_config("website", "production");

        Context ctx(config, data);
        // System value (production) does not satisfy the staging filter yet.
        REQUIRE(ctx.treatment("exp_system") == 0);
        REQUIRE(ctx.pending() == 1);

        // User overrides environment after creation; the cached assignment is
        // invalidated and re-evaluated, now matching.
        ctx.set_attribute("environment", "staging");
        REQUIRE(ctx.treatment("exp_system") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("a production filter stops matching once the user sets environment=staging") {
        auto data = make_system_audience_data(
            R"({"filter":[{"eq":[{"var":"environment"},{"value":"production"}]}]})");
        auto config = make_system_config("website", "production");

        Context ctx(config, data);
        REQUIRE(ctx.treatment("exp_system") == 1);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("environment", "staging");
        REQUIRE(ctx.treatment("exp_system") == 0);
        REQUIRE(ctx.pending() == 2);
    }
}

TEST_CASE("System attributes: missing config means no key and no match", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"environment"},{"value":"production"}]}]})");
    // No environment configured: the key is absent from the evaluation map, so the
    // filter cannot match and strict mode serves control. No crash.
    auto config = make_system_config("website", "");

    Context ctx(config, data);
    REQUIRE(ctx.treatment("exp_system") == 0);

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].audienceMismatch == true);
}

TEST_CASE("System attributes: nothing extra is published", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"environment"},{"value":"production"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    ctx.treatment("exp_system");

    auto event = ctx.publish();

    // Check the serialized payload, not an in-memory object: the system attributes
    // are injected into the evaluation map only, never into the publish payload
    // (the collector attaches application/environment server-side).
    nlohmann::json payload = event;
    REQUIRE(payload.contains("attributes"));
    REQUIRE(payload["attributes"].is_array());
    REQUIRE(payload["attributes"].empty());
}

TEST_CASE("System attributes: user-set attributes are still published", "[system-attributes]") {
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"environment"},{"value":"production"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    ctx.set_attribute("user_agent", "Mozilla/5.0");
    ctx.treatment("exp_system");

    auto event = ctx.publish();
    nlohmann::json payload = event;

    // Only the user-set attribute is published; application/environment are not.
    REQUIRE(payload["attributes"].size() == 1);
    REQUIRE(payload["attributes"][0]["name"] == "user_agent");
    for (const auto& attr : payload["attributes"]) {
        REQUIRE(attr["name"] != "application");
        REQUIRE(attr["name"] != "environment");
        REQUIRE(attr["name"] != "app_version");
        REQUIRE(attr["name"] != "application_version");
    }
}

TEST_CASE("System attributes: app_version is not injected", "[system-attributes]") {
    // Even with an application version configured on the client (the SDK sends
    // X-Application-Version in headers), no app_version attribute is injected, so a
    // filter on app_version does not match and strict mode serves control.
    auto data = make_system_audience_data(
        R"({"filter":[{"eq":[{"var":"app_version"},{"value":"0"}]}]})");
    auto config = make_system_config("website", "production");

    Context ctx(config, data);
    REQUIRE(ctx.treatment("exp_system") == 0);

    auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].audienceMismatch == true);
}

// A never-called HTTP client; the data provider below bypasses the network.
