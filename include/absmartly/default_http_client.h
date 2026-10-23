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
    static std::string build_url_with_query(const std::string& url,
                                            const std::map<std::string, std::string>& query);
    static struct curl_slist* build_header_list(const std::map<std::string, std::string>& headers);
    static Response perform_request(CURL* curl);
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace absmartly
