#pragma once

#include <absmartly/http_client.h>

#include <curl/curl.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace absmartly {

class AsyncHTTPClient : public HTTPClient {
public:
    AsyncHTTPClient();
    ~AsyncHTTPClient() override;

    AsyncHTTPClient(const AsyncHTTPClient&) = delete;
    AsyncHTTPClient& operator=(const AsyncHTTPClient&) = delete;

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
    struct PendingRequest {
        CURL* easy;
        std::string url;
        std::promise<Response> promise;
        std::vector<uint8_t> response_body;
        struct curl_slist* headers;
        std::vector<uint8_t> request_body;
    };

    std::future<Response> enqueue(CURL* easy, std::string url,
                                   struct curl_slist* header_list,
                                   std::vector<uint8_t> request_body,
                                   const char* method_override = nullptr);
    void event_loop();
    void process_completed();

    static std::string build_url_with_query(const std::string& url,
                                            const std::map<std::string, std::string>& query);
    static struct curl_slist* build_header_list(const std::map<std::string, std::string>& headers);
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);

    CURLM* multi_;
    std::thread worker_;
    bool running_{true};

    std::mutex pending_mutex_;
    std::vector<PendingRequest*> pending_queue_;
    std::condition_variable pending_cv_;

    std::map<CURL*, PendingRequest*> active_requests_;
};

} // namespace absmartly
