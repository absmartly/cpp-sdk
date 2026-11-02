#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <absmartly/context.h>
#include <absmartly/errors.h>

using namespace absmartly;

static ContextData make_test_data() {
    ContextData data;

    {
        ExperimentData exp;
        exp.id = 1;
        exp.name = "exp_test_ab";
        exp.unitType = "session_id";
        exp.iteration = 1;
        exp.seedHi = 3603515;
        exp.seedLo = 233373850;
        exp.split = {0.5, 0.5};
        exp.trafficSeedHi = 449867249;
        exp.trafficSeedLo = 455443629;
        exp.trafficSplit = {0.0, 1.0};
        exp.fullOnVariant = 0;
        exp.audience = nullptr;

        ExperimentVariant v0;
        v0.name = "A";
        v0.config = nullptr;

        ExperimentVariant v1;
        v1.name = "B";
        v1.config = nlohmann::json(R"({"banner.border":1,"banner.size":"large"})");

        exp.variants = {v0, v1};
        data.experiments.push_back(exp);
    }

    {
        ExperimentData exp;
        exp.id = 2;
        exp.name = "exp_test_abc";
        exp.unitType = "session_id";
        exp.iteration = 1;
        exp.seedHi = 55006150;
        exp.seedLo = 47189152;
        exp.split = {0.34, 0.33, 0.33};
        exp.trafficSeedHi = 705671872;
        exp.trafficSeedLo = 212903484;
        exp.trafficSplit = {0.0, 1.0};
        exp.fullOnVariant = 0;
        exp.audience = nlohmann::json("");

        ExperimentVariant v0;
        v0.name = "A";
        v0.config = nullptr;

        ExperimentVariant v1;
        v1.name = "B";
        v1.config = nlohmann::json(R"({"button.color":"blue"})");

        ExperimentVariant v2;
        v2.name = "C";
        v2.config = nlohmann::json(R"({"button.color":"red"})");

        CustomFieldValue cf;
        cf.name = "country";
        cf.value = "US,PT,ES,DE,FR";
        cf.type = "string";
        exp.customFieldValues.push_back(cf);

        exp.variants = {v0, v1, v2};
        data.experiments.push_back(exp);
    }

    {
        ExperimentData exp;
        exp.id = 3;
        exp.name = "exp_test_not_eligible";
        exp.unitType = "user_id";
        exp.iteration = 1;
        exp.seedHi = 503266407;
        exp.seedLo = 144942754;
        exp.split = {0.34, 0.33, 0.33};
        exp.trafficSeedHi = 87768905;
        exp.trafficSeedLo = 511357582;
        exp.trafficSplit = {0.99, 0.01};
        exp.fullOnVariant = 0;
        exp.audience = nlohmann::json("{}");

        ExperimentVariant v0;
        v0.name = "A";
        v0.config = nullptr;

        ExperimentVariant v1;
        v1.name = "B";
        v1.config = nlohmann::json(R"({"card.width":"80%"})");

        ExperimentVariant v2;
        v2.name = "C";
        v2.config = nlohmann::json(R"({"card.width":"75%"})");

        exp.variants = {v0, v1, v2};
        data.experiments.push_back(exp);
    }

    {
        ExperimentData exp;
        exp.id = 4;
        exp.name = "exp_test_fullon";
        exp.unitType = "session_id";
        exp.iteration = 1;
        exp.seedHi = 856061641;
        exp.seedLo = 990838475;
        exp.split = {0.25, 0.25, 0.25, 0.25};
        exp.trafficSeedHi = 360868579;
        exp.trafficSeedLo = 330937933;
        exp.trafficSplit = {0.0, 1.0};
        exp.fullOnVariant = 2;
        exp.audience = nlohmann::json("null");

        ExperimentVariant v0;
        v0.name = "A";
        v0.config = nullptr;

        ExperimentVariant v1;
        v1.name = "B";
        v1.config = nlohmann::json(R"({"submit.color":"red","submit.shape":"circle"})");

        ExperimentVariant v2;
        v2.name = "C";
        v2.config = nlohmann::json(R"({"submit.color":"blue","submit.shape":"rect"})");

        ExperimentVariant v3;
        v3.name = "D";
        v3.config = nlohmann::json(R"({"submit.color":"green","submit.shape":"square"})");

        exp.variants = {v0, v1, v2, v3};
        data.experiments.push_back(exp);
    }

    {
        ExperimentData exp;
        exp.id = 5;
        exp.name = "exp_test_custom_fields";
        exp.unitType = "session_id";
        exp.iteration = 1;
        exp.seedHi = 9372617;
        exp.seedLo = 121364805;
        exp.split = {0.5, 0.5};
        exp.trafficSeedHi = 318746944;
        exp.trafficSeedLo = 359812364;
        exp.trafficSplit = {0.0, 1.0};
        exp.fullOnVariant = 0;
        exp.audience = nullptr;

        ExperimentVariant v0;
        v0.name = "A";
        v0.config = nullptr;

        ExperimentVariant v1;
        v1.name = "B";
        v1.config = nlohmann::json(R"({"submit.size":"sm"})");

        CustomFieldValue cf1;
        cf1.name = "country";
        cf1.value = "US,PT,ES";
        cf1.type = "string";
        exp.customFieldValues.push_back(cf1);

        CustomFieldValue cf2;
        cf2.name = "languages";
        cf2.value = "en-US,en-GB,pt-PT,pt-BR,es-ES,es-MX";
        cf2.type = "string";
        exp.customFieldValues.push_back(cf2);

        CustomFieldValue cf3;
        cf3.name = "text_field";
        cf3.value = "hello text";
        cf3.type = "text";
        exp.customFieldValues.push_back(cf3);

        CustomFieldValue cf4;
        cf4.name = "number_field";
        cf4.value = "123";
        cf4.type = "number";
        exp.customFieldValues.push_back(cf4);

        CustomFieldValue cf5;
        cf5.name = "boolean_field";
        cf5.value = "true";
        cf5.type = "boolean";
        exp.customFieldValues.push_back(cf5);

        CustomFieldValue cf6;
        cf6.name = "false_boolean_field";
        cf6.value = "false";
        cf6.type = "boolean";
        exp.customFieldValues.push_back(cf6);

        exp.variants = {v0, v1};
        data.experiments.push_back(exp);
    }

    return data;
}

static ContextData make_audience_data() {
    auto data = make_test_data();
    for (auto& exp : data.experiments) {
        if (exp.name == "exp_test_ab") {
            exp.audience = nlohmann::json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
        }
    }
    return data;
}

static ContextData make_audience_strict_data() {
    auto data = make_audience_data();
    for (auto& exp : data.experiments) {
        if (exp.name == "exp_test_ab") {
            exp.audienceStrict = true;
            for (auto& v : exp.variants) {
                if (v.name == "A") {
                    v.config = nlohmann::json(R"({"banner.size":"tiny"})");
                }
            }
        }
    }
    return data;
}

static ContextData make_refresh_data() {
    auto data = make_test_data();

    ExperimentData exp;
    exp.id = 6;
    exp.name = "exp_test_new";
    exp.unitType = "session_id";
    exp.iteration = 2;
    exp.seedHi = 934590467;
    exp.seedLo = 714771373;
    exp.split = {0.5, 0.5};
    exp.trafficSeedHi = 940553836;
    exp.trafficSeedLo = 270705624;
    exp.trafficSplit = {0.0, 1.0};
    exp.fullOnVariant = 1;

    ExperimentVariant v0;
    v0.name = "A";
    v0.config = nullptr;

    ExperimentVariant v1;
    v1.name = "B";
    v1.config = nlohmann::json(R"({"show-modal":true})");

    exp.variants = {v0, v1};
    data.experiments.insert(data.experiments.begin(), exp);

    return data;
}

static ContextConfig make_test_config() {
    ContextConfig config;
    config.publish_delay = -1;
    config.refresh_period = 0;
    config.units = {
        {"session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec"},
        {"user_id", "123456789"}
    };
    return config;
}

class MockEventHandler : public ContextEventLogger {
public:
    struct Event {
        std::string type;
        nlohmann::json data;
    };

    void handle_event(Context& context, const std::string& event_type, const nlohmann::json& data) override {
        (void)context;
        events.push_back({event_type, data});
    }

    std::vector<Event> events;

    void clear() { events.clear(); }

    int count_events(const std::string& type) const {
        int c = 0;
        for (const auto& e : events) {
            if (e.type == type) c++;
        }
        return c;
    }
};

class MockDataProvider : public ContextDataProvider {
public:
    explicit MockDataProvider(ContextData data) : data_(std::move(data)) {}

    std::future<ContextData> get_context_data() override {
        std::promise<ContextData> promise;
        promise.set_value(data_);
        return promise.get_future();
    }

    void set_data(ContextData data) {
        data_ = std::move(data);
    }

private:
    ContextData data_;
};

TEST_CASE("Context construction", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should be ready with data") {
        Context ctx(config, data);
        REQUIRE(ctx.is_ready());
        REQUIRE_FALSE(ctx.is_failed());
        REQUIRE_FALSE(ctx.is_finalized());
        REQUIRE_FALSE(ctx.is_finalizing());
    }

    SECTION("should load experiment names") {
        Context ctx(config, data);
        auto exps = ctx.experiments();
        REQUIRE(exps.size() == 5);
        REQUIRE(exps[0] == "exp_test_ab");
        REQUIRE(exps[1] == "exp_test_abc");
        REQUIRE(exps[2] == "exp_test_not_eligible");
        REQUIRE(exps[3] == "exp_test_fullon");
        REQUIRE(exps[4] == "exp_test_custom_fields");
    }

    SECTION("should emit ready event") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);
        REQUIRE(handler->count_events("ready") == 1);
    }
}

TEST_CASE("Context treatment", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return correct variants") {
        Context ctx(config, data);
        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.treatment("exp_test_not_eligible") == 0);
        REQUIRE(ctx.treatment("exp_test_fullon") == 2);
        REQUIRE(ctx.treatment("exp_test_custom_fields") == 1);
    }

    SECTION("should queue exposures") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);

        ctx.treatment("exp_test_abc");
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should queue exposures only once") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should queue exposure on unknown experiment") {
        Context ctx(config, data);
        REQUIRE(ctx.treatment("not_found") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.treatment("not_found");
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should return 0 for not-eligible experiment") {
        Context ctx(config, data);
        REQUIRE(ctx.treatment("exp_test_not_eligible") == 0);
    }

    SECTION("full-on variant should return full-on value") {
        Context ctx(config, data);
        REQUIRE(ctx.treatment("exp_test_fullon") == 2);
    }

    SECTION("should emit exposure event") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);

        handler->clear();
        ctx.treatment("exp_test_ab");

        REQUIRE(handler->count_events("exposure") == 1);

        auto& evt = handler->events[0];
        REQUIRE(evt.data["name"] == "exp_test_ab");
        REQUIRE(evt.data["variant"] == 1);
        REQUIRE(evt.data["assigned"] == true);
        REQUIRE(evt.data["eligible"] == true);
        REQUIRE(evt.data["overridden"] == false);
        REQUIRE(evt.data["fullOn"] == false);
        REQUIRE(evt.data["custom"] == false);
        REQUIRE(evt.data["audienceMismatch"] == false);
    }

    SECTION("should not emit exposure event on second call") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);

        handler->clear();
        ctx.treatment("exp_test_ab");
        REQUIRE(handler->count_events("exposure") == 1);

        handler->clear();
        ctx.treatment("exp_test_ab");
        REQUIRE(handler->count_events("exposure") == 0);
    }

    SECTION("should throw after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.treatment("exp_test_ab"), ContextFinalizedException);
    }
}

TEST_CASE("Context peek", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return correct variants without queuing exposures") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        REQUIRE(ctx.peek("exp_test_ab") == 1);
        REQUIRE(ctx.peek("exp_test_abc") == 2);
        REQUIRE(ctx.peek("exp_test_not_eligible") == 0);
        REQUIRE(ctx.peek("exp_test_fullon") == 2);
        REQUIRE(ctx.peek("exp_test_custom_fields") == 1);

        REQUIRE(ctx.pending() == 0);
    }

    SECTION("should return override variant") {
        Context ctx(config, data);
        ctx.set_override("exp_test_ab", 5);
        ctx.set_override("not_found", 3);

        REQUIRE(ctx.peek("exp_test_ab") == 5);
        REQUIRE(ctx.peek("not_found") == 3);
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("treatment after peek should still queue exposure") {
        Context ctx(config, data);
        ctx.peek("exp_test_ab");
        REQUIRE(ctx.pending() == 0);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);
    }
}

TEST_CASE("Context audience mismatch", "[context]") {
    auto config = make_test_config();

    SECTION("non-strict mode: should return assigned variant on audience mismatch") {
        auto data = make_audience_data();
        Context ctx(config, data);
        REQUIRE(ctx.peek("exp_test_ab") == 1);
    }

    SECTION("strict mode: should return control variant on audience mismatch") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);
        REQUIRE(ctx.peek("exp_test_ab") == 0);
    }

    SECTION("strict mode: should return assigned variant when audience matches") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);
        ctx.set_attribute("age", 25);
        REQUIRE(ctx.peek("exp_test_ab") == 1);
    }

    SECTION("non-strict mode: audience match should still return assigned variant") {
        auto data = make_audience_data();
        Context ctx(config, data);
        ctx.set_attribute("age", 25);
        REQUIRE(ctx.peek("exp_test_ab") == 1);
    }
}

TEST_CASE("Context audience re-evaluation", "[context]") {
    auto config = make_test_config();

    SECTION("strict mode: should re-evaluate when attributes change") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("non-strict mode: should re-evaluate when attributes change") {
        auto data = make_audience_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should not re-evaluate when no new attributes set") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        ctx.set_attribute("age", 15);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should not re-evaluate for experiments without audience filter") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should not invalidate cache when audience result unchanged") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        ctx.set_attribute("age", 15);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 18);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should re-evaluate from mismatch to match in strict mode") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 30);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should not re-evaluate when attribute set before assignment") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 1);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should re-evaluate when attribute set after assignment") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should update attrs_seq after checking unchanged audience") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        ctx.set_attribute("age", 15);
        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 16);
        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.set_attribute("age", 17);
        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);

        REQUIRE(ctx.treatment("exp_test_ab") == 0);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("peek: should re-evaluate in strict mode") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);

        REQUIRE(ctx.peek("exp_test_ab") == 0);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.peek("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("peek: should re-evaluate in non-strict mode") {
        auto data = make_audience_data();
        Context ctx(config, data);

        REQUIRE(ctx.peek("exp_test_ab") == 1);

        ctx.set_attribute("age", 25);

        REQUIRE(ctx.peek("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 0);
    }
}

TEST_CASE("Context variables", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return variable value for assigned variant") {
        Context ctx(config, data);
        REQUIRE(ctx.variable_value("banner.border", 0) == 1);
        REQUIRE(ctx.variable_value("banner.size", "small") == "large");
        REQUIRE(ctx.variable_value("button.color", "green") == "red");
    }

    SECTION("should return default value for unknown variable") {
        Context ctx(config, data);
        REQUIRE(ctx.variable_value("not.found", 17) == 17);
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("should queue exposures") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        ctx.variable_value("banner.border", 0);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should return default for not-eligible experiment variable") {
        Context ctx(config, data);
        REQUIRE(ctx.variable_value("card.width", "100%") == "100%");
    }

    SECTION("should return full-on experiment variable value") {
        Context ctx(config, data);
        REQUIRE(ctx.variable_value("submit.color", "white") == "blue");
        REQUIRE(ctx.variable_value("submit.shape", "square") == "rect");
    }

    SECTION("should return default for audience strict mismatch") {
        auto strict_data = make_audience_strict_data();
        Context ctx(config, strict_data);
        REQUIRE(ctx.variable_value("banner.size", "small") == "small");
    }

    SECTION("should return variable value when overridden") {
        auto strict_data = make_audience_strict_data();
        Context ctx(config, strict_data);

        ctx.set_override("exp_test_ab", 0);

        REQUIRE(ctx.variable_value("banner.size", 17) == "tiny");
    }

    SECTION("should return default for unknown override variant") {
        Context ctx(config, data);
        ctx.set_override("exp_test_ab", 11);
        REQUIRE(ctx.variable_value("banner.size", 17) == 17);
    }

    SECTION("should throw after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.variable_value("banner.size", 0), ContextFinalizedException);
    }
}

TEST_CASE("Context peek variable value", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should not queue exposures") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        REQUIRE(ctx.peek_variable_value("banner.border", 0) == 1);
        REQUIRE(ctx.peek_variable_value("banner.size", "small") == "large");
        REQUIRE(ctx.peek_variable_value("button.color", "green") == "red");

        REQUIRE(ctx.pending() == 0);
    }

    SECTION("should return default for not-eligible experiment") {
        Context ctx(config, data);
        REQUIRE(ctx.peek_variable_value("card.width", "100%") == "100%");
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("should return default for audience strict mismatch") {
        auto strict_data = make_audience_strict_data();
        Context ctx(config, strict_data);
        REQUIRE(ctx.peek_variable_value("banner.size", "small") == "small");
    }

    SECTION("should return variable value when overridden") {
        auto strict_data = make_audience_strict_data();
        Context ctx(config, strict_data);

        ctx.set_override("exp_test_ab", 0);
        REQUIRE(ctx.peek_variable_value("banner.size", 17) == "tiny");
    }

    SECTION("should return default for unknown override variant") {
        Context ctx(config, data);
        ctx.set_override("exp_test_ab", 11);
        REQUIRE(ctx.peek_variable_value("banner.size", 17) == 17);
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("variable_value after peek_variable_value should queue exposure") {
        Context ctx(config, data);
        ctx.peek_variable_value("banner.border", 0);
        REQUIRE(ctx.pending() == 0);

        ctx.variable_value("banner.border", 0);
        REQUIRE(ctx.pending() == 1);
    }
}

TEST_CASE("Context variable keys", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return all variable keys") {
        Context ctx(config, data);
        auto keys = ctx.variable_keys();
        REQUIRE(keys.count("banner.border") == 1);
        REQUIRE(keys.count("banner.size") == 1);
        REQUIRE(keys.count("button.color") == 1);
        REQUIRE(keys.count("card.width") == 1);
        REQUIRE(keys.count("submit.color") == 1);
        REQUIRE(keys.count("submit.shape") == 1);
        REQUIRE(keys.count("submit.size") == 1);

        REQUIRE(keys["banner.border"] == std::vector<std::string>{"exp_test_ab"});
        REQUIRE(keys["button.color"] == std::vector<std::string>{"exp_test_abc"});
    }
}

TEST_CASE("Missing optional model fields clear reused values", "[models]") {
    ExperimentVariant variant{"old", {{"color", "red"}}};
    from_json(nlohmann::json{{"name", "new"}}, variant);
    REQUIRE(variant.config.is_null());

    GoalAchievement goal{"old", 1, {{"amount", 10}}};
    from_json(nlohmann::json{{"name", "new"}, {"achievedAt", 2}}, goal);
    REQUIRE(goal.properties.is_null());

    Attribute attribute{"old", "stale", 1};
    from_json(nlohmann::json{{"name", "new"}, {"setAt", 2}}, attribute);
    REQUIRE(attribute.value.is_null());
}

TEST_CASE("Malformed variant config emits a diagnostic and keeps defaults", "[context]") {
    auto data = make_test_data();
    data.experiments[0].variants[0].config = "{";
    auto handler = std::make_shared<MockEventHandler>();
    Context ctx(make_test_config(), data, handler);
    REQUIRE(ctx.is_ready());
    REQUIRE_FALSE(ctx.is_failed());
    REQUIRE(handler->count_events("error") == 1);
    REQUIRE(handler->events[0].data["message"] ==
            "Failed to parse variant config for " + data.experiments[0].name);
    REQUIRE(handler->events[0].data["error"].is_string());
    REQUIRE(handler->events[0].data["variant"] == 0);
}

TEST_CASE("Variant numeric overflow stays isolated in both construction modes", "[context]") {
    auto data = make_test_data();
    data.experiments[0].variants[0].config = R"({"threshold":1e400})";
    data.experiments[1].fullOnVariant = 1;
    auto handler = std::make_shared<MockEventHandler>();
    std::unique_ptr<Context> ctx;

    SECTION("synchronous data") {
        ctx = std::make_unique<Context>(make_test_config(), data, handler);
    }
    SECTION("future-backed data") {
        std::promise<ContextData> promise;
        promise.set_value(data);
        ctx = std::make_unique<Context>(make_test_config(), promise.get_future(), handler);
        ctx->wait_until_ready();
    }

    REQUIRE(ctx->is_ready());
    REQUIRE_FALSE(ctx->is_failed());
    REQUIRE(ctx->experiments().size() == data.experiments.size());
    REQUIRE(ctx->treatment(data.experiments[1].name) == 1);
    REQUIRE(ctx->peek_variable_value("threshold", "default") == "default");
    REQUIRE(handler->count_events("error") == 1);
    REQUIRE(handler->events[0].data["message"] ==
            "Failed to parse variant config for " + data.experiments[0].name);
    REQUIRE(handler->events[0].data["error"].get<std::string>().find("overflow") != std::string::npos);
    REQUIRE(handler->events[0].data["variant"] == 0);
}

TEST_CASE("Context overrides", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should override variant") {
        Context ctx(config, data);
        ctx.set_override("exp_test_ab", 5);

        REQUIRE(ctx.treatment("exp_test_ab") == 5);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should override unknown experiment") {
        Context ctx(config, data);
        ctx.set_override("not_found", 3);

        REQUIRE(ctx.treatment("not_found") == 3);
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should override multiple experiments") {
        Context ctx(config, data);
        ctx.set_overrides({{"exp_test_ab", 3}, {"exp_test_abc", 4}});
        REQUIRE(ctx.peek("exp_test_ab") == 3);
        REQUIRE(ctx.peek("exp_test_abc") == 4);
    }

    SECTION("exposure should reflect override") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);

        ctx.set_override("exp_test_ab", 5);
        handler->clear();
        ctx.treatment("exp_test_ab");

        REQUIRE(handler->count_events("exposure") == 1);
        auto& evt = handler->events[0];
        REQUIRE(evt.data["variant"] == 5);
        REQUIRE(evt.data["overridden"] == true);
    }
}

TEST_CASE("Context custom assignments", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should use custom assignment") {
        Context ctx(config, data);
        ctx.set_custom_assignment("exp_test_ab", 3);
        REQUIRE(ctx.peek("exp_test_ab") == 3);
    }

    SECTION("should set multiple custom assignments") {
        Context ctx(config, data);
        ctx.set_custom_assignments({{"exp_test_ab", 3}, {"exp_test_abc", 4}});
        REQUIRE(ctx.peek("exp_test_ab") == 3);
        REQUIRE(ctx.peek("exp_test_abc") == 4);
    }

    SECTION("should allow custom assignments after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_NOTHROW(ctx.set_custom_assignment("exp_test_ab", 1));
        REQUIRE_NOTHROW(ctx.set_custom_assignments({{"exp_test_ab", 2}, {"exp_test_abc", 1}}));
        REQUIRE(ctx.is_finalized());
    }
}

TEST_CASE("Context track", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should queue goals") {
        Context ctx(config, data);
        REQUIRE(ctx.pending() == 0);

        ctx.track("goal1", {{"amount", 125}, {"hours", 245}});
        REQUIRE(ctx.pending() == 1);

        ctx.track("goal2", {{"tries", 7}});
        REQUIRE(ctx.pending() == 2);

        ctx.track("goal2", {{"tests", 12}});
        REQUIRE(ctx.pending() == 3);
    }

    SECTION("should emit goal event") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);

        handler->clear();
        ctx.track("goal1", {{"amount", 125}});
        REQUIRE(handler->count_events("goal") == 1);
    }

    SECTION("should preserve all property types") {
        Context ctx(config, data);
        ctx.track("goal1", {
            {"amount", 125},
            {"name", "test"},
            {"flag", true},
            {"count", 7}
        });

        auto event = ctx.publish();
        REQUIRE(event.goals.size() == 1);
        REQUIRE(event.goals[0].properties.contains("amount"));
        REQUIRE(event.goals[0].properties.contains("count"));
        REQUIRE(event.goals[0].properties.contains("name"));
        REQUIRE(event.goals[0].properties.contains("flag"));
        REQUIRE(event.goals[0].properties["amount"] == 125);
        REQUIRE(event.goals[0].properties["name"] == "test");
        REQUIRE(event.goals[0].properties["flag"] == true);
    }

    SECTION("should handle empty properties") {
        Context ctx(config, data);
        ctx.track("goal1");
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("should throw after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.track("goal1", {{"amount", 125}}), ContextFinalizedException);
    }

    SECTION("rejects properties that are not an object") {
        auto config = make_test_config();
        Context ctx(config, data);

        for (const auto& bad : {nlohmann::json(1), nlohmann::json("str"), nlohmann::json::array({1})}) {
            REQUIRE_THROWS_WITH(ctx.track("goal1", bad),
                                "Goal 'goal1' properties must be of type object.");
        }
        REQUIRE(ctx.pending() == 0);
    }
}

TEST_CASE("Context publish", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return empty when queue is empty") {
        Context ctx(config, data);
        auto event = ctx.publish();
        REQUIRE(event.units.empty());
        REQUIRE(event.exposures.empty());
        REQUIRE(event.goals.empty());
    }

    SECTION("should collect exposures and goals") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        ctx.track("goal1", {{"amount", 125}});

        auto event = ctx.publish();

        REQUIRE(event.hashed == true);
        REQUIRE(event.units.size() == 2);
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.goals.size() == 1);
        REQUIRE(event.publishedAt > 0);
    }

    SECTION("should hash units") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.hashed == true);

        for (const auto& u : event.units) {
            REQUIRE_FALSE(u.uid.empty());
            auto original = ctx.get_unit(u.type);
            REQUIRE(original.has_value());
            REQUIRE(u.uid != original.value());
        }
    }

    SECTION("should clear queues after publish") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        ctx.track("goal1", {{"amount", 125}});

        REQUIRE(ctx.pending() == 2);
        ctx.publish();
        REQUIRE(ctx.pending() == 0);
    }

    SECTION("should include attributes") {
        Context ctx(config, data);
        ctx.set_attribute("attr1", "value1");
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.attributes.size() == 1);
        REQUIRE(event.attributes[0].name == "attr1");
        REQUIRE(event.attributes[0].value == "value1");
    }

    SECTION("should emit publish event") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);
        ctx.treatment("exp_test_ab");

        handler->clear();
        ctx.publish();
        REQUIRE(handler->count_events("publish") == 1);
    }

    SECTION("exposure data should be correct") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        ctx.treatment("exp_test_not_eligible");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 2);

        auto& exp1 = event.exposures[0];
        REQUIRE(exp1.id == 1);
        REQUIRE(exp1.name == "exp_test_ab");
        REQUIRE(exp1.unit == "session_id");
        REQUIRE(exp1.variant == 1);
        REQUIRE(exp1.assigned == true);
        REQUIRE(exp1.eligible == true);
        REQUIRE(exp1.overridden == false);
        REQUIRE(exp1.fullOn == false);
        REQUIRE(exp1.custom == false);
        REQUIRE(exp1.audienceMismatch == false);

        auto& exp2 = event.exposures[1];
        REQUIRE(exp2.id == 3);
        REQUIRE(exp2.name == "exp_test_not_eligible");
        REQUIRE(exp2.unit == "user_id");
        REQUIRE(exp2.variant == 0);
        REQUIRE(exp2.assigned == true);
        REQUIRE(exp2.eligible == false);
    }

    SECTION("full-on exposure should have fullOn flag") {
        Context ctx(config, data);
        ctx.treatment("exp_test_fullon");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].fullOn == true);
        REQUIRE(event.exposures[0].variant == 2);
    }

    SECTION("override exposure data") {
        Context ctx(config, data);
        ctx.set_override("exp_test_ab", 5);
        ctx.set_override("not_found", 3);
        ctx.treatment("exp_test_ab");
        ctx.treatment("not_found");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 2);

        REQUIRE(event.exposures[0].id == 1);
        REQUIRE(event.exposures[0].overridden == true);
        REQUIRE(event.exposures[0].variant == 5);
        REQUIRE(event.exposures[0].assigned == false);
        REQUIRE(event.exposures[0].unit == "session_id");

        REQUIRE(event.exposures[1].id == 0);
        REQUIRE(event.exposures[1].overridden == true);
        REQUIRE(event.exposures[1].variant == 3);
        REQUIRE(event.exposures[1].assigned == false);
        REQUIRE(event.exposures[1].unit == "");
    }
}

TEST_CASE("Context finalize", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should publish pending events and seal") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);

        auto result = ctx.finalize();
        REQUIRE(ctx.is_finalized());
        REQUIRE(ctx.pending() == 0);
        REQUIRE(result.exposures.size() == 1);
    }

    SECTION("should return empty on second finalize") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");

        ctx.finalize();
        auto result = ctx.finalize();
        REQUIRE(result.units.empty());
    }

    SECTION("should emit finalize event") {
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);
        ctx.treatment("exp_test_ab");

        handler->clear();
        ctx.finalize();
        REQUIRE(handler->count_events("finalize") == 1);
    }

    SECTION("should seal context") {
        Context ctx(config, data);
        ctx.finalize();

        REQUIRE_THROWS_AS(ctx.treatment("exp_test_ab"), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.peek("exp_test_ab"), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.track("goal1"), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.set_attribute("a", 1), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.set_unit("test", "test"), ContextFinalizedException);
        REQUIRE_NOTHROW(ctx.set_custom_assignment("exp", 1));
        REQUIRE_THROWS_AS(ctx.variable_value("key", 0), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.peek_variable_value("key", 0), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.variable_keys(), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.publish(), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.custom_field_keys(), ContextFinalizedException);
    }
}

TEST_CASE("Context refresh/cache invalidation", "[context]") {
    auto config = make_test_config();

    SECTION("should clear cache for started experiment") {
        auto data = make_test_data();
        auto refresh_data = make_refresh_data();
        auto provider = std::make_shared<MockDataProvider>(refresh_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        REQUIRE(ctx.treatment("exp_test_new") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_new") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache for stopped experiment") {
        auto data = make_test_data();
        auto stopped_data = data;
        stopped_data.experiments.erase(
            std::remove_if(stopped_data.experiments.begin(),
                           stopped_data.experiments.end(),
                           [](const ExperimentData& e) { return e.name == "exp_test_abc"; }),
            stopped_data.experiments.end());
        auto provider = std::make_shared<MockDataProvider>(stopped_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when experiment ID changes") {
        auto data = make_test_data();
        auto changed_data = data;
        for (auto& exp : changed_data.experiments) {
            if (exp.name == "exp_test_abc") {
                exp.id = 11;
                exp.trafficSeedHi = 54870830;
                exp.trafficSeedLo = 398724581;
                exp.seedHi = 77498863;
                exp.seedLo = 34737352;
            }
        }
        auto provider = std::make_shared<MockDataProvider>(changed_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when full-on changes") {
        auto data = make_test_data();
        auto fullon_data = data;
        for (auto& exp : fullon_data.experiments) {
            if (exp.name == "exp_test_abc") {
                exp.fullOnVariant = 1;
            }
        }
        auto provider = std::make_shared<MockDataProvider>(fullon_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when traffic split changes") {
        auto data = make_test_data();
        auto split_data = data;
        for (auto& exp : split_data.experiments) {
            if (exp.name == "exp_test_not_eligible") {
                exp.trafficSplit = {0.0, 1.0};
            }
        }
        auto provider = std::make_shared<MockDataProvider>(split_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        REQUIRE(ctx.treatment("exp_test_not_eligible") == 0);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_not_eligible") == 2);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should not re-queue exposures after refresh when experiments unchanged") {
        auto data = make_test_data();
        auto refresh_data = make_refresh_data();
        auto provider = std::make_shared<MockDataProvider>(refresh_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }
        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));

        ctx.refresh();

        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));

        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }
        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));
    }

    SECTION("should keep overrides after refresh") {
        auto data = make_test_data();
        auto refresh_data = make_refresh_data();
        auto provider = std::make_shared<MockDataProvider>(refresh_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        ctx.set_override("not_found", 3);
        REQUIRE(ctx.peek("not_found") == 3);

        ctx.refresh();

        REQUIRE(ctx.peek("not_found") == 3);
    }

    SECTION("should keep custom assignments after refresh") {
        auto data = make_test_data();
        auto refresh_data = make_refresh_data();
        auto provider = std::make_shared<MockDataProvider>(refresh_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        ctx.set_custom_assignment("exp_test_ab", 3);
        REQUIRE(ctx.peek("exp_test_ab") == 3);

        ctx.refresh();

        REQUIRE(ctx.peek("exp_test_ab") == 3);
    }

    SECTION("should throw after finalize") {
        auto data = make_test_data();
        auto provider = std::make_shared<MockDataProvider>(data);
        Context ctx(config, data, nullptr, nullptr, provider);
        ctx.finalize();

        REQUIRE_THROWS_AS(ctx.refresh(), ContextFinalizedException);
    }

    SECTION("should emit refresh event") {
        auto data = make_test_data();
        auto handler = std::make_shared<MockEventHandler>();
        auto provider = std::make_shared<MockDataProvider>(data);
        Context ctx(config, data, handler, nullptr, provider);

        handler->clear();
        ctx.refresh();
        REQUIRE(handler->count_events("refresh") == 1);
    }

    SECTION("should keep overridden assignment after refresh changes the experiment id") {
        auto data = make_test_data();
        auto changed_data = data;
        for (auto& exp : changed_data.experiments) {
            if (exp.name == "exp_test_ab") {
                exp.id = 99;
            }
        }
        auto provider = std::make_shared<MockDataProvider>(changed_data);
        Context ctx(config, data, nullptr, nullptr, provider);

        ctx.set_override("exp_test_ab", 3);
        REQUIRE(ctx.treatment("exp_test_ab") == 3);
        REQUIRE(ctx.pending() == 1);

        ctx.refresh();

        REQUIRE(ctx.treatment("exp_test_ab") == 3);
        // Sticky like JS (context.ts:670-686): the override's variant is
        // unchanged, so no second exposure is queued.
        REQUIRE(ctx.pending() == 1);
    }
}

TEST_CASE("Context units", "[context]") {
    auto data = make_test_data();

    SECTION("should set and get unit") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        ctx.set_unit("session_id", "abc123");
        REQUIRE(ctx.get_unit("session_id").value() == "abc123");
    }

    SECTION("should return nullopt for unknown unit") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        REQUIRE_FALSE(ctx.get_unit("unknown").has_value());
    }

    SECTION("should throw on blank uid") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        REQUIRE_THROWS(ctx.set_unit("session_id", ""));
    }

    SECTION("should throw on duplicate unit with different value") {
        auto config = make_test_config();
        Context ctx(config, data);

        REQUIRE_THROWS(ctx.set_unit("session_id", "new_id"));
    }

    SECTION("should not throw on duplicate unit with same value") {
        auto config = make_test_config();
        Context ctx(config, data);

        REQUIRE_NOTHROW(ctx.set_unit("session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec"));
    }

    SECTION("should set multiple units") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        ctx.set_units({{"session_id", "abc"}, {"user_id", "123"}});
        REQUIRE(ctx.get_units().size() == 2);
    }

    SECTION("should throw after finalize") {
        auto config = make_test_config();
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.set_unit("test", "test"), ContextFinalizedException);
    }

    SECTION("keeps the type of numeric uids") {
        ContextConfig config;
        config.publish_delay = -1;
        config.units = {{"user_id", 125}, {"float_id", 125.75}};
        Context ctx(config, data);

        REQUIRE(ctx.get_unit("user_id").value() == nlohmann::json(125));
        REQUIRE(ctx.get_unit("user_id").value().is_number_integer());
        REQUIRE(ctx.get_unit("float_id").value() == nlohmann::json(125.75));
        REQUIRE(ctx.get_units().at("float_id").is_number_float());
    }

    SECTION("hashes a fractional uid the way JS does") {
        ContextConfig config;
        config.publish_delay = -1;
        config.units = {{"float_id", 125.75}};
        Context ctx(config, data);
        ctx.track("goal1");

        auto event = ctx.publish();
        REQUIRE(event.units.size() == 1);
        REQUIRE(event.units[0].uid == hash_unit(std::string("126")));
    }

    SECTION("the same JS Number keeps the first uid and its hash") {
        ContextConfig config;
        config.publish_delay = -1;
        config.units = {{"user_id", 1000000000000000129LL}};
        Context ctx(config, data);
        ctx.track("goal1");
        auto first = ctx.publish();

        REQUIRE_NOTHROW(ctx.set_unit("user_id", 1000000000000000128.0));
        REQUIRE(ctx.get_unit("user_id").value().is_number_integer());
        REQUIRE_THROWS(ctx.set_unit("user_id", "1000000000000000128"));
        ctx.track("goal1");
        auto second = ctx.publish();
        REQUIRE(first.units[0].uid == hash_unit(std::string("1000000000000000128")));
        REQUIRE(second.units[0].uid == first.units[0].uid);
    }

    SECTION("trims string uids and rejects whitespace-only ones") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        REQUIRE_THROWS_WITH(ctx.set_unit("session_id", "   "),
                            "Unit 'session_id' UID must not be blank.");
        ctx.set_unit("session_id", "  abc ");
        REQUIRE(ctx.get_unit("session_id").value() == "abc");
    }

    SECTION("rejects uids that are not strings or numbers") {
        ContextConfig config;
        config.publish_delay = -1;
        Context ctx(config, data);

        REQUIRE_THROWS_WITH(ctx.set_unit("session_id", nlohmann::json::object()),
                            "Unit 'session_id' must be a string or a number.");
        REQUIRE_THROWS_WITH(ctx.set_unit("session_id", true),
                            "Unit 'session_id' must be a string or a number.");
    }
}

TEST_CASE("Context attributes", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should set and get attribute") {
        Context ctx(config, data);
        ctx.set_attribute("attr1", "value1");
        REQUIRE(ctx.get_attribute("attr1") == "value1");
    }

    SECTION("should get last set value") {
        Context ctx(config, data);
        ctx.set_attribute("attr1", "value1");
        ctx.set_attribute("attr1", "value2");
        REQUIRE(ctx.get_attribute("attr1") == "value2");
    }

    SECTION("should set multiple attributes") {
        Context ctx(config, data);
        ctx.set_attributes({{"attr1", "value1"}, {"attr2", 15}});
        auto attrs = ctx.get_attributes();
        REQUIRE(attrs.size() == 2);
        REQUIRE(attrs["attr1"] == "value1");
        REQUIRE(attrs["attr2"] == 15);
    }

    SECTION("should return null for unknown attribute") {
        Context ctx(config, data);
        REQUIRE(ctx.get_attribute("unknown").is_null());
    }

    SECTION("should throw after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.set_attribute("a", 1), ContextFinalizedException);
    }
}

TEST_CASE("Context custom fields", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should return string custom field") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "country") == "US,PT,ES");
    }

    SECTION("should return text custom field") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "text_field") == "hello text");
    }

    SECTION("should return number custom field") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "number_field") == 123.0);
    }

    SECTION("should return boolean custom field") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "boolean_field") == true);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "false_boolean_field") == false);
    }

    SECTION("should return null for unknown field") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("exp_test_custom_fields", "unknown").is_null());
    }

    SECTION("should return null for unknown experiment") {
        Context ctx(config, data);
        REQUIRE(ctx.custom_field_value("not_found", "country").is_null());
    }

    SECTION("should list custom field keys") {
        Context ctx(config, data);
        auto keys = ctx.custom_field_keys();
        REQUIRE_FALSE(keys.empty());
    }
}

TEST_CASE("Context exposure de-duplication", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("treatment called twice queues only one exposure") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
    }

    SECTION("treatment on different experiments queues multiple exposures") {
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");
        ctx.treatment("exp_test_abc");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 2);
    }

    SECTION("peek then treatment queues exactly one exposure") {
        Context ctx(config, data);
        ctx.peek("exp_test_ab");
        REQUIRE(ctx.pending() == 0);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("all experiments queued once") {
        Context ctx(config, data);
        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }

        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));

        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }

        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));
    }
}

TEST_CASE("Context audience mismatch exposure data", "[context]") {
    auto config = make_test_config();

    SECTION("non-strict: exposure should have audienceMismatch true") {
        auto data = make_audience_data();
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].audienceMismatch == true);
        REQUIRE(event.exposures[0].variant == 1);
        REQUIRE(event.exposures[0].assigned == true);
    }

    SECTION("non-strict: exposure should have audienceMismatch false when matched") {
        auto data = make_audience_data();
        Context ctx(config, data);
        ctx.set_attribute("age", 21);
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].audienceMismatch == false);
        REQUIRE(event.exposures[0].variant == 1);
    }

    SECTION("strict: exposure should have audienceMismatch true and control variant") {
        auto data = make_audience_strict_data();
        Context ctx(config, data);
        ctx.treatment("exp_test_ab");

        auto event = ctx.publish();
        REQUIRE(event.exposures.size() == 1);
        REQUIRE(event.exposures[0].audienceMismatch == true);
        REQUIRE(event.exposures[0].variant == 0);
        REQUIRE(event.exposures[0].assigned == false);
    }
}

TEST_CASE("Context config from constructor", "[context]") {
    auto data = make_test_data();

    SECTION("should apply units from config") {
        ContextConfig config;
        config.units = {{"session_id", "abc123"}};
        Context ctx(config, data);
        REQUIRE(ctx.get_unit("session_id").value() == "abc123");
    }

    SECTION("should apply overrides from config") {
        ContextConfig config;
        config.overrides = {{"exp_test_ab", 5}};
        config.units = {{"session_id", "abc123"}};
        Context ctx(config, data);
        REQUIRE(ctx.peek("exp_test_ab") == 5);
    }

    SECTION("should apply custom assignments from config") {
        auto config = make_test_config();
        config.custom_assignments = {{"exp_test_ab", 3}};
        Context ctx(config, data);
        REQUIRE(ctx.peek("exp_test_ab") == 3);
    }
}

TEST_CASE("Context conflicting variable keys", "[context]") {
    auto data = make_test_data();
    auto config = make_test_config();

    SECTION("should pick lowest experiment id on conflicting key") {
        auto conflict_data = data;
        for (auto& exp : conflict_data.experiments) {
            if (exp.name == "exp_test_ab") {
                exp.id = 99;
                for (auto& v : exp.variants) {
                    if (v.name == "B") {
                        v.config = nlohmann::json(R"({"icon":"arrow"})");
                    }
                }
            }
            if (exp.name == "exp_test_abc") {
                exp.id = 1;
                for (auto& v : exp.variants) {
                    if (v.name == "C") {
                        v.config = nlohmann::json(R"({"icon":"circle"})");
                    }
                }
            }
        }

        Context ctx(config, conflict_data);
        REQUIRE(ctx.peek_variable_value("icon", "square") == "circle");
    }
}

TEST_CASE("Context disjointed audiences", "[context]") {
    auto config = make_test_config();

    SECTION("should resolve variable from matching audience experiment") {
        auto data = make_test_data();
        for (auto& exp : data.experiments) {
            if (exp.name == "exp_test_ab") {
                exp.audienceStrict = true;
                exp.audience = nlohmann::json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
                for (auto& v : exp.variants) {
                    if (v.name == "B") {
                        v.config = nlohmann::json(R"({"icon":"arrow"})");
                    }
                }
            }
            if (exp.name == "exp_test_abc") {
                exp.audienceStrict = true;
                exp.audience = nlohmann::json(R"({"filter":[{"lt":[{"var":"age"},{"value":20}]}]})");
                for (auto& v : exp.variants) {
                    if (v.name == "C") {
                        v.config = nlohmann::json(R"({"icon":"circle"})");
                    }
                }
            }
        }

        Context ctx1(config, data);
        ctx1.set_attribute("age", 20);
        REQUIRE(ctx1.variable_value("icon", "square") == "arrow");

        Context ctx2(config, data);
        ctx2.set_attribute("age", 19);
        REQUIRE(ctx2.variable_value("icon", "square") == "circle");
    }
}

TEST_CASE("custom field reads throw before ready", "[context][notready]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    std::promise<ContextData> promise;
    auto future = promise.get_future();

    Context ctx(config, std::move(future));

    REQUIRE_THROWS_AS(ctx.custom_field_value("exp_test_abc", "country"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.custom_field_value_type("exp_test_abc", "country"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.custom_field_keys(), ContextNotReadyException);
}

TEST_CASE("Overrides remain allowed after finalize", "[context][fix23]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.finalize();

    REQUIRE_NOTHROW(ctx.set_override("exp_test_ab", 1));
    REQUIRE_NOTHROW(ctx.set_overrides({{"exp_test_ab", 2}}));
}

TEST_CASE("experiments() does not throw after finalize", "[context][fix22]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.finalize();

    REQUIRE_NOTHROW(ctx.experiments());
}

TEST_CASE("variable_keys() throws before ready and after finalize", "[context][fix24]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    SECTION("throws before ready") {
        std::promise<ContextData> promise;
        auto future = promise.get_future();
        Context ctx(config, std::move(future));
        REQUIRE_THROWS_AS(ctx.variable_keys(), ContextNotReadyException);
    }

    SECTION("throws after finalize") {
        ContextData data = make_test_data();
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.variable_keys(), ContextFinalizedException);
    }
}

TEST_CASE("custom_field_value/keys throw after finalize", "[context][fix8]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.finalize();

    REQUIRE_THROWS_AS(ctx.custom_field_value("exp_test_abc", "country"), ContextFinalizedException);
    REQUIRE_THROWS_AS(ctx.custom_field_keys(), ContextFinalizedException);
}

TEST_CASE("Fix: redundant data copy removed in init", "[context][fix9]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    REQUIRE(ctx.is_ready());
    REQUIRE(ctx.data().experiments.size() == data.experiments.size());
    REQUIRE(ctx.treatment("exp_test_ab") >= 0);
}

class RecordingPublisher : public ContextPublisher {
public:
    int call_count = 0;
    PublishEvent last_event;
    std::future<void> publish(const PublishEvent& event) override {
        call_count++;
        last_event = event;
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }
};

TEST_CASE("Fix: publish calls event_publisher", "[context][fix36]") {

    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();
    auto publisher = std::make_shared<RecordingPublisher>();

    Context ctx(config, data, nullptr, publisher);
    ctx.treatment("exp_test_ab");

    auto event = ctx.publish();
    REQUIRE(publisher->call_count == 1);
    REQUIRE(!publisher->last_event.exposures.empty());
}

TEST_CASE("Fix: finalize calls event_publisher", "[context][fix36]") {

    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();
    auto publisher = std::make_shared<RecordingPublisher>();

    Context ctx(config, data, nullptr, publisher);
    ctx.track("goal1", {{"amount", 100}});
    ctx.finalize();

    REQUIRE(publisher->call_count == 1);
}

TEST_CASE("Fix: refresh only invalidates changed experiments", "[context][fix20]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    auto handler = std::make_shared<MockEventHandler>();
    auto provider = std::make_shared<MockDataProvider>(data);
    Context ctx(config, data, handler, nullptr, provider);

    int t1 = ctx.treatment("exp_test_ab");
    handler->clear();

    ctx.refresh();
    int t2 = ctx.treatment("exp_test_ab");
    REQUIRE(t1 == t2);

    bool has_exposure_after_refresh = false;
    for (const auto& e : handler->events) {
        if (e.type == "exposure" && e.data["name"] == "exp_test_ab") {
            has_exposure_after_refresh = true;
        }
    }
    REQUIRE_FALSE(has_exposure_after_refresh);
}

TEST_CASE("Fix: refresh invalidates changed experiments", "[context][fix20]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    ContextData new_data = data;
    new_data.experiments[0].iteration = 2;

    auto handler = std::make_shared<MockEventHandler>();
    auto provider = std::make_shared<MockDataProvider>(new_data);
    Context ctx(config, data, handler, nullptr, provider);

    ctx.treatment("exp_test_ab");
    handler->clear();

    ctx.refresh();

    ctx.treatment("exp_test_ab");
    bool has_exposure = false;
    for (const auto& e : handler->events) {
        if (e.type == "exposure" && e.data["name"] == "exp_test_ab") {
            has_exposure = true;
        }
    }
    REQUIRE(has_exposure);
}

TEST_CASE("Fix: noexcept accessors", "[context][fix43]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    REQUIRE(noexcept(ctx.is_failed()));
    REQUIRE(noexcept(ctx.is_finalized()));
    REQUIRE(noexcept(ctx.is_finalizing()));
    REQUIRE(noexcept(ctx.pending()));
}

TEST_CASE("Fix: track preserves non-numeric goal properties", "[context][fix14]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.track("goal1", {{"amount", 125}, {"label", "purchase"}, {"active", true}});

    auto event = ctx.publish();
    REQUIRE(event.goals.size() == 1);
    REQUIRE(event.goals[0].properties["amount"] == 125);
    REQUIRE(event.goals[0].properties["label"] == "purchase");
    REQUIRE(event.goals[0].properties["active"] == true);
}

TEST_CASE("Fix: custom_field_value specific exception handling", "[context][fix46]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    ContextData data;
    ExperimentData exp;
    exp.id = 100;
    exp.name = "exp_custom";
    exp.unitType = "session_id";
    exp.iteration = 1;
    exp.seedHi = 1;
    exp.seedLo = 1;
    exp.split = {0.5, 0.5};
    exp.trafficSeedHi = 1;
    exp.trafficSeedLo = 1;
    exp.trafficSplit = {0.0, 1.0};
    exp.fullOnVariant = 0;

    CustomFieldValue cf_num;
    cf_num.name = "bad_number";
    cf_num.value = "not_a_number";
    cf_num.type = "number";
    exp.customFieldValues.push_back(cf_num);

    CustomFieldValue cf_json;
    cf_json.name = "bad_json";
    cf_json.value = "{invalid json";
    cf_json.type = "json";
    exp.customFieldValues.push_back(cf_json);

    ExperimentVariant v0;
    v0.name = "A";
    v0.config = nullptr;
    exp.variants = {v0};

    data.experiments.push_back(exp);

    Context ctx(config, data);
    REQUIRE(ctx.custom_field_value("exp_custom", "bad_number").is_null());
    REQUIRE(ctx.custom_field_value("exp_custom", "bad_json").is_null());
}

TEST_CASE("ready_error returns empty on success", "[context][readyerror]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    REQUIRE(ctx.ready_error().empty());
}

TEST_CASE("ready_error returns message on failure", "[context][readyerror]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    std::promise<ContextData> p;
    auto f = p.get_future();
    p.set_exception(std::make_exception_ptr(std::runtime_error("load failed")));

    Context ctx(config, std::move(f));
    ctx.wait_until_ready();

    REQUIRE(ctx.is_failed());
    REQUIRE(ctx.is_ready());
    REQUIRE(ctx.ready_error() == "load failed");
}

TEST_CASE("A failed load is ready with empty data", "[context][failed]") {
    auto config = make_test_config();
    std::promise<ContextData> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("load failed")));
    Context ctx(config, promise.get_future());

    // is_ready() itself picks up the failed future, like JS isReady() after the
    // load promise rejects.
    REQUIRE(ctx.is_ready());
    REQUIRE(ctx.is_failed());
    REQUIRE(ctx.data().experiments.empty());
    REQUIRE(ctx.treatment("exp_test_ab") == 0);
}

TEST_CASE("custom_field_value_type returns type string", "[context][customfieldtype]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    REQUIRE(ctx.custom_field_value_type("exp_test_custom_fields", "country") == "string");
    REQUIRE(ctx.custom_field_value_type("exp_test_custom_fields", "text_field") == "text");
    REQUIRE(ctx.custom_field_value_type("exp_test_custom_fields", "number_field") == "number");
    REQUIRE(ctx.custom_field_value_type("exp_test_custom_fields", "boolean_field") == "boolean");
}

TEST_CASE("custom_field_value_type returns nullopt for missing", "[context][customfieldtype]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    REQUIRE_FALSE(ctx.custom_field_value_type("exp_test_custom_fields", "missing").has_value());
    REQUIRE_FALSE(ctx.custom_field_value_type("not_found", "country").has_value());
}

TEST_CASE("close is alias for finalize", "[context][aliases]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    REQUIRE_FALSE(ctx.is_closed());
    ctx.close();
    REQUIRE(ctx.is_closed());
    REQUIRE(ctx.is_finalized());
}

TEST_CASE("is_closed returns same as is_finalized", "[context][aliases]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    REQUIRE(ctx.is_closed() == ctx.is_finalized());
    ctx.finalize();
    REQUIRE(ctx.is_closed() == ctx.is_finalized());
}

TEST_CASE("is_closing returns same as is_finalizing", "[context][aliases]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    REQUIRE(ctx.is_closing() == ctx.is_finalizing());
}

TEST_CASE("error messages use ABsmartly prefix", "[context][errors]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    ctx.finalize();

    try {
        ctx.set_unit("new_unit", "uid");
        FAIL("Expected ContextFinalizedException");
    } catch (const ContextFinalizedException& e) {
        REQUIRE(std::string(e.what()) == "ABsmartly Context is finalized.");
    }
}

TEST_CASE("not ready error message uses ABsmartly prefix", "[context][errors]") {
    ContextNotReadyException ex;
    REQUIRE(std::string(ex.what()) == "ABsmartly Context is not yet ready.");
}

TEST_CASE("unit UID already set error message", "[context][errors]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    ctx.set_unit("new_unit", "uid1");
    try {
        ctx.set_unit("new_unit", "uid2");
        FAIL("Expected exception");
    } catch (const std::exception& e) {
        REQUIRE(std::string(e.what()) == "Unit 'new_unit' UID already set.");
    }
}

TEST_CASE("unit UID blank error message", "[context][errors]") {
    auto data = make_test_data();
    auto config = make_test_config();

    Context ctx(config, data);
    try {
        ctx.set_unit("new_unit", "");
        FAIL("Expected exception");
    } catch (const std::exception& e) {
        REQUIRE(std::string(e.what()) == "Unit 'new_unit' UID must not be blank.");
    }
}

// ---------------------------------------------------------------------------
// Publish failure semantics (cross-SDK scenario 193): a failed publish must not
// lose pending events. They are requeued so a later publish() or finalize()
// can deliver them, and the failure is surfaced to the caller.
// ---------------------------------------------------------------------------

namespace {

// Publisher whose outcome per call is scripted. Calls beyond the script
// succeed. Every event handed to the publisher is recorded.
class ScriptedPublisher : public ContextPublisher {
public:
    enum class Outcome { Succeed, FailFuture, ThrowSync };

    explicit ScriptedPublisher(std::vector<Outcome> script = {}) : script_(std::move(script)) {}

    std::future<void> publish(const PublishEvent& event) override {
        received.push_back(event);
        Outcome outcome = next_ < script_.size() ? script_[next_] : Outcome::Succeed;
        ++next_;

        if (outcome == Outcome::ThrowSync) {
            throw std::runtime_error("Publish failed");
        }

        std::promise<void> p;
        if (outcome == Outcome::FailFuture) {
            p.set_exception(std::make_exception_ptr(std::runtime_error("Publish failed")));
        } else {
            p.set_value();
        }
        return p.get_future();
    }

    std::vector<PublishEvent> received;

private:
    std::vector<Outcome> script_;
    size_t next_ = 0;
};

using Outcome = ScriptedPublisher::Outcome;

} // namespace

TEST_CASE("Publish failure preserves pending events", "[context][publishfail]") {
    auto data = make_test_data();
    auto config = make_test_config();
    auto handler = std::make_shared<MockEventHandler>();

    SECTION("failed publish future rethrows and keeps pending count") {
        auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::FailFuture});
        Context ctx(config, data, handler, publisher);
        ctx.treatment("exp_test_ab");
        ctx.track("goal1", {{"amount", 125}});
        REQUIRE(ctx.pending() == 2);

        handler->clear();
        REQUIRE_THROWS_WITH(ctx.publish(), "Publish failed");

        REQUIRE(publisher->received.size() == 1);
        REQUIRE(ctx.pending() == 2);
        REQUIRE(handler->count_events("publish") == 0);
        REQUIRE(handler->count_events("error") == 1);
        REQUIRE(handler->events.back().data["message"] == "Publish failed");
    }

    SECTION("publisher throwing synchronously keeps pending count") {
        auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::ThrowSync});
        Context ctx(config, data, handler, publisher);
        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);

        handler->clear();
        REQUIRE_THROWS_WITH(ctx.publish(), "Publish failed");

        REQUIRE(ctx.pending() == 1);
        REQUIRE(handler->count_events("publish") == 0);
        REQUIRE(handler->count_events("error") == 1);
    }

    SECTION("scenario 193: exposure, failed publish, pending still 1") {
        ContextConfig cfg;
        cfg.publish_delay = -1;
        cfg.refresh_period = 0;
        cfg.units = {{"session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec"}};

        auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::FailFuture});
        Context ctx(cfg, data, handler, publisher);
        REQUIRE(ctx.treatment("exp_test_ab") == 1);
        REQUIRE(ctx.pending() == 1);

        REQUIRE_THROWS_WITH(ctx.publish(), "Publish failed");
        REQUIRE(ctx.pending() == 1);
    }

    SECTION("requeued exposure is not duplicated by a repeat treatment call") {
        auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::FailFuture});
        Context ctx(config, data, handler, publisher);
        ctx.treatment("exp_test_ab");
        REQUIRE_THROWS(ctx.publish());

        ctx.treatment("exp_test_ab");
        REQUIRE(ctx.pending() == 1);
    }
}

TEST_CASE("Publish retry after failure delivers the original events", "[context][publishfail]") {
    auto data = make_test_data();
    auto config = make_test_config();
    auto handler = std::make_shared<MockEventHandler>();
    auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::FailFuture});

    Context ctx(config, data, handler, publisher);
    ctx.treatment("exp_test_ab");
    ctx.track("goal1", {{"amount", 125}});
    REQUIRE_THROWS_WITH(ctx.publish(), "Publish failed");

    const PublishEvent failed = publisher->received.at(0);
    REQUIRE(failed.exposures.size() == 1);
    REQUIRE(failed.goals.size() == 1);

    ctx.track("goal2");
    REQUIRE(ctx.pending() == 3);

    handler->clear();
    auto event = ctx.publish();

    REQUIRE(publisher->received.size() == 2);
    REQUIRE(ctx.pending() == 0);
    REQUIRE(handler->count_events("publish") == 1);
    REQUIRE(handler->count_events("error") == 0);

    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].name == "exp_test_ab");
    REQUIRE(event.exposures[0].exposedAt == failed.exposures[0].exposedAt);

    REQUIRE(event.goals.size() == 2);
    REQUIRE(event.goals[0].name == "goal1");
    REQUIRE(event.goals[0].achievedAt == failed.goals[0].achievedAt);
    REQUIRE(event.goals[0].properties == failed.goals[0].properties);
    REQUIRE(event.goals[1].name == "goal2");

    REQUIRE(ctx.publish().units.empty());
    REQUIRE(publisher->received.size() == 2);
}

TEST_CASE("Publish success with a publisher clears pending as before", "[context][publishfail]") {
    auto data = make_test_data();
    auto config = make_test_config();
    auto handler = std::make_shared<MockEventHandler>();
    auto publisher = std::make_shared<ScriptedPublisher>();

    Context ctx(config, data, handler, publisher);
    ctx.treatment("exp_test_ab");
    ctx.track("goal1");

    handler->clear();
    auto event = ctx.publish();

    REQUIRE(publisher->received.size() == 1);
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.goals.size() == 1);
    REQUIRE(ctx.pending() == 0);
    REQUIRE(handler->count_events("publish") == 1);
    REQUIRE(handler->count_events("error") == 0);

    REQUIRE(ctx.publish().units.empty());
    REQUIRE(publisher->received.size() == 1);
}

TEST_CASE("Finalize failure keeps the context open with pending events", "[context][publishfail]") {
    auto data = make_test_data();
    auto config = make_test_config();
    auto handler = std::make_shared<MockEventHandler>();
    auto publisher = std::make_shared<ScriptedPublisher>(std::vector<Outcome>{Outcome::FailFuture});

    Context ctx(config, data, handler, publisher);
    ctx.treatment("exp_test_ab");
    REQUIRE(ctx.pending() == 1);

    handler->clear();
    REQUIRE_THROWS_WITH(ctx.finalize(), "Publish failed");

    REQUIRE_FALSE(ctx.is_finalized());
    REQUIRE_FALSE(ctx.is_finalizing());
    REQUIRE(ctx.pending() == 1);
    REQUIRE(handler->count_events("finalize") == 0);
    REQUIRE_NOTHROW(ctx.track("goal1"));
    REQUIRE(ctx.pending() == 2);

    auto result = ctx.finalize();

    REQUIRE(ctx.is_finalized());
    REQUIRE(ctx.pending() == 0);
    REQUIRE(result.exposures.size() == 1);
    REQUIRE(result.goals.size() == 1);
    REQUIRE(publisher->received.size() == 2);
    REQUIRE(handler->count_events("finalize") == 1);
}

TEST_CASE("Context refresh keeps overridden assignment when the experiment id changes", "[context][refresh]") {
    auto make_data = [](int id) {
        ContextData d;
        ExperimentData e;
        e.id = id;
        e.name = "example";
        e.unitType = "user_id";
        e.iteration = 1;
        e.split = {0.5, 0.5};
        e.trafficSplit = {0.0, 1.0};
        e.trafficSeedHi = 1;
        e.trafficSeedLo = 2;
        e.seedHi = 3;
        e.seedLo = 4;
        e.variants = {ExperimentVariant{}, ExperimentVariant{}};
        d.experiments.push_back(e);
        return d;
    };
    ContextConfig cfg;
    cfg.units = {{"user_id", "u1"}};
    Context ctx(cfg, make_data(1));
    ctx.set_override("example", 1);
    REQUIRE(ctx.treatment("example") == 1);
    ctx.refresh(make_data(99));
    REQUIRE(ctx.treatment("example") == 1);
    // Sticky like JS (context.ts:670-686) and Java (Context.java:1147): an
    // override stays cached while its variant is unchanged.
    auto ev = ctx.publish();
    REQUIRE(ev.exposures.size() == 1);
    REQUIRE(ev.exposures.back().id == 1);
}

TEST_CASE("Context refresh keeps overridden assignment on same-id variable change", "[context][refresh]") {
    auto make_data = [](const char* color) {
        ContextData d;
        ExperimentData e;
        e.id = 1;
        e.name = "example";
        e.unitType = "user_id";
        e.split = {0.5, 0.5};
        e.trafficSplit = {0.0, 1.0};
        ExperimentVariant v0, v1;
        v0.config = nullptr;
        v1.config = nlohmann::json(std::string("{\"color\":\"") + color + "\"}");
        e.variants = {v0, v1};
        d.experiments.push_back(e);
        return d;
    };
    ContextConfig cfg;
    cfg.units = {{"user_id", "u1"}};
    Context ctx(cfg, make_data("red"));
    ctx.set_override("example", 1);
    REQUIRE(ctx.variable_value("color", nlohmann::json("none")) == "red");
    ctx.refresh(make_data("blue"));
    REQUIRE(ctx.variable_value("color", nlohmann::json("none")) == "red");
}

TEST_CASE("Context unchanged refresh keeps nonzero-iteration override without duplicate exposure", "[context][refresh]") {
    auto make_data = []() {
        ContextData d;
        ExperimentData e;
        e.id = 1;
        e.name = "example";
        e.unitType = "user_id";
        e.iteration = 3;
        e.split = {0.5, 0.5};
        e.trafficSplit = {0.0, 1.0};
        e.variants = {ExperimentVariant{}, ExperimentVariant{}};
        d.experiments.push_back(e);
        return d;
    };
    ContextConfig cfg;
    cfg.units = {{"user_id", "u1"}};
    Context ctx(cfg, make_data());
    ctx.set_override("example", 1);
    REQUIRE(ctx.treatment("example") == 1);
    ctx.refresh(make_data());
    REQUIRE(ctx.treatment("example") == 1);
    ctx.refresh(make_data());
    REQUIRE(ctx.treatment("example") == 1);
    REQUIRE(ctx.publish().exposures.size() == 1);
}

// ---------------------------------------------------------------------------
// Parity with the JS SDK (javascript-sdk main, src/context.ts).
// ---------------------------------------------------------------------------

namespace {

class ThrowingEventHandler : public ContextEventLogger {
public:
    void handle_event(Context& context, const std::string& event_type, const nlohmann::json& data) override {
        (void)context;
        (void)data;
        events.push_back(event_type);
        throw std::runtime_error("logger failed");
    }

    std::vector<std::string> events;
};

class CountingDataProvider : public ContextDataProvider {
public:
    explicit CountingDataProvider(ContextData data) : data_(std::move(data)) {}

    std::future<ContextData> get_context_data() override {
        ++calls;
        std::promise<ContextData> promise;
        promise.set_value(data_);
        return promise.get_future();
    }

    int calls = 0;

private:
    ContextData data_;
};

} // namespace

TEST_CASE("Reads throw before the context is ready", "[context][notready]") {
    auto config = make_test_config();
    std::promise<ContextData> promise;
    Context ctx(config, promise.get_future());

    REQUIRE_FALSE(ctx.is_ready());
    REQUIRE_THROWS_AS(ctx.treatment("exp_test_ab"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.peek("exp_test_ab"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.variable_value("banner.size", "small"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.peek_variable_value("banner.size", "small"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.variable_keys(), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.experiments(), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.publish(), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.refresh(), ContextNotReadyException);
    REQUIRE_THROWS_WITH(ctx.treatment("exp_test_ab"), "ABsmartly Context is not yet ready.");

    // Writes are allowed before ready, as in JS.
    REQUIRE_NOTHROW(ctx.track("goal1"));
    REQUIRE_NOTHROW(ctx.set_attribute("a", 1));

    promise.set_value(make_test_data());
    REQUIRE(ctx.treatment("exp_test_ab") == 1);
}

TEST_CASE("Reads on a failed context return defaults", "[context][failed]") {
    auto config = make_test_config();
    std::promise<ContextData> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("load failed")));
    Context ctx(config, promise.get_future());
    ctx.wait_until_ready();

    REQUIRE(ctx.is_failed());
    REQUIRE(ctx.treatment("exp_test_ab") == 0);
    REQUIRE(ctx.variable_value("banner.size", "small") == "small");
    REQUIRE(ctx.experiments().empty());
}

TEST_CASE("Refresh on a failed context is a no-op", "[context][failed]") {
    auto config = make_test_config();
    auto provider = std::make_shared<CountingDataProvider>(make_test_data());
    std::promise<ContextData> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("load failed")));
    Context ctx(config, promise.get_future(), nullptr, nullptr, provider);
    ctx.wait_until_ready();

    REQUIRE_NOTHROW(ctx.refresh());
    REQUIRE_NOTHROW(ctx.refresh(make_test_data()));
    REQUIRE(provider->calls == 0);
    REQUIRE(ctx.is_failed());
    REQUIRE(ctx.experiments().empty());
    REQUIRE(ctx.treatment("exp_test_ab") == 0);
}

TEST_CASE("Publishing on a failed context discards the queued events", "[context][failed]") {
    auto config = make_test_config();
    auto handler = std::make_shared<MockEventHandler>();
    auto publisher = std::make_shared<ScriptedPublisher>();
    std::promise<ContextData> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("load failed")));
    Context ctx(config, promise.get_future(), handler, publisher);
    ctx.wait_until_ready();

    ctx.track("goal1");
    REQUIRE(ctx.pending() == 1);
    handler->clear();

    REQUIRE_NOTHROW(ctx.finalize());
    REQUIRE(ctx.is_finalized());
    REQUIRE(ctx.pending() == 0);
    REQUIRE(publisher->received.empty());
    REQUIRE(handler->count_events("error") == 1);
}

TEST_CASE("A late unit does not change a cached unassigned treatment", "[context][sticky]") {
    // JS keeps the cached assignment (experimentMatches ignores units;
    // unit() only evicts holdout/rule assignments pinned with a missing unit).
    ContextConfig config;
    Context ctx(config, make_test_data());

    REQUIRE(ctx.peek("exp_test_ab") == 0);
    ctx.set_unit("session_id", "e791e240fcd3df7d238cfc285f475e8152fcc0ec");
    REQUIRE(ctx.treatment("exp_test_ab") == 0);
    REQUIRE(ctx.publish().exposures.at(0).assigned == false);

    REQUIRE(ctx.treatment("exp_test_abc") == 2);
}

TEST_CASE("Override retains cache across iteration and seed changes (scenario 33)", "[context][sticky][scenario33]") {
    auto config = make_test_config();
    auto data = make_test_data();
    auto handler = std::make_shared<MockEventHandler>();
    Context ctx(config, data, handler);
    ctx.set_override("exp_test_ab", 1);
    REQUIRE(ctx.treatment("exp_test_ab") == 1);
    REQUIRE(handler->count_events("exposure") == 1);

    for (auto& exp : data.experiments) {
        if (exp.name == "exp_test_ab") {
            ++exp.iteration;
            exp.seedHi = 99999;
            exp.seedLo = 99999;
            exp.split = {0.3, 0.7};
        }
    }
    ctx.refresh(data);
    REQUIRE(ctx.treatment("exp_test_ab") == 1);
    REQUIRE(handler->count_events("exposure") == 1);
    REQUIRE(ctx.pending() == 1);
    const auto event = ctx.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].overridden);
    REQUIRE_FALSE(event.exposures[0].assigned);
}

TEST_CASE("Ordinary assignments retain variant configs across non-key refresh changes", "[context][sticky]") {
    auto config = make_test_config();
    auto data = make_test_data();
    Context ctx(config, data);
    const auto variant = ctx.treatment("exp_test_ab");
    const auto value = ctx.variable_value("banner.size", "missing");
    REQUIRE(ctx.pending() == 1);

    for (auto& exp : data.experiments) {
        if (exp.name == "exp_test_ab") {
            exp.seedHi = 99999;
            exp.seedLo = 99999;
            exp.trafficSeedHi = 99999;
            exp.trafficSeedLo = 99999;
            exp.split = {1.0, 0.0};
            exp.audience = "not json";
            for (auto& v : exp.variants) {
                v.config = nlohmann::json{{"banner.size", "changed"}};
            }
        }
    }
    ctx.refresh(data);
    REQUIRE(ctx.treatment("exp_test_ab") == variant);
    REQUIRE(ctx.variable_value("banner.size", "missing") == value);
    REQUIRE(ctx.pending() == 1);
}

TEST_CASE("An indeterminate audience result keeps the cached assignment", "[context][sticky]") {
    ContextData data;
    ExperimentData exp;
    exp.id = 1;
    exp.name = "exp";
    exp.unitType = "session_id";
    exp.split = {0.0, 1.0};
    exp.trafficSplit = {0.0, 1.0};
    exp.audienceStrict = true;
    exp.audience = nlohmann::json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
    exp.variants = {ExperimentVariant{}, ExperimentVariant{}};
    data.experiments.push_back(exp);

    auto config = make_test_config();
    Context ctx(config, data);
    ctx.set_attribute("age", 10);
    REQUIRE(ctx.treatment("exp") == 0);
    REQUIRE(ctx.pending() == 1);

    // Audience text is not part of the cache key, so the refresh keeps the
    // assignment; the unparsable audience then evaluates to "unknown" after
    // the attribute change, which must not flip the cached mismatch.
    data.experiments[0].audience = nlohmann::json("not json");
    ctx.refresh(data);
    ctx.set_attribute("age", 30);
    REQUIRE(ctx.treatment("exp") == 0);
    REQUIRE(ctx.pending() == 1);
}

TEST_CASE("A throwing event logger does not fail the context", "[context][logger]") {
    auto config = make_test_config();
    auto handler = std::make_shared<ThrowingEventHandler>();
    auto publisher = std::make_shared<ScriptedPublisher>();

    std::promise<ContextData> promise;
    promise.set_value(make_test_data());
    Context ctx(config, promise.get_future(), handler, publisher);
    ctx.wait_until_ready();

    REQUIRE(ctx.is_ready());
    REQUIRE_FALSE(ctx.is_failed());

    REQUIRE(ctx.treatment("exp_test_ab") == 1);
    REQUIRE(ctx.pending() == 1);

    REQUIRE_NOTHROW(ctx.publish());
    REQUIRE(ctx.pending() == 0);
    REQUIRE(publisher->received.size() == 1);
    REQUIRE(publisher->received[0].exposures.size() == 1);

    ctx.track("goal1");
    REQUIRE_NOTHROW(ctx.finalize());
    REQUIRE(ctx.is_finalized());
    REQUIRE(publisher->received.size() == 2);
}

namespace {

class ReentrantFinalizePublisher : public ContextPublisher {
public:
    std::future<void> publish(const PublishEvent& event) override {
        (void)event;
        ++calls;
        if (calls == 1) {
            context->finalize();
            throw std::runtime_error("Publish failed");
        }
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }

    Context* context = nullptr;
    int calls = 0;
};

} // namespace

TEST_CASE("A re-entrant finalize during a failing finalize does not seal the context", "[context][publishfail]") {
    auto config = make_test_config();
    auto publisher = std::make_shared<ReentrantFinalizePublisher>();
    Context ctx(config, make_test_data(), nullptr, publisher);
    publisher->context = &ctx;

    ctx.treatment("exp_test_ab");
    REQUIRE_THROWS_WITH(ctx.finalize(), "Publish failed");

    REQUIRE_FALSE(ctx.is_finalized());
    REQUIRE_FALSE(ctx.is_finalizing());
    REQUIRE(ctx.pending() == 1);

    REQUIRE_NOTHROW(ctx.finalize());
    REQUIRE(ctx.is_finalized());
    REQUIRE(ctx.pending() == 0);
}

TEST_CASE("Reads during finalize report finalizing", "[context][errors]") {
    auto config = make_test_config();

    class ReadingPublisher : public ContextPublisher {
    public:
        std::future<void> publish(const PublishEvent&) override {
            try {
                context->treatment("exp_test_ab");
            } catch (const ContextFinalizingException& e) {
                message = e.what();
                caught_finalizing = true;
            } catch (const std::exception& e) {
                message = e.what();
            }
            try {
                context->peek("exp_test_ab");
            } catch (const ContextFinalizedException&) {
                caught_as_finalized = true;
            } catch (...) {
            }
            std::promise<void> p;
            p.set_value();
            return p.get_future();
        }
        Context* context = nullptr;
        std::string message;
        bool caught_finalizing = false;
        bool caught_as_finalized = false;
    };

    auto publisher = std::make_shared<ReadingPublisher>();
    Context ctx(config, make_test_data(), nullptr, publisher);
    publisher->context = &ctx;
    ctx.track("goal1");
    ctx.finalize();

    REQUIRE(publisher->message == "ABsmartly Context is finalizing.");
    REQUIRE(publisher->caught_finalizing);
    REQUIRE(publisher->caught_as_finalized);
}
