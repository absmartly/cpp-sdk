#include "absmartly/default_http_client.h"

#include "curl_global.h"
#include "curl_utils.h"

#include <sstream>
#include <mutex>
#include <stdexcept>

namespace absmartly {

DefaultHTTPClient::DefaultHTTPClient() {
    detail::ensure_curl_global_init();
}

DefaultHTTPClient::~DefaultHTTPClient() = default;

std::future<HTTPClient::Response> DefaultHTTPClient::get(const std::string& url,
                                                         const std::map<std::string, std::string>& query,
                                                         const std::map<std::string, std::string>& headers) {
    return std::async(std::launch::async, [url, query, headers]() -> HTTPClient::Response {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {500, "Failed to initialize CURL", "", {}};
        }

        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        std::string full_url = detail::build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        struct curl_slist* header_list = detail::build_header_list(headers);
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
        std::string full_url = detail::build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        // Set the request body BEFORE overriding the method. If CURLOPT_CUSTOMREQUEST
        // is set first, libcurl puts the transfer into upload (read-callback) mode and
        // fails the request with CURLE_READ_ERROR when only POSTFIELDS are supplied.
        // Supplying the post fields first makes this a body-bearing request that
        // CUSTOMREQUEST then simply relabels as "PUT".
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

        struct curl_slist* header_list = detail::build_header_list(headers);
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
        std::string full_url = detail::build_url_with_query(url, query);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));

        struct curl_slist* header_list = detail::build_header_list(headers);
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

HTTPClient::Response DefaultHTTPClient::perform_request(CURL* curl) {
    Response response;
    std::vector<uint8_t> response_body;

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, detail::write_callback);
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

} // namespace absmartly
