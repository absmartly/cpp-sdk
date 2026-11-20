#include <catch2/catch_test_macros.hpp>
#include <absmartly/client.h>
#include "switchback_support.h"

using namespace absmartly;

namespace {

class PayloadHTTPClient : public HTTPClient {
public:
    explicit PayloadHTTPClient(std::string body) : body_(std::move(body)) {}
    std::future<Response> get(const std::string&, const std::map<std::string, std::string>&,
                              const std::map<std::string, std::string>&) override {
        Response response;
        response.status_code = 200;
        response.content.assign(body_.begin(), body_.end());
        std::promise<Response> p;
        p.set_value(std::move(response));
        return p.get_future();
    }
    std::future<Response> put(const std::string&, const std::map<std::string, std::string>&,
                              const std::map<std::string, std::string>&, const std::vector<uint8_t>&) override {
        return get({}, {}, {});
    }
    std::future<Response> post(const std::string&, const std::map<std::string, std::string>&,
                               const std::map<std::string, std::string>&, const std::vector<uint8_t>&) override {
        return get({}, {}, {});
    }

private:
    std::string body_;
};

ClientConfig config() {
    ClientConfig c;
    c.endpoint = "https://example.absmartly.io/v1";
    c.api_key = "key";
    c.application = "website";
    c.environment = "production";
    return c;
}

}

TEST_CASE("Switchback client: a fetched payload keeps each cluster map in its served order", "[switchback][client]") {
    Client client(config(), std::make_shared<PayloadHTTPClient>(
                                R"({"experiments":[{"id":1,"name":"s","unitType":"u","iteration":1,"seedHi":0,"seedLo":0,)"
                                R"("randomizationType":"switchback","switchbackClusters":{"user_id":"a","device_id":"b","store_id":"c"}}]})"));
    const auto data = client.get_context_data().get();
    std::vector<std::string> keys;
    const auto& map = data.experiments.at(0).switchbackClusters;
    for (auto it = map.begin(); it != map.end(); ++it) keys.push_back(it.key());
    REQUIRE(keys == std::vector<std::string>{"user_id", "device_id", "store_id"});
}

TEST_CASE("Switchback: malformed known fields stay forward compatible", "[switchback][models][compat]") {
    const auto payload = nlohmann::ordered_json::parse(
        R"({"experiments":[{"id":1,"name":"sb","unitType":"store_id","iteration":1,"seedHi":0,"seedLo":0,"split":[0.5,0.5],"trafficSplit":[0,1],"variants":[{"name":"A","config":null},{"name":"B","config":null}],"randomizationType":"switchback","switchbackClusters":{"store_id":"city"},"switchbackClusterValues":"not-an-array","switchbackStartAt":"soon","switchbackPeriodMs":1.5,"switchbackEstimand":{"future":true}},{"id":2,"name":"unit","unitType":"store_id","iteration":1,"seedHi":0,"seedLo":0,"split":[0.5,0.5],"trafficSplit":[0,1],"variants":[{"name":"A","config":null},{"name":"B","config":null}]}]})");
    Client client(config(), std::make_shared<PayloadHTTPClient>(payload.dump()));
    const auto data = client.get_context_data().get();
    struct Log : ContextEventLogger {
        int errors = 0;
        void handle_event(Context&, const std::string& type, const nlohmann::json&) override {
            if (type == "error") ++errors;
        }
    };
    auto log = std::make_shared<Log>();
    ContextConfig cfg;
    cfg.units["store_id"] = "u";
    Context context(cfg, data, log);
    REQUIRE(context.is_ready());
    REQUIRE_FALSE(context.is_failed());
    context.set_attribute("city", "Porto");
    REQUIRE(context.treatment("sb") == 0);
    context.treatment("unit");
    const auto event = context.publish();
    REQUIRE(event.exposures.size() == 1);
    REQUIRE(event.exposures[0].name == "unit");
    REQUIRE(event.exposures[0].assigned);
    REQUIRE(log->errors == 0);
    REQUIRE(data.experiments.size() == 2);
    REQUIRE(data.experiments[0].switchbackClusters.is_object());
    REQUIRE_FALSE(data.experiments[0].switchbackClusterValues.has_value());
    REQUIRE_FALSE(data.experiments[0].switchbackStartAt.has_value());
    REQUIRE_FALSE(data.experiments[0].switchbackPeriodMs.has_value());
    REQUIRE(data.experiments[0].switchbackEstimand.empty());
    REQUIRE(data.experiments[1].name == "unit");
}

TEST_CASE("Switchback client: a payload without switchbacks decodes as before", "[switchback][client]") {
    Client client(config(), std::make_shared<PayloadHTTPClient>(
        R"({"experiments":[{"id":1,"name":"e","unitType":"u","iteration":1,"seedHi":0,"seedLo":0}]})"));
    const auto data = client.get_context_data().get();
    REQUIRE(data.experiments.size() == 1);
    REQUIRE(data.experiments[0].switchbackClusters.is_null());
}
