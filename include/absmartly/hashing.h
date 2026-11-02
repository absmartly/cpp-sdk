#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace absmartly {

uint32_t murmur3_32(const uint8_t* data, size_t len, uint32_t seed);
uint32_t murmur3_32(const std::string& str, uint32_t seed);

std::string md5_hex(const std::string& input);
std::vector<uint8_t> md5_raw(const std::string& input);

std::string hash_unit(const std::string& unit);
// Hashes a string or number uid. Numbers are first rounded to an integer
// string, as JavaScript's toFixed(0) does (125.75 -> "126").
std::string hash_unit(const nlohmann::json& unit);
std::string unit_to_string(const nlohmann::json& unit);
inline std::string hash_unit(const char* unit) { return hash_unit(std::string(unit)); }
std::string base64url_no_padding(const uint8_t* data, size_t len);

int choose_variant(const std::vector<double>& split, double probability);

} // namespace absmartly
