#include "absmartly/default_http_client.h"

#include <sstream>
#include <mutex>

namespace absmartly {

static std::once_flag default_curl_init_flag;

static void ensure_default_curl_initialized() {
    std::call_once(default_curl_init_flag, [] { curl_global_init(CURL_GLOBAL_ALL); });
}

DefaultHTTPClient::DefaultHTTPClient() {
    ensure_default_curl_initialized();
}

DefaultHTTPClient::~DefaultHTTPClient() {
}

std::future<HTTPClient::Response> DefaultHTTPClient::get(const std::string& url,
                                                         const std::map<std::string, std::string>& query,
                                                         const std::map<std::string, std::string>& headers) {
    return std::async(std::launch::async, [url, query, headers]() -> HTTPClient::Response {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {500, "Failed to initialize CURL", "", {}};
        }

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        std::string full_url = build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        struct curl_slist* header_list = build_header_list(headers);
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        auto response = perform_request(curl);

        if (header_list) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);

        return response;
    });
}

std::future<HTTPClient::Response> DefaultHTTPClient::put(const std::string& url,
                                                         const std::map<std::string, std::string>& query,
                                                         const std::map<std::string, std::string>& headers,
                                                         const std::vector<uint8_t>& body) {
    return std::async(std::launch::async, [url, query, headers, body]() -> HTTPClient::Response {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {500, "Failed to initialize CURL", "", {}};
        }

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        std::string full_url = build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        // Set the request body BEFORE overriding the method. If CURLOPT_CUSTOMREQUEST
        // is set first, libcurl puts the transfer into upload (read-callback) mode and
        // fails the request with CURLE_READ_ERROR when only POSTFIELDS are supplied.
        // Supplying the post fields first makes this a body-bearing request that
        // CUSTOMREQUEST then simply relabels as "PUT".
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist* header_list = build_header_list(headers);
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        auto response = perform_request(curl);

        if (header_list) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);

        return response;
    });
}

std::future<HTTPClient::Response> DefaultHTTPClient::post(const std::string& url,
                                                          const std::map<std::string, std::string>& query,
                                                          const std::map<std::string, std::string>& headers,
                                                          const std::vector<uint8_t>& body) {
    return std::async(std::launch::async, [url, query, headers, body]() -> HTTPClient::Response {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {500, "Failed to initialize CURL", "", {}};
        }

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        std::string full_url = build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

        struct curl_slist* header_list = build_header_list(headers);
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }

        auto response = perform_request(curl);

        if (header_list) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);

        return response;
    });
}

std::string DefaultHTTPClient::build_url_with_query(const std::string& url,
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

struct curl_slist* DefaultHTTPClient::build_header_list(const std::map<std::string, std::string>& headers) {
    struct curl_slist* list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        list = curl_slist_append(list, header.c_str());
    }
    return list;
}

HTTPClient::Response DefaultHTTPClient::perform_request(CURL* curl) {
    Response response;
    std::vector<uint8_t> response_body;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.status_code = 0;
        response.status_message = curl_easy_strerror(res);
        return response;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = static_cast<int>(http_code);

    char* content_type_ptr = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type_ptr);
    if (content_type_ptr) {
        response.content_type = content_type_ptr;
    }

    response.content = std::move(response_body);
    return response;
}

size_t DefaultHTTPClient::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total_size = size * nmemb;
    body->insert(body->end(), reinterpret_cast<uint8_t*>(ptr),
                 reinterpret_cast<uint8_t*>(ptr) + total_size);
    return total_size;
}

} // namespace absmartly
