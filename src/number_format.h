#pragma once

#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace absmartly::detail {

inline std::string large_number_to_string(double value) {
    std::string result;
    for (int precision = 1; precision <= 17; ++precision) {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::setprecision(precision) << value;
        result = stream.str();
        // A short candidate can round past DBL_MAX; keep searching.
        if (std::strtod(result.c_str(), nullptr) == value) break;
    }
    return result;
}

} // namespace absmartly::detail
