#include "curl_global.h"

#include <curl/curl.h>

#include <mutex>
#include <stdexcept>
#include <string>

namespace absmartly {
namespace detail {

void ensure_curl_global_init() {
    static std::once_flag flag;
    std::call_once(flag, [] {
        const CURLcode rc = curl_global_init(CURL_GLOBAL_ALL);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("curl_global_init failed: ") + curl_easy_strerror(rc));
        }
    });
}

} // namespace detail
} // namespace absmartly
