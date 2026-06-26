#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace absmartly {

uint32_t murmur3_32(const uint8_t* data, size_t len, uint32_t seed);
uint32_t murmur3_32(const std::string& str, uint32_t seed);

std::string md5_hex(const std::string& input);
std::vector<uint8_t> md5_raw(const std::string& input);

std::string hash_unit(const std::string& unit);
std::string base64url_no_padding(const uint8_t* data, size_t len);

int choose_variant(const std::vector<double>& split, double probability);

} // namespace absmartly
