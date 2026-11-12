#include <catch2/catch_test_macros.hpp>
#include <absmartly/audience_matcher.h>
#include <absmartly/context.h>
#include <absmartly/context_event_logger.h>

#include <algorithm>
#include <functional>

using namespace absmartly;
using json = nlohmann::json;

// Ported from the JS SDK's "rules evaluation" / "split rules" context tests (javascript-sdk#66)
// and the Java SDK's ContextAssignmentRulesTest (java-sdk#17), on the same exp_test_abc
// fixture and unit, so every SDK resolves the same rules to the same variants. Without rules
// this unit gets variant 2. With these seeds it hashes to ~0.748 on the experiment seed and
// ~0.682 on the traffic seed, so each way of resolving a split lands on a different variant.

static const char* SESSION_ID = "e791e240fcd3df7d238cfc285f475e8152fcc0ec";
static const int NORMAL_VARIANT = 2;

static ExperimentData make_exp_test_abc() {
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
    exp.audience = json(R"({"filter":[{"value":true}]})");

    ExperimentVariant v0;
    v0.name = "A";
    v0.config = nullptr;
    ExperimentVariant v1;
    v1.name = "B";
    v1.config = json(R"({"button.color":"blue"})");
    ExperimentVariant v2;
    v2.name = "C";
    v2.config = json(R"({"button.color":"red"})");
    exp.variants = {v0, v1, v2};
    return exp;
}

static ContextData make_data(const json& rules, const std::function<void(ExperimentData&)>& edit = nullptr) {
    auto exp = make_exp_test_abc();
    if (!rules.is_null()) {
        exp.assignmentRules = rules.is_string() ? rules.get<std::string>() : rules.dump();
    }
    if (edit) {
        edit(exp);
    }
    ContextData data;
    data.experiments.push_back(exp);
    return data;
}

static json us_condition() {
    return json::parse(R"({"and":[{"eq":[{"var":"country"},{"value":"US"}]}]})");
}

static json assign_rule(int variant, const json& conditions = us_condition(),
                        const json& environments = json::array()) {
    return {{"name", "US Users"}, {"type", "assign"}, {"conditions", conditions},
            {"environments", environments}, {"variant", variant}};
}

static json split_rule(const json& percentages, const json& conditions = us_condition()) {
    return {{"name", "US Users"}, {"type", "split"}, {"conditions", conditions},
            {"environments", json::array()}, {"percentages", percentages}};
}

static json rules_of(std::initializer_list<json> rules) {
    json list = json::array();
    for (const auto& rule : rules) {
        list.push_back(rule);
    }
    return {{"rules", list}};
}

static ContextConfig make_config(bool with_session = true, const std::string& environment = "") {
    ContextConfig config;
    config.units = {{"user_id", "123456789"}};
    if (with_session) {
        config.units["session_id"] = SESSION_ID;
    }
    config.environment = environment;
    return config;
}

struct ErrorCounter : ContextEventLogger {
    int errors = 0;
    void handle_event(Context&, const std::string& type, const json&) override {
        if (type == "error") ++errors;
    }
};

static std::vector<Exposure> exposures_for(const PublishEvent& event, const std::string& name) {
    std::vector<Exposure> result;
    std::copy_if(event.exposures.begin(), event.exposures.end(), std::back_inserter(result),
                 [&](const Exposure& e) { return e.name == name; });
    return result;
}

TEST_CASE("Assignment rules: assign", "[assignment-rules]") {
    SECTION("a matching rule gives its variant") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("normal assignment when no rule matches") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "UK");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }

    SECTION("a rule without conditions always matches") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1, nullptr)})));
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("multiple and conditions must all hold") {
        auto conditions = json::parse(
            R"({"and":[{"eq":[{"var":"country"},{"value":"US"}]},{"gte":[{"var":"age"},{"value":18}]}]})");
        Context ctx(make_config(), make_data(rules_of({assign_rule(1, conditions)})));
        ctx.set_attribute("country", "US");
        ctx.set_attribute("age", 16);
        REQUIRE(ctx.peek("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("age", 21);
        REQUIRE(ctx.peek("exp_test_abc") == 1);
    }

    SECTION("the first matching rule wins") {
        auto pt = json::parse(R"({"eq":[{"var":"country"},{"value":"PT"}]})");
        Context ctx(make_config(), make_data(rules_of({assign_rule(0, pt), assign_rule(1), assign_rule(2, nullptr)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("variant 0 from a rule is still a rule override") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(0)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        REQUIRE(exposures[0].ruleOverride);
    }

    SECTION("an out-of-range or negative variant falls back to normal assignment without trying later rules") {
        for (int variant : {3, -1}) {
            Context ctx(make_config(), make_data(rules_of({assign_rule(variant), assign_rule(1, nullptr)})));
            ctx.set_attribute("country", "US");
            REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
            REQUIRE_FALSE(exposures_for(ctx.publish(), "exp_test_abc")[0].ruleOverride);
        }
    }

    SECTION("a non-integer variant skips the rule") {
        auto bad = assign_rule(1);
        bad["variant"] = 1.5;
        auto bad_string = assign_rule(1);
        bad_string["variant"] = "1";
        Context ctx(make_config(), make_data(rules_of({bad, bad_string, assign_rule(0, nullptr)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
    }

    SECTION("an integral float variant is accepted") {
        auto rule = assign_rule(1);
        rule["variant"] = 1.0;
        Context ctx(make_config(), make_data(rules_of({rule})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("invalid or non-object assignmentRules fall back to normal assignment with an error event") {
        for (const char* rules : {"{not json", "[]", "5", "\"rules\""}) {
            auto logger = std::make_shared<ErrorCounter>();
            Context ctx(make_config(), make_data(json(rules)), logger);
            ctx.set_attribute("country", "US");
            REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
            REQUIRE(logger->errors == 1);
        }
    }

    SECTION("an object without a rules array means no rules, without an error event") {
        for (const char* rules : {"{}", R"({"rules":{}})", R"({"rules":null})"}) {
            auto logger = std::make_shared<ErrorCounter>();
            Context ctx(make_config(), make_data(json(rules)), logger);
            ctx.set_attribute("country", "US");
            REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
            REQUIRE(logger->errors == 0);
        }
    }

    SECTION("variable values follow the rule's variant") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.peek_variable_value("button.color", "none") == "blue");
        REQUIRE(ctx.variable_value("button.color", "none") == "blue");
    }

    SECTION("a rule can match on the application system attribute") {
        auto conditions = json::parse(R"({"eq":[{"var":"application"},{"value":"website"}]})");
        auto config = make_config();
        config.application = "website";
        Context ctx(config, make_data(rules_of({assign_rule(1, conditions)})));
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("a rule can compare app versions with semver operators") {
        auto conditions = json::parse(R"({"semver_gte":[{"var":"app_version"},{"value":"2.4.0"}]})");
        Context ctx(make_config(), make_data(rules_of({assign_rule(1, conditions)})));
        ctx.set_attribute("app_version", "2.3.9");
        REQUIRE(ctx.peek("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("app_version", "v2.10.0");
        REQUIRE(ctx.peek("exp_test_abc") == 1);
    }
}

TEST_CASE("Assignment rules: environments", "[assignment-rules]") {
    SECTION("a rule scoped to another environment is skipped") {
        Context ctx(make_config(true, "staging"),
                    make_data(rules_of({assign_rule(1, us_condition(), {"production"})})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }

    SECTION("an environment-scoped rule never matches a context without an environment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1, us_condition(), {"production"})})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }

    SECTION("a rule scoped to the context's environment matches") {
        Context ctx(make_config(true, "production"),
                    make_data(rules_of({assign_rule(1, us_condition(), {"staging", "production"})})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("environment names are case-sensitive") {
        Context ctx(make_config(true, "Production"),
                    make_data(rules_of({assign_rule(1, us_condition(), {"production"})})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }

    SECTION("an empty or null environments list matches every environment") {
        auto null_envs = assign_rule(1);
        null_envs["environments"] = nullptr;
        for (const auto& rule : {assign_rule(1), null_envs}) {
            Context ctx(make_config(true, "production"), make_data(rules_of({rule})));
            ctx.set_attribute("country", "US");
            REQUIRE(ctx.treatment("exp_test_abc") == 1);
        }
    }

    SECTION("a non-array environments value skips the rule") {
        auto rule = assign_rule(1);
        rule["environments"] = "production";
        Context ctx(make_config(true, "production"), make_data(rules_of({rule})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }
}

TEST_CASE("Assignment rules: precedence and exposure flags", "[assignment-rules]") {
    SECTION("rule exposures are flagged ruleOverride, not assigned, eligible") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        ctx.treatment("exp_test_abc");
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        const auto& e = exposures[0];
        REQUIRE(e.id == 2);
        REQUIRE(e.unit == "session_id");
        REQUIRE(e.variant == 1);
        REQUIRE_FALSE(e.assigned);
        REQUIRE(e.eligible);
        REQUIRE_FALSE(e.overridden);
        REQUIRE_FALSE(e.fullOn);
        REQUIRE_FALSE(e.custom);
        REQUIRE_FALSE(e.audienceMismatch);
        REQUIRE(e.ruleOverride);
    }

    SECTION("normal assignment exposures are not flagged") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "UK");
        ctx.treatment("exp_test_abc");
        auto e = exposures_for(ctx.publish(), "exp_test_abc")[0];
        REQUIRE(e.assigned);
        REQUIRE(e.eligible);
        REQUIRE_FALSE(e.ruleOverride);
    }

    SECTION("ruleOverride is serialized in the publish payload") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        ctx.treatment("exp_test_abc");
        json payload = ctx.publish();
        REQUIRE(payload["exposures"][0]["ruleOverride"] == true);
    }

    SECTION("an override wins over a rule, with override flags") {
        for (int override_variant : {0, 1}) {
            Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
            ctx.set_attribute("country", "US");
            ctx.set_override("exp_test_abc", override_variant);
            REQUIRE(ctx.treatment("exp_test_abc") == override_variant);
            auto e = exposures_for(ctx.publish(), "exp_test_abc")[0];
            REQUIRE(e.overridden);
            REQUIRE_FALSE(e.ruleOverride);
        }
    }

    SECTION("a rule wins over a custom assignment and is exposed only once") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        ctx.set_custom_assignment("exp_test_abc", 2);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        REQUIRE_FALSE(exposures[0].custom);
    }

    SECTION("a rule wins over a full-on variant") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), [](ExperimentData& e) { e.fullOnVariant = 2; }));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE_FALSE(exposures_for(ctx.publish(), "exp_test_abc")[0].fullOn);
    }

    SECTION("a rule wins over traffic eligibility") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), [](ExperimentData& e) { e.trafficSplit = {1.0, 0.0}; }));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("a rule wins over audienceStrict without evaluating the audience") {
        auto strict = [](ExperimentData& e) {
            e.audienceStrict = true;
            e.audience = json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
        };
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), strict));
        ctx.set_attribute("country", "US");
        ctx.set_attribute("age", 10);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE_FALSE(exposures_for(ctx.publish(), "exp_test_abc")[0].audienceMismatch);
    }

    SECTION("audienceStrict applies when no rule matches") {
        auto strict = [](ExperimentData& e) {
            e.audienceStrict = true;
            e.audience = json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
        };
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), strict));
        ctx.set_attribute("country", "UK");
        ctx.set_attribute("age", 10);
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        auto e = exposures_for(ctx.publish(), "exp_test_abc")[0];
        REQUIRE(e.audienceMismatch);
        REQUIRE_FALSE(e.ruleOverride);
    }
}

TEST_CASE("Assignment rules: caching", "[assignment-rules]") {
    SECTION("an attribute change that switches the matching rule re-resolves") {
        auto pt = json::parse(R"({"eq":[{"var":"country"},{"value":"PT"}]})");
        Context ctx(make_config(), make_data(rules_of({assign_rule(1), assign_rule(0, pt)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.set_attribute("country", "PT");
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 2);
    }

    SECTION("a rule that stops matching falls back to normal assignment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.set_attribute("country", "UK");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 2);
        REQUIRE(exposures[0].ruleOverride);
        REQUIRE_FALSE(exposures[1].ruleOverride);
    }

    SECTION("a rule that starts matching takes over from normal assignment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("an unrelated attribute does not re-expose a rule-won assignment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        ctx.treatment("exp_test_abc");
        ctx.set_attribute("unrelated", true);
        ctx.treatment("exp_test_abc");
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 1);
    }

    SECTION("an audience change does not rebuild a rule-won assignment") {
        auto audience = [](ExperimentData& e) {
            e.audience = json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
        };
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), audience));
        ctx.set_attribute("country", "US");
        ctx.set_attribute("age", 30);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.set_attribute("age", 10);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        REQUIRE_FALSE(exposures[0].audienceMismatch);
    }

    SECTION("an unmatched rule without an audience still re-resolves on later attribute changes") {
        auto no_audience = [](ExperimentData& e) { e.audience = nullptr; };
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), no_audience));
        ctx.set_attribute("country", "UK");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 2);
    }

    SECTION("a rule check does not skip a pending audience re-check") {
        auto audience = [](ExperimentData& e) {
            e.audienceStrict = true;
            e.audience = json(R"({"filter":[{"gte":[{"var":"age"},{"value":20}]}]})");
        };
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)}), audience));
        ctx.set_attribute("age", 30);
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("age", 10);
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc")[1].audienceMismatch);
    }

    SECTION("an out-of-range rule variant that changes to another out-of-range one keeps the cache") {
        auto pt = json::parse(R"({"eq":[{"var":"country"},{"value":"PT"}]})");
        Context ctx(make_config(), make_data(rules_of({assign_rule(5), assign_rule(7, pt)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        ctx.set_attribute("country", "PT");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 1);
    }

    SECTION("a refresh that changes the rules re-resolves") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.refresh(make_data(rules_of({assign_rule(0)})));
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
    }

    SECTION("a refresh that removes the rules goes back to normal assignment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.refresh(make_data(nullptr));
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 2);
        REQUIRE_FALSE(exposures[1].ruleOverride);
    }

    SECTION("a refresh with identical rules keeps the exposed assignment") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        ctx.treatment("exp_test_abc");
        ctx.refresh(make_data(rules_of({assign_rule(1)})));
        ctx.treatment("exp_test_abc");
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 1);
    }

    SECTION("a rule-won assignment whose experiment disappears is rebuilt") {
        Context ctx(make_config(), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.refresh(ContextData{});
        REQUIRE(ctx.treatment("exp_test_abc") == 0);
    }
}

TEST_CASE("Assignment rules: split", "[assignment-rules][split]") {
    SECTION("hashes the unit with the experiment seed") {
        Context ctx(make_config(), make_data(rules_of({split_rule("70/10/20")})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("a split equal to the experiment's keeps the unit's normal variant") {
        Context ctx(make_config(), make_data(rules_of({split_rule("34/33/33")})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc")[0].ruleOverride);
    }

    SECTION("assigns the only variant with a non-zero share") {
        Context ctx(make_config(), make_data(rules_of({split_rule("0/100/0")})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
    }

    SECTION("sets the same exposure flags as an assign rule") {
        Context ctx(make_config(), make_data(rules_of({split_rule("0/100/0")})));
        ctx.set_attribute("country", "US");
        ctx.treatment("exp_test_abc");
        auto e = exposures_for(ctx.publish(), "exp_test_abc")[0];
        REQUIRE(e.variant == 1);
        REQUIRE_FALSE(e.assigned);
        REQUIRE(e.eligible);
        REQUIRE_FALSE(e.overridden);
        REQUIRE_FALSE(e.fullOn);
        REQUIRE_FALSE(e.custom);
        REQUIRE_FALSE(e.audienceMismatch);
        REQUIRE(e.ruleOverride);
    }

    SECTION("takes priority over a custom assignment") {
        Context ctx(make_config(), make_data(rules_of({split_rule("0/100/0")})));
        ctx.set_attribute("country", "US");
        ctx.set_custom_assignment("exp_test_abc", 2);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 1);
    }

    SECTION("a wrong value count falls back to normal assignment without trying later rules") {
        Context ctx(make_config(), make_data(rules_of({split_rule("0/100"), assign_rule(1, nullptr)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == NORMAL_VARIANT);
    }

    SECTION("malformed percentages skip to the next rule") {
        for (const json& bad : {json("abc/50/50"), json("-10/60/50"), json("10/10/10"), json(json::array({0, 100, 0})),
                                json(nullptr), json("")}) {
            Context ctx(make_config(), make_data(rules_of({split_rule(bad), assign_rule(0, nullptr)})));
            ctx.set_attribute("country", "US");
            REQUIRE(ctx.treatment("exp_test_abc") == 0);
        }
    }

    SECTION("a missing unit gives variant 0 and stops rule evaluation, then re-resolves once the unit is set") {
        auto everyone = assign_rule(2, nullptr);
        Context ctx(make_config(false), make_data(rules_of({split_rule("0/100/0"), everyone})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.peek("exp_test_abc") == 0);

        ctx.set_unit("session_id", SESSION_ID);
        REQUIRE(ctx.peek("exp_test_abc") == 1);
    }

    SECTION("an assign rule's cached assignment is kept when the unit is set later") {
        Context ctx(make_config(false), make_data(rules_of({assign_rule(1)})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        ctx.set_unit("session_id", SESSION_ID);
        REQUIRE(ctx.treatment("exp_test_abc") == 1);
        REQUIRE(exposures_for(ctx.publish(), "exp_test_abc").size() == 1);
    }

    SECTION("re-resolves once the unit is set when the split starts matching on the previous rule's variant") {
        auto fallback = assign_rule(0, nullptr);
        Context ctx(make_config(false), make_data(rules_of({split_rule("0/100/0"), fallback})));
        REQUIRE(ctx.peek("exp_test_abc") == 0);

        ctx.set_attribute("country", "US");
        REQUIRE(ctx.peek("exp_test_abc") == 0);

        ctx.set_unit("session_id", SESSION_ID);
        REQUIRE(ctx.peek("exp_test_abc") == 1);
    }

    SECTION("an exposed split stays on variant 0 after its unit is set and is exposed only once") {
        Context ctx(make_config(false), make_data(rules_of({split_rule("0/100/0")})));
        ctx.set_attribute("country", "US");
        REQUIRE(ctx.treatment("exp_test_abc") == 0);

        ctx.set_unit("session_id", SESSION_ID);
        ctx.set_attribute("unrelated", true);
        REQUIRE(ctx.treatment("exp_test_abc") == 0);

        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        REQUIRE(exposures[0].variant == 0);
    }

    SECTION("an exposed split stays on variant 0 after its unit is set when a custom assignment lost to it") {
        Context ctx(make_config(false), make_data(rules_of({split_rule("0/100/0")})));
        ctx.set_attribute("country", "US");
        ctx.set_custom_assignment("exp_test_abc", 2);
        REQUIRE(ctx.treatment("exp_test_abc") == 0);

        ctx.set_unit("session_id", SESSION_ID);
        REQUIRE(ctx.treatment("exp_test_abc") == 0);

        auto exposures = exposures_for(ctx.publish(), "exp_test_abc");
        REQUIRE(exposures.size() == 1);
        REQUIRE(exposures[0].variant == 0);
    }

    SECTION("gives the full-on variant while the unit is missing on a full-on experiment") {
        auto everyone = split_rule("0/100/0", nullptr);
        Context ctx(make_config(false), make_data(rules_of({everyone}), [](ExperimentData& e) { e.fullOnVariant = 2; }));
        REQUIRE(ctx.peek("exp_test_abc") == 2);

        ctx.set_unit("session_id", SESSION_ID);
        REQUIRE(ctx.peek("exp_test_abc") == 1);
    }
}

TEST_CASE("Assignment rules: split percentages parsing", "[assignment-rules][split]") {
    const auto parse = [](const json& v) { return AudienceMatcher::parse_split_percentages(v); };

    SECTION("values are divided by 100 without normalisation") {
        auto split = parse("20/30/50");
        REQUIRE(split.has_value());
        REQUIRE(*split == std::vector<double>{0.2, 0.3, 0.5});
    }

    SECTION("sum tolerance matches the backend: the gap rounded to two decimals must be <= 0.01") {
        REQUIRE(parse("50/50.014").has_value());
        REQUIRE(parse("50/49.99").has_value());
        REQUIRE(parse("33.33/33.33/33.34").has_value());
        REQUIRE_FALSE(parse("50/50.015").has_value());
        REQUIRE_FALSE(parse("50/49.98").has_value());
    }

    SECTION("values are read like JavaScript parseFloat") {
        REQUIRE(parse(" 50/50").has_value());
        REQUIRE(parse("50abc/50").has_value());
        REQUIRE(parse("+50/5e1").has_value());
        REQUIRE(parse(".5e2/50").has_value());
        REQUIRE_FALSE(parse("abc/100").has_value());
        REQUIRE_FALSE(parse("/100").has_value());
        REQUIRE_FALSE(parse("100/").has_value());
        REQUIRE_FALSE(parse("-0.5/100.5").has_value());
        REQUIRE_FALSE(parse("Infinity/0").has_value());
        REQUIRE_FALSE(parse("1e400/0").has_value());
    }

    SECTION("non-string percentages are rejected") {
        REQUIRE_FALSE(parse(100).has_value());
        REQUIRE_FALSE(parse(json::array({50, 50})).has_value());
        REQUIRE_FALSE(parse(nullptr).has_value());
    }
}

TEST_CASE("Assignment rules: matcher", "[assignment-rules]") {
    AudienceMatcher matcher;
    const json attrs = {{"country", "US"}};

    SECTION("no action without a rules array") {
        REQUIRE_FALSE(matcher.evaluate_rules(nullptr, "", attrs).has_value());
        REQUIRE_FALSE(matcher.evaluate_rules(json::object(), "", attrs).has_value());
        REQUIRE_FALSE(matcher.evaluate_rules({{"rules", json::object()}}, "", attrs).has_value());
        REQUIRE_FALSE(matcher.evaluate_rules({{"rules", json::array()}}, "", attrs).has_value());
    }

    SECTION("non-object rules, unknown types and non-object conditions are skipped") {
        auto bad_conditions = assign_rule(0);
        bad_conditions["conditions"] = json::array({true});
        auto unknown = assign_rule(0);
        unknown["type"] = "other";
        auto action = matcher.evaluate_rules(
            {{"rules", json::array({nullptr, 5, unknown, bad_conditions, assign_rule(1)})}}, "", attrs);
        REQUIRE(action.has_value());
        REQUIRE_FALSE(action->is_split);
        REQUIRE(action->variant == 1);
    }

    SECTION("conditions must evaluate to true, not just be truthy-but-null") {
        auto missing = json::parse(R"({"eq":[{"var":"missing"},{"value":"x"}]})");
        REQUIRE_FALSE(matcher.evaluate_rules(rules_of({assign_rule(1, missing)}), "", attrs).has_value());
    }

    SECTION("split action carries the parsed fractions") {
        auto action = matcher.evaluate_rules(rules_of({split_rule("20/30/50")}), "", attrs);
        REQUIRE(action.has_value());
        REQUIRE(action->is_split);
        REQUIRE(action->split == std::vector<double>{0.2, 0.3, 0.5});
    }
}

TEST_CASE("Assignment rules: JSON model", "[assignment-rules]") {
    SECTION("assignmentRules is read when it is a string and ignored otherwise") {
        auto data = json::parse(R"({"experiments":[
            {"id":1,"name":"a","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,
             "assignmentRules":"{\"rules\":[{\"type\":\"assign\",\"variant\":1}]}"},
            {"id":2,"name":"b","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,"assignmentRules":{"rules":[]}},
            {"id":3,"name":"c","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,"assignmentRules":null},
            {"id":4,"name":"d","unitType":"u","iteration":1,"seedHi":0,"seedLo":0}
        ]})").get<ContextData>();
        REQUIRE(data.experiments[0].assignmentRules == R"({"rules":[{"type":"assign","variant":1}]})");
        REQUIRE(data.experiments[1].assignmentRules.empty());
        REQUIRE(data.experiments[2].assignmentRules.empty());
        REQUIRE(data.experiments[3].assignmentRules.empty());
    }

    SECTION("assignmentRules round-trips and is omitted when empty") {
        auto exp = make_exp_test_abc();
        REQUIRE_FALSE(json(exp).contains("assignmentRules"));
        exp.assignmentRules = R"({"rules":[]})";
        REQUIRE(json(exp).get<ExperimentData>().assignmentRules == exp.assignmentRules);
    }

    SECTION("Exposure ruleOverride round-trips and defaults to false") {
        Exposure e;
        e.ruleOverride = true;
        REQUIRE(json(e).get<Exposure>().ruleOverride);
        auto old = json(Exposure{});
        old.erase("ruleOverride");
        REQUIRE_FALSE(old.get<Exposure>().ruleOverride);
    }
}
