#include "us4/tokenizer.hpp"

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace us4 {

namespace {

std::uint32_t fnv1a(const std::string& s) {
  std::uint32_t h = 2166136261u;
  for (unsigned char c : s) {
    h ^= c;
    h *= 16777619u;
  }
  return h;
}

}  // namespace

std::vector<std::uint32_t> Tokenizer::encode(const std::string& text,
                                             std::uint32_t vocab) const {
  std::vector<std::uint32_t> tokens;
  if (vocab == 0) return tokens;
  std::string word;
  auto flush = [&]() {
    if (!word.empty()) {
      tokens.push_back(fnv1a(word) % vocab);
      word.clear();
    }
  };
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      flush();
    } else {
      word.push_back(c);
    }
  }
  flush();
  return tokens;
}

std::string Tokenizer::decode_token(std::uint32_t token) const {
  // Deterministic synthetic surface form. Not a real vocabulary; the skeleton
  // only needs reproducible, distinguishable tokens.
  static const char* cons = "bcdfghklmnprstvz";
  static const char* vowel = "aeiou";
  std::string s;
  std::uint32_t v = token;
  for (int i = 0; i < 3; ++i) {
    s.push_back(cons[v % 16]);
    v /= 16;
    s.push_back(vowel[v % 5]);
    v /= 5;
  }
  return s;
}

std::string Tokenizer::decode(const std::vector<std::uint32_t>& tokens) const {
  std::string out;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (i) out.push_back(' ');
    out += decode_token(tokens[i]);
  }
  return out;
}

}  // namespace us4
