#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace us4 {

// Deterministic placeholder tokenizer for the skeleton. It is not a real BPE
// model; it provides reproducible encode/decode so the generation loop and its
// tests are stable. Real tokenizers ship with model loading in a later sprint.
class Tokenizer {
 public:
  std::vector<std::uint32_t> encode(const std::string& text,
                                    std::uint32_t vocab) const;
  std::string decode(const std::vector<std::uint32_t>& tokens) const;
  std::string decode_token(std::uint32_t token) const;
};

}  // namespace us4
