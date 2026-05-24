#pragma once

#include <cstdint>

#include "us4/hardware.hpp"

namespace us4 {

// Runtime modes map RAM tiers to capability envelopes. These are scheduling
// hints, never hard performance guarantees (see the no-hardcoded-benchmarks
// rule in the spec).
enum class RuntimeMode {
  Full,          // 128 GB  - frontier MoE
  BalancedPlus,  // 96 GB   - universal target
  Balanced,      // 64 GB
  Performance,   // 48 GB
  Standard,      // 32 GB
  Compact,       // 24 GB
  Nano,          // 16 GB   - small 1B-3B dense only
};

struct ModeProfile {
  RuntimeMode mode = RuntimeMode::Nano;
  const char* name = "Nano";
  std::uint32_t min_ram_gib = 16;
  const char* target_models = "small dense (1B-3B)";
};

class ModeSelector {
 public:
  // Highest tier whose RAM requirement fits the detected memory; floors at Nano.
  static ModeProfile select(const HardwareInfo& hw);
  static ModeProfile profile(RuntimeMode m);
};

const char* to_string(RuntimeMode);

}  // namespace us4
