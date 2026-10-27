#include "absmartly/hashing.h"
#include "number_format.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

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

std::string unit_to_string(const nlohmann::json& unit) {
    if (unit.is_string()) {
        return unit.get<std::string>();
    }
    if (unit.is_number()) {
        // Every number is a JavaScript Number (a double), so 64-bit integers
        // above 2^53 hash as their nearest double, as in JS.
        double d = unit.get<double>();
        if (!std::isfinite(d) || std::abs(d) >= 1e21) {
            // toFixed falls back to the shortest round-trip form here.
            if (std::isnan(d)) return "NaN";
            if (std::isinf(d)) return d < 0 ? "-Infinity" : "Infinity";
            return detail::large_number_to_string(d);
        }
        // toFixed(0): the nearest integer, ties to the larger magnitude, and a
        // "-" sign for any negative input (-0.4 -> "-0").
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f", std::round(std::abs(d)));
        return (d < 0 ? "-" : "") + std::string(buf);
    }
    throw std::invalid_argument("unit uid must be a string or a number");
}

std::string hash_unit(const nlohmann::json& unit) {
    return hash_unit(unit_to_string(unit));
}

int choose_variant(const std::vector<double>& split, double probability) {
    if (split.empty()) {
        return 0;
    }
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
