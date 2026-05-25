#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace us4 {

// Self-contained SHA-256 used for content-addressable tuple/receipt ids and as
// the hash source for the HAMT registry (truncated to 30 bits per the spec).

std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len);
std::string to_hex(const std::uint8_t* data, std::size_t len);
std::string sha256_hex(const std::string& data);

// 30-bit key hash for the HAMT (top 30 bits of the SHA-256 digest).
std::uint32_t hamt_hash30(const std::string& key);

// Short, stable receipt tag (first 16 hex chars of a SHA-256 digest).
std::string short_hash(const std::string& data);

}  // namespace us4
