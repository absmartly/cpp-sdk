#include <absmartly/context.h>
#include <absmartly/sdk.h>
#include <absmartly/sdk_version.h>

#include <iostream>
#include <string>
#include <utility>

int main() {
    absmartly::ContextData data;
    // ContextData had one member in 1.0.0. Existing bindings must keep that shape.
    auto& [experiments] = data;
    const auto& [const_experiments] = std::as_const(data);
    auto [copied_experiments] = data;
    auto&& [moved_experiments] = std::move(data);
    if (&experiments != &data.experiments || &const_experiments != &data.experiments ||
        &moved_experiments != &data.experiments || !copied_experiments.empty()) return 2;
    absmartly::ExperimentData released;
    released.name = "released";
    released.assignmentRules = "rules";
    auto& [id, name, unitType, iteration, seedHi, seedLo, split, seedStrictMode, trafficSeedHi, trafficSeedLo,
           trafficSplit, fullOnVariant, audience, variants, audienceStrict, customFieldValues, application,
           environment, assignmentRules] = released;
    const auto& [const_id, const_name, cu, ci, csh, csl, cs, cssm, ctsh, ctsl, cts, cfov, ca, cv, cas, ccfv, capp,
                 cenv, const_rules] = std::as_const(released);
    if (&name != &released.name || &assignmentRules != &released.assignmentRules ||
        &const_rules != &released.assignmentRules || name != "released") return 3;
    absmartly::ExperimentData exp;
    exp.id = 1;
    exp.name = "exp";
    exp.unitType = "session_id";
    exp.iteration = 1;
    exp.split = {0.5, 0.5};
    exp.trafficSplit = {0.0, 1.0};
    exp.variants = {{"A", nullptr}, {"B", nullptr}};
    exp.assignmentRules = R"({"rules":[{"type":"assign","variant":1}]})";
    data.experiments.push_back(exp);

    absmartly::ContextConfig config;
    config.units = {{"session_id", "abc"}};
    absmartly::Context context(config, data);

    if (context.treatment("exp") != 1) {
        std::cerr << "unexpected variant\n";
        return 1;
    }
    std::cout << absmartly::SDK_AGENT << "\n";
    return 0;
}
