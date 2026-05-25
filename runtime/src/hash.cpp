#include "us4/hash.hpp"

#include <cstring>

namespace us4 {

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

}  // namespace

std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len) {
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  const std::uint64_t bit_len = static_cast<std::uint64_t>(len) * 8;
  const std::size_t total = ((len + 8) / 64 + 1) * 64;

  auto byte_at = [&](std::size_t i) -> std::uint8_t {
    if (i < len) return data[i];
    if (i == len) return 0x80;
    if (i >= total - 8) {
      const int shift = static_cast<int>((total - 1 - i) * 8);
      return static_cast<std::uint8_t>((bit_len >> shift) & 0xff);
    }
    return 0x00;
  };

  std::uint32_t w[64];
  for (std::size_t chunk = 0; chunk < total; chunk += 64) {
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(byte_at(chunk + i * 4)) << 24) |
             (static_cast<std::uint32_t>(byte_at(chunk + i * 4 + 1)) << 16) |
             (static_cast<std::uint32_t>(byte_at(chunk + i * 4 + 2)) << 8) |
             (static_cast<std::uint32_t>(byte_at(chunk + i * 4 + 3)));
    }
    for (int i = 16; i < 64; ++i) {
      std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      std::uint32_t ch = (e & f) ^ (~e & g);
      std::uint32_t t1 = hh + s1 + ch + kK[i] + w[i];
      std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      std::uint32_t t2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<std::uint8_t>((h[i] >> 24) & 0xff);
    out[i * 4 + 1] = static_cast<std::uint8_t>((h[i] >> 16) & 0xff);
    out[i * 4 + 2] = static_cast<std::uint8_t>((h[i] >> 8) & 0xff);
    out[i * 4 + 3] = static_cast<std::uint8_t>(h[i] & 0xff);
  }
  return out;
}

std::string to_hex(const std::uint8_t* data, std::size_t len) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(digits[data[i] >> 4]);
    out.push_back(digits[data[i] & 0x0f]);
  }
  return out;
}

std::string sha256_hex(const std::string& data) {
  auto d = sha256(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
  return to_hex(d.data(), d.size());
}

std::uint32_t hamt_hash30(const std::string& key) {
  auto d = sha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size());
  std::uint32_t top = (static_cast<std::uint32_t>(d[0]) << 24) |
                      (static_cast<std::uint32_t>(d[1]) << 16) |
                      (static_cast<std::uint32_t>(d[2]) << 8) |
                      static_cast<std::uint32_t>(d[3]);
  return top & 0x3fffffff;  // 30 bits
}

std::string short_hash(const std::string& data) {
  return sha256_hex(data).substr(0, 16);
}

}  // namespace us4
