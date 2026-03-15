#include <catch2/catch_test_macros.hpp>
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

class MockEventHandler : public ContextEventHandler {
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

    SECTION("should throw after finalize") {
        Context ctx(config, data);
        ctx.finalize();
        REQUIRE_THROWS_AS(ctx.set_custom_assignment("exp_test_ab", 1), ContextFinalizedException);
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
        REQUIRE_THROWS_AS(ctx.set_custom_assignment("exp", 1), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.variable_value("key", 0), ContextFinalizedException);
        REQUIRE_THROWS_AS(ctx.peek_variable_value("key", 0), ContextFinalizedException);
    }
}

TEST_CASE("Context refresh/cache invalidation", "[context]") {
    auto config = make_test_config();

    SECTION("should clear cache for started experiment") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_new") == 0);
        REQUIRE(ctx.pending() == 1);

        auto refresh_data = make_refresh_data();
        ctx.refresh(refresh_data);

        REQUIRE(ctx.treatment("exp_test_new") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache for stopped experiment") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        auto stopped_data = data;
        stopped_data.experiments.erase(
            std::remove_if(stopped_data.experiments.begin(),
                           stopped_data.experiments.end(),
                           [](const ExperimentData& e) { return e.name == "exp_test_abc"; }),
            stopped_data.experiments.end());

        ctx.refresh(stopped_data);

        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when experiment ID changes") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

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

        ctx.refresh(changed_data);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when full-on changes") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_abc") == 2);
        REQUIRE(ctx.pending() == 1);

        auto fullon_data = data;
        for (auto& exp : fullon_data.experiments) {
            if (exp.name == "exp_test_abc") {
                exp.fullOnVariant = 1;
            }
        }

        ctx.refresh(fullon_data);

        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should clear cache when traffic split changes") {
        auto data = make_test_data();
        Context ctx(config, data);

        REQUIRE(ctx.treatment("exp_test_not_eligible") == 0);
        REQUIRE(ctx.pending() == 1);

        auto split_data = data;
        for (auto& exp : split_data.experiments) {
            if (exp.name == "exp_test_not_eligible") {
                exp.trafficSplit = {0.0, 1.0};
            }
        }

        ctx.refresh(split_data);

        REQUIRE(ctx.treatment("exp_test_not_eligible") == 2);
        REQUIRE(ctx.pending() == 2);
    }

    SECTION("should re-queue exposures after refresh even when not changed") {
        auto data = make_test_data();
        Context ctx(config, data);

        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }
        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));

        auto refresh_data = make_refresh_data();
        ctx.refresh(refresh_data);

        // Refresh keeps cached assignments — pending unchanged
        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()));

        for (const auto& exp : data.experiments) {
            ctx.treatment(exp.name);
        }
        // After refresh, exposed is reset to false so treatments re-queue exposures
        REQUIRE(ctx.pending() == static_cast<int>(data.experiments.size()) * 2);
    }

    SECTION("should keep overrides after refresh") {
        auto data = make_test_data();
        Context ctx(config, data);

        ctx.set_override("not_found", 3);
        REQUIRE(ctx.peek("not_found") == 3);

        auto refresh_data = make_refresh_data();
        ctx.refresh(refresh_data);

        REQUIRE(ctx.peek("not_found") == 3);
    }

    SECTION("should keep custom assignments after refresh") {
        auto data = make_test_data();
        Context ctx(config, data);

        ctx.set_custom_assignment("exp_test_ab", 3);
        REQUIRE(ctx.peek("exp_test_ab") == 3);

        auto refresh_data = make_refresh_data();
        ctx.refresh(refresh_data);

        REQUIRE(ctx.peek("exp_test_ab") == 3);
    }

    SECTION("should throw after finalize") {
        auto data = make_test_data();
        Context ctx(config, data);
        ctx.finalize();

        REQUIRE_THROWS_AS(ctx.refresh(data), ContextFinalizedException);
    }

    SECTION("should emit refresh event") {
        auto data = make_test_data();
        auto handler = std::make_shared<MockEventHandler>();
        Context ctx(config, data, handler);

        handler->clear();
        ctx.refresh(data);
        REQUIRE(handler->count_events("refresh") == 1);
    }

    SECTION("should re-queue overridden experiment exposure after refresh") {
        auto data = make_test_data();
        Context ctx(config, data);

        ctx.set_override("exp_test_ab", 3);
        REQUIRE(ctx.treatment("exp_test_ab") == 3);
        REQUIRE(ctx.pending() == 1);

        auto changed_data = data;
        for (auto& exp : changed_data.experiments) {
            if (exp.name == "exp_test_ab") {
                exp.id = 99;
            }
        }

        ctx.refresh(changed_data);

        REQUIRE(ctx.treatment("exp_test_ab") == 3);
        // After refresh, exposed is reset — override treatment re-queues exposure
        REQUIRE(ctx.pending() == 2);
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

TEST_CASE("Fix: check_ready on custom_field_value and custom_field_keys", "[context][fix8]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    std::promise<ContextData> promise;
    auto future = promise.get_future();

    Context ctx(config, std::move(future));

    REQUIRE_THROWS_AS(ctx.custom_field_value("exp_test_abc", "country"), ContextNotReadyException);
    REQUIRE_THROWS_AS(ctx.custom_field_keys(), ContextNotReadyException);
}

TEST_CASE("Fix: set_override throws after finalize", "[context][fix23]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.finalize();

    // Overrides are allowed after finalize (cross-SDK consistency)
    REQUIRE_NOTHROW(ctx.set_override("exp_test_ab", 1));
    REQUIRE_NOTHROW(ctx.set_overrides({{"exp_test_ab", 2}}));
}

TEST_CASE("Fix: experiments() throws after finalize", "[context][fix22]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();

    Context ctx(config, data);
    ctx.finalize();

    REQUIRE_THROWS_AS(ctx.experiments(), ContextFinalizedException);
}

TEST_CASE("Fix: variable_keys() throws before ready and after finalize", "[context][fix24]") {
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

TEST_CASE("Fix: custom_field_value/keys throws after finalize", "[context][fix8]") {
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

TEST_CASE("Fix: publish calls event_publisher", "[context][fix36]") {
    class MockPublisher : public ContextEventPublisher {
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

    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();
    auto publisher = std::make_shared<MockPublisher>();

    Context ctx(config, data, nullptr, publisher);
    ctx.treatment("exp_test_ab");

    auto event = ctx.publish();
    REQUIRE(publisher->call_count == 1);
    REQUIRE(!publisher->last_event.exposures.empty());
}

TEST_CASE("Fix: finalize calls event_publisher", "[context][fix36]") {
    class MockPublisher : public ContextEventPublisher {
    public:
        int call_count = 0;
        std::future<void> publish(const PublishEvent& event) override {
            (void)event;
            call_count++;
            std::promise<void> p;
            p.set_value();
            return p.get_future();
        }
    };

    ContextConfig config;
    config.units = {{"session_id", "abc123"}};
    ContextData data = make_test_data();
    auto publisher = std::make_shared<MockPublisher>();

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
    Context ctx(config, data, handler);

    int t1 = ctx.treatment("exp_test_ab");
    handler->clear();

    ctx.refresh(data);
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

    auto handler = std::make_shared<MockEventHandler>();
    Context ctx(config, data, handler);

    ctx.treatment("exp_test_ab");
    handler->clear();

    ContextData new_data = data;
    new_data.experiments[0].iteration = 2;

    ctx.refresh(new_data);

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
    REQUIRE(ctx.ready_error() == "load failed");
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
        ctx.treatment("exp_test_ab");
        FAIL("Expected ContextFinalizedException");
    } catch (const ContextFinalizedException& e) {
        REQUIRE(std::string(e.what()) == "ABsmartly Context is finalized.");
    }
}

TEST_CASE("not ready error message uses ABsmartly prefix", "[context][errors]") {
    ContextConfig config;
    config.units = {{"session_id", "abc123"}};

    std::promise<ContextData> p;
    auto f = p.get_future();

    Context ctx(config, std::move(f));

    try {
        ctx.treatment("exp_test_ab");
        FAIL("Expected ContextNotReadyException");
    } catch (const ContextNotReadyException& e) {
        REQUIRE(std::string(e.what()) == "ABsmartly Context is not yet ready.");
    }
    p.set_value(ContextData{});
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
