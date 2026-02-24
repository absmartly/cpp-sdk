#include "absmartly/hashing.h"
#include <cstring>

namespace absmartly {

static const char BASE64URL_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url_no_padding(const uint8_t* data, size_t len) {
    size_t remaining = len % 3;
    size_t full_triples = len / 3;
    size_t encode_len = full_triples * 4 + (remaining == 0 ? 0 : (remaining == 1 ? 2 : 3));

    std::string result;
    result.reserve(encode_len);

    size_t i = 0;
    size_t end = len - remaining;
    for (; i < end; i += 3) {
        uint32_t bytes = (static_cast<uint32_t>(data[i]) << 16)
                       | (static_cast<uint32_t>(data[i + 1]) << 8)
                       | static_cast<uint32_t>(data[i + 2]);
        result.push_back(BASE64URL_CHARS[(bytes >> 18) & 63]);
        result.push_back(BASE64URL_CHARS[(bytes >> 12) & 63]);
        result.push_back(BASE64URL_CHARS[(bytes >> 6) & 63]);
        result.push_back(BASE64URL_CHARS[bytes & 63]);
    }

    switch (remaining) {
        case 2: {
            uint32_t bytes = (static_cast<uint32_t>(data[i]) << 16)
                           | (static_cast<uint32_t>(data[i + 1]) << 8);
            result.push_back(BASE64URL_CHARS[(bytes >> 18) & 63]);
            result.push_back(BASE64URL_CHARS[(bytes >> 12) & 63]);
            result.push_back(BASE64URL_CHARS[(bytes >> 6) & 63]);
            break;
        }
        case 1: {
            uint32_t bytes = static_cast<uint32_t>(data[i]) << 16;
            result.push_back(BASE64URL_CHARS[(bytes >> 18) & 63]);
            result.push_back(BASE64URL_CHARS[(bytes >> 12) & 63]);
            break;
        }
        default:
            break;
    }

    return result;
}

std::string hash_unit(const std::string& unit) {
    auto raw = md5_raw(unit);
    return base64url_no_padding(raw.data(), raw.size());
}

int choose_variant(const std::vector<double>& split, double probability) {
    double cum_sum = 0.0;
    for (size_t i = 0; i < split.size(); ++i) {
        cum_sum += split[i];
        if (probability < cum_sum) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(split.size()) - 1;
}

} // namespace absmartly
