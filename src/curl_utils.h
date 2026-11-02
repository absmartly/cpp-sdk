#pragma once

#include <curl/curl.h>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace absmartly::detail {

inline std::string build_url_with_query(const std::string& url,
                                        const std::map<std::string, std::string>& query) {
    if (query.empty()) return url;
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
            if (!encoded_key || !encoded_value) {
                curl_free(encoded_key);
                curl_free(encoded_value);
                curl_easy_cleanup(curl);
                throw std::runtime_error("Failed to URL-encode query parameter");
            }
            oss << encoded_key << '=' << encoded_value;
            curl_free(encoded_key);
            curl_free(encoded_value);
        }
        curl_easy_cleanup(curl);
    }
    return oss.str();
}

inline curl_slist* build_header_list(const std::map<std::string, std::string>& headers) {
    curl_slist* list = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        list = curl_slist_append(list, header.c_str());
    }
    return list;
}

inline size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total_size = size * nmemb;
    body->insert(body->end(), reinterpret_cast<uint8_t*>(ptr),
                 reinterpret_cast<uint8_t*>(ptr) + total_size);
    return total_size;
}

} // namespace absmartly::detail
