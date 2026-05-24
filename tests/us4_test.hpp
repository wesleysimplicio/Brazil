#pragma once

// Minimal dependency-free assertion harness so the test suite builds offline.
// Replaceable with GoogleTest in a later sprint without touching call sites.

#include <cstdio>

namespace us4test {
inline int g_failures = 0;
inline int g_checks = 0;
}  // namespace us4test

#define US4_CHECK(cond)                                                    \
  do {                                                                     \
    ++us4test::g_checks;                                                   \
    if (!(cond)) {                                                         \
      ++us4test::g_failures;                                               \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                      \
  } while (0)

#define US4_CHECK_EQ(a, b)                                                     \
  do {                                                                         \
    ++us4test::g_checks;                                                       \
    if (!((a) == (b))) {                                                       \
      ++us4test::g_failures;                                                   \
      std::fprintf(stderr, "FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a,   \
                   #b);                                                        \
    }                                                                          \
  } while (0)

#define US4_RUN(fn)                          \
  do {                                       \
    std::fprintf(stderr, "[run] %s\n", #fn); \
    fn();                                    \
  } while (0)

#define US4_MAIN_END()                                                     \
  do {                                                                     \
    std::fprintf(stderr, "checks=%d failures=%d\n", us4test::g_checks,     \
                 us4test::g_failures);                                     \
    return us4test::g_failures == 0 ? 0 : 1;                               \
  } while (0)
