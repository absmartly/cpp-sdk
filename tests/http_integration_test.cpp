// Hermetic local-HTTP-server integration test.
//
// This drives the PUBLIC SDK surface (SDK::create_simple ->
// create_context -> treatment/track -> publish) so that the REAL libcurl-backed
// DefaultHTTPClient performs an actual GET /v1/context and PUT /v1/context
// against a local loopback server. It asserts the HTTP wire contract documented
// for the ABsmartly collector (see absmartly-wire-contract.md):
//
//   GET  <endpoint>/context   with ?application=&environment=  (drives ready)
//   PUT  <endpoint>/context   with X-API-Key / X-Application / X-Environment /
//                             X-Application-Version / X-Agent / Content-Type
//                             and a body containing hashed / units / publishedAt.
//
// NOTE: the C++ Client appends "/v1/context" to the configured endpoint, so the
// server is mounted at /v1/context here (the wire contract's "/v1" lives in the
// endpoint base in production). We assert the path the SDK actually requests.
//
// The server is a tiny single-threaded blocking-socket implementation so the
// test stays hermetic with no extra dependency (it reuses libcurl, already a
// dependency of the SDK itself, only on the client side).

#include <catch2/catch_test_macros.hpp>

#include <absmartly/sdk.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// A single captured HTTP request as seen by the local server.
struct CapturedRequest {
    std::string method;
    std::string path;   // includes query string, e.g. "/v1/context?application=..."
    std::map<std::string, std::string> headers; // lower-cased header names
    std::string body;
};

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Read until we have the full headers + (if present) Content-Length bytes of body.
bool read_request(int fd, CapturedRequest& out) {
    std::string buffer;
    char chunk[4096];

    auto header_end = [&]() { return buffer.find("\r\n\r\n"); };

    // Read until end of headers.
    while (header_end() == std::string::npos) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) return false;
        buffer.append(chunk, static_cast<size_t>(n));
    }

    size_t headers_len = header_end();
    std::string head = buffer.substr(0, headers_len);
    std::string rest = buffer.substr(headers_len + 4);

    // Request line.
    size_t line_end = head.find("\r\n");
    std::string request_line = head.substr(0, line_end);
    {
        size_t sp1 = request_line.find(' ');
        size_t sp2 = request_line.find(' ', sp1 + 1);
        out.method = request_line.substr(0, sp1);
        out.path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
    }

    // Headers.
    size_t pos = line_end + 2;
    size_t content_length = 0;
    while (pos < head.size()) {
        size_t eol = head.find("\r\n", pos);
        if (eol == std::string::npos) eol = head.size();
        std::string line = head.substr(pos, eol - pos);
        pos = eol + 2;
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = to_lower(line.substr(0, colon));
            std::string value = line.substr(colon + 1);
            // trim leading spaces.
            size_t start = value.find_first_not_of(" \t");
            value = (start == std::string::npos) ? "" : value.substr(start);
            out.headers[name] = value;
            if (name == "content-length") content_length = std::stoul(value);
        }
    }

    // Body (read remaining bytes if a Content-Length was given).
    std::string body = rest;
    while (body.size() < content_length) {
        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        body.append(chunk, static_cast<size_t>(n));
    }
    out.body = body;
    return true;
}

void send_response(int fd, int status, const std::string& status_text,
                   const std::string& body) {
    std::string resp = "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    ::send(fd, resp.data(), resp.size(), 0);
}

// A tiny hermetic HTTP server bound to an ephemeral loopback port. It serves
// exactly two requests (a GET then a PUT) and records what it saw.
class LocalServer {
public:
    LocalServer() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(listen_fd_ >= 0);

        int opt = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0; // ephemeral

        REQUIRE(::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
        REQUIRE(::listen(listen_fd_, 8) == 0);

        socklen_t len = sizeof(addr);
        REQUIRE(::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
        port_ = ntohs(addr.sin_port);
    }

    ~LocalServer() {
        if (listen_fd_ >= 0) ::close(listen_fd_);
        if (thread_.joinable()) thread_.join();
    }

    uint16_t port() const { return port_; }

    // Serve `count` requests on a background thread.
    void start(int count) {
        thread_ = std::thread([this, count]() {
            for (int i = 0; i < count; ++i) {
                int client = ::accept(listen_fd_, nullptr, nullptr);
                if (client < 0) return;

                CapturedRequest req;
                if (read_request(client, req)) {
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        requests_.push_back(req);
                    }
                    if (req.method == "GET") {
                        send_response(client, 200, "OK", R"({"experiments":[]})");
                    } else {
                        send_response(client, 200, "OK", "{}");
                    }
                }
                ::close(client);
            }
        });
    }

    std::vector<CapturedRequest> requests() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

    // Block (up to `timeout`) until at least `n` requests have been recorded.
    bool wait_for_requests(size_t n, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (requests_.size() >= n) return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_.size() >= n;
    }

private:
    int listen_fd_ = -1;
    uint16_t port_ = 0;
    std::thread thread_;
    std::mutex mutex_;
    std::vector<CapturedRequest> requests_;
};

} // namespace

TEST_CASE("Real HTTP client hits local server with the wire contract", "[integration][http]") {
    LocalServer server;
    // We expect two requests: the GET (createContext) and the PUT (publish).
    server.start(2);

    const std::string endpoint = "http://127.0.0.1:" + std::to_string(server.port());

    absmartly::ClientConfig client_config;
    client_config.endpoint = endpoint;
    client_config.api_key = "test-api-key";
    client_config.application = "www";
    client_config.environment = "test";

    // Default (async, libcurl-multi) client path — this is what SDK users get.
    auto sdk = absmartly::SDK::create_simple(client_config, absmartly::HTTPMode::async);

    absmartly::ContextConfig context_config;
    context_config.publish_delay = -1;
    context_config.units = {{"user_id", "123456"}};

    auto ctx = sdk->create_context(context_config);
    ctx->wait_until_ready();

    REQUIRE(ctx->is_ready());
    REQUIRE_FALSE(ctx->is_failed());

    // Queue an event so publish() has something to send.
    ctx->track("payment", {{"amount", 1000}});
    REQUIRE(ctx->pending() > 0);

    auto event = ctx->publish();

    // publish() dispatches the PUT asynchronously; wait until the server records it.
    REQUIRE(server.wait_for_requests(2, std::chrono::seconds(5)));

    auto requests = server.requests();
    REQUIRE(requests.size() == 2);

    // ---- GET /context (fetch -> ready) ----
    const auto& get = requests[0];
    REQUIRE(get.method == "GET");
    REQUIRE(get.path.rfind("/v1/context", 0) == 0); // path starts with /v1/context
    REQUIRE(get.path.find("application=www") != std::string::npos);
    REQUIRE(get.path.find("environment=test") != std::string::npos);

    // ---- PUT /context (publish) ----
    const auto& put = requests[1];
    REQUIRE(put.method == "PUT");
    REQUIRE(put.path.rfind("/v1/context", 0) == 0);
    // No query params on the PUT path.
    REQUIRE(put.path.find('?') == std::string::npos);

    // Required auth / identity headers (exact names per the wire contract).
    REQUIRE(put.headers.at("x-api-key") == "test-api-key");
    REQUIRE(put.headers.at("x-application") == "www");
    REQUIRE(put.headers.at("x-environment") == "test");
    REQUIRE(put.headers.at("x-application-version") == "0");
    REQUIRE(put.headers.count("x-agent") == 1);
    REQUIRE_FALSE(put.headers.at("x-agent").empty()); // value varies per SDK
    REQUIRE(put.headers.at("content-type").find("application/json") != std::string::npos);

    // Body must carry the required publish fields.
    auto body = nlohmann::json::parse(put.body);
    REQUIRE(body.contains("hashed"));
    REQUIRE(body["hashed"].is_boolean());
    REQUIRE(body.contains("units"));
    REQUIRE(body["units"].is_array());
    REQUIRE(body.contains("publishedAt"));
    REQUIRE(body["publishedAt"].get<int64_t>() > 0);
    // We tracked a goal, so goals must be present and non-empty.
    REQUIRE(body.contains("goals"));
    REQUIRE(body["goals"].is_array());
    REQUIRE_FALSE(body["goals"].empty());
    REQUIRE(body["goals"][0]["name"] == "payment");

    // The publish() call itself returns the event it sent.
    REQUIRE(event.publishedAt > 0);
}
