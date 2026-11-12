#include <absmartly/context.h>
#include <absmartly/sdk.h>
#include <absmartly/sdk_version.h>

#include <iostream>
#include <string>

int main() {
    absmartly::ContextData data;
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
