#include "us4/hash.hpp"

#include "us4_test.hpp"

using namespace us4;

static void sha256_known_vectors() {
  // Standard NIST/FIPS-180 test vectors.
  US4_CHECK(sha256_hex("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  US4_CHECK(sha256_hex("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  US4_CHECK(
      sha256_hex("The quick brown fox jumps over the lazy dog") ==
      "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

static void hash_helpers() {
  // Deterministic and within 30 bits.
  std::uint32_t h = hamt_hash30("agent.dev.python");
  US4_CHECK(h == hamt_hash30("agent.dev.python"));
  US4_CHECK((h & 0xC0000000u) == 0);  // top 2 bits clear
  US4_CHECK(short_hash("abc").size() == 16);
  US4_CHECK(hamt_hash30("a") != hamt_hash30("b"));
}

int main() {
  US4_RUN(sha256_known_vectors);
  US4_RUN(hash_helpers);
  US4_MAIN_END();
}
