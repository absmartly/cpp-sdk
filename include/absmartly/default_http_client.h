#pragma once

#include <absmartly/http_client.h>

#include <curl/curl.h>

namespace absmartly {

class DefaultHTTPClient : public HTTPClient {
public:
    DefaultHTTPClient();
    ~DefaultHTTPClient() override;

    DefaultHTTPClient(const DefaultHTTPClient&) = delete;
    DefaultHTTPClient& operator=(const DefaultHTTPClient&) = delete;

    std::future<Response> get(const std::string& url,
                              const std::map<std::string, std::string>& query,
                              const std::map<std::string, std::string>& headers) override;

    std::future<Response> put(const std::string& url,
                              const std::map<std::string, std::string>& query,
                              const std::map<std::string, std::string>& headers,
                              const std::vector<uint8_t>& body) override;

    std::future<Response> post(const std::string& url,
                               const std::map<std::string, std::string>& query,
                               const std::map<std::string, std::string>& headers,
                               const std::vector<uint8_t>& body) override;

private:
    static Response perform_request(CURL* curl);
};

} // namespace absmartly
