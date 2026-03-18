#include "absmartly/async_http_client.h"

#include <sstream>
#include <mutex>

namespace absmartly {

static std::once_flag curl_init_flag;

static void ensure_curl_initialized() {
    std::call_once(curl_init_flag, [] { curl_global_init(CURL_GLOBAL_ALL); });
}

AsyncHTTPClient::AsyncHTTPClient() {
    ensure_curl_initialized();
    multi_ = curl_multi_init();
    worker_ = std::thread(&AsyncHTTPClient::event_loop, this);
}

AsyncHTTPClient::~AsyncHTTPClient() {
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        running_ = false;
    }
    pending_cv_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }

    for (auto* req : pending_queue_) {
        if (req->headers) {
            curl_slist_free_all(req->headers);
        }
        curl_easy_cleanup(req->easy);
        req->promise.set_value({0, "Client shutting down", "", {}});
        delete req;
    }
    pending_queue_.clear();

    for (auto& [easy, req] : active_requests_) {
        curl_multi_remove_handle(multi_, easy);
        if (req->headers) {
            curl_slist_free_all(req->headers);
        }
        curl_easy_cleanup(easy);
        req->promise.set_value({0, "Client shutting down", "", {}});
        delete req;
    }

    curl_multi_cleanup(multi_);
}

std::future<HTTPClient::Response> AsyncHTTPClient::get(
    const std::string& url,
    const std::map<std::string, std::string>& query,
    const std::map<std::string, std::string>& headers) {

    CURL* easy = curl_easy_init();
    if (!easy) {
        std::promise<Response> p;
        p.set_value({500, "Failed to initialize CURL", "", {}});
        return p.get_future();
    }

    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);

    return enqueue(easy, build_url_with_query(url, query), build_header_list(headers), {});
}

std::future<HTTPClient::Response> AsyncHTTPClient::put(
    const std::string& url,
    const std::map<std::string, std::string>& query,
    const std::map<std::string, std::string>& headers,
    const std::vector<uint8_t>& body) {

    CURL* easy = curl_easy_init();
    if (!easy) {
        std::promise<Response> p;
        p.set_value({500, "Failed to initialize CURL", "", {}});
        return p.get_future();
    }

    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "PUT");

    std::vector<uint8_t> body_copy(body);
    return enqueue(easy, build_url_with_query(url, query), build_header_list(headers), std::move(body_copy));
}

std::future<HTTPClient::Response> AsyncHTTPClient::post(
    const std::string& url,
    const std::map<std::string, std::string>& query,
    const std::map<std::string, std::string>& headers,
    const std::vector<uint8_t>& body) {

    CURL* easy = curl_easy_init();
    if (!easy) {
        std::promise<Response> p;
        p.set_value({500, "Failed to initialize CURL", "", {}});
        return p.get_future();
    }

    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_POST, 1L);

    std::vector<uint8_t> body_copy(body);
    return enqueue(easy, build_url_with_query(url, query), build_header_list(headers), std::move(body_copy));
}

std::future<HTTPClient::Response> AsyncHTTPClient::enqueue(
    CURL* easy, std::string url, struct curl_slist* header_list, std::vector<uint8_t> request_body) {

    auto* req = new PendingRequest();
    req->easy = easy;
    req->url = std::move(url);
    req->headers = header_list;
    req->request_body = std::move(request_body);

    curl_easy_setopt(easy, CURLOPT_URL, req->url.c_str());
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, 30L);

    if (header_list) {
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, header_list);
    }

    if (!req->request_body.empty()) {
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, req->request_body.data());
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(req->request_body.size()));
    }

    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &req->response_body);

    auto future = req->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_queue_.push_back(req);
    }
    pending_cv_.notify_one();

    return future;
}

void AsyncHTTPClient::event_loop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            if (!running_ && pending_queue_.empty()) {
                break;
            }
            for (auto* req : pending_queue_) {
                CURLMcode mc = curl_multi_add_handle(multi_, req->easy);
                if (mc != CURLM_OK) {
                    if (req->headers) {
                        curl_slist_free_all(req->headers);
                    }
                    curl_easy_cleanup(req->easy);
                    req->promise.set_value({0, curl_multi_strerror(mc), "", {}});
                    delete req;
                } else {
                    active_requests_[req->easy] = req;
                }
            }
            pending_queue_.clear();
        }

        if (active_requests_.empty()) {
            std::unique_lock<std::mutex> lock(pending_mutex_);
            pending_cv_.wait_for(lock, std::chrono::milliseconds(100),
                                 [this] { return !pending_queue_.empty() || !running_; });
            continue;
        }

        int still_running = 0;
        curl_multi_perform(multi_, &still_running);
        process_completed();

        if (still_running > 0) {
            curl_multi_poll(multi_, nullptr, 0, 100, nullptr);
        }
    }
}

void AsyncHTTPClient::process_completed() {
    int msgs_left = 0;
    CURLMsg* msg;
    while ((msg = curl_multi_info_read(multi_, &msgs_left))) {
        if (msg->msg != CURLMSG_DONE) {
            continue;
        }

        CURL* easy = msg->easy_handle;
        auto it = active_requests_.find(easy);
        if (it == active_requests_.end()) {
            continue;
        }

        auto* req = it->second;
        active_requests_.erase(it);
        curl_multi_remove_handle(multi_, easy);

        Response response;
        if (msg->data.result != CURLE_OK) {
            response.status_code = 0;
            response.status_message = curl_easy_strerror(msg->data.result);
        } else {
            long http_code = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &http_code);
            response.status_code = static_cast<int>(http_code);

            char* content_type_ptr = nullptr;
            curl_easy_getinfo(easy, CURLINFO_CONTENT_TYPE, &content_type_ptr);
            if (content_type_ptr) {
                response.content_type = content_type_ptr;
            }

            response.content = std::move(req->response_body);
        }

        if (req->headers) {
            curl_slist_free_all(req->headers);
        }
        curl_easy_cleanup(easy);

        req->promise.set_value(std::move(response));
        delete req;
    }
}

std::string AsyncHTTPClient::build_url_with_query(const std::string& url,
                                                   const std::map<std::string, std::string>& query) {
    if (query.empty()) {
        return url;
    }

    std::ostringstream oss;
    oss << url;

    bool has_query = url.find('?') != std::string::npos;
    CURL* curl = curl_easy_init();
    if (curl) {
        for (const auto& [key, value] : query) {
            oss << (has_query ? '&' : '?');
            has_query = true;

            char* encoded_key = curl_easy_escape(curl, key.c_str(), static_cast<int>(key.size()));
            char* encoded_value = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
            oss << encoded_key << '=' << encoded_value;
            curl_free(encoded_key);
            curl_free(encoded_value);
        }
        curl_easy_cleanup(curl);
    }

    return oss.str();
}

struct curl_slist* AsyncHTTPClient::build_header_list(const std::map<std::string, std::string>& headers) {
    struct curl_slist* list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        list = curl_slist_append(list, header.c_str());
    }
    return list;
}

size_t AsyncHTTPClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total_size = size * nmemb;
    body->insert(body->end(), reinterpret_cast<uint8_t*>(ptr),
                 reinterpret_cast<uint8_t*>(ptr) + total_size);
    return total_size;
}

} // namespace absmartly
