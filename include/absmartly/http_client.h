#pragma once

#include <cstdint>
#include <future>
#include <map>
#include <string>
#include <vector>

namespace absmartly {

class HTTPClient {
public:
    struct Response {
        int status_code = 0;
        std::string status_message;
        std::string content_type;
        std::vector<uint8_t> content;
    };

    virtual ~HTTPClient() = default;

    virtual std::future<Response> get(const std::string& url,
                                      const std::map<std::string, std::string>& query,
                                      const std::map<std::string, std::string>& headers) = 0;

    virtual std::future<Response> put(const std::string& url,
                                      const std::map<std::string, std::string>& query,
                                      const std::map<std::string, std::string>& headers,
                                      const std::vector<uint8_t>& body) = 0;

    virtual std::future<Response> post(const std::string& url,
                                       const std::map<std::string, std::string>& query,
                                       const std::map<std::string, std::string>& headers,
                                       const std::vector<uint8_t>& body) = 0;
};

} // namespace absmartly
