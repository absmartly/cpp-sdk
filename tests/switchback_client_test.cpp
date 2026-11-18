#include <catch2/catch_test_macros.hpp>
#include <absmartly/client.h>

using namespace absmartly;

namespace {

// Serves one context payload body.
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

} // namespace

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

TEST_CASE("Switchback client: a payload without switchbacks decodes as before", "[switchback][client]") {
    Client client(config(), std::make_shared<PayloadHTTPClient>(
                                R"({"experiments":[{"id":1,"name":"e","unitType":"u","iteration":1,"seedHi":0,"seedLo":0}]})"));
    const auto data = client.get_context_data().get();
    REQUIRE(data.experiments.size() == 1);
    REQUIRE(data.experiments[0].switchbackClusters.is_null());
}
