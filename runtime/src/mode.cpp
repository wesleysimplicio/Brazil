#include "us4/mode.hpp"

#include <array>

namespace us4 {

namespace {

// Highest tier first. select() walks this and takes the first that fits.
constexpr std::array<ModeProfile, 7> kProfiles = {{
    {RuntimeMode::Full, "Full", 128, "frontier MoE (DeepSeek/Kimi/MiniMax)"},
    {RuntimeMode::BalancedPlus, "Balanced+", 96, "universal target, large MoE"},
    {RuntimeMode::Balanced, "Balanced", 64, "mid MoE + large dense"},
    {RuntimeMode::Performance, "Performance", 48, "large dense, small MoE"},
    {RuntimeMode::Standard, "Standard", 32, "dense up to ~14B"},
    {RuntimeMode::Compact, "Compact", 24, "dense up to ~8B"},
    {RuntimeMode::Nano, "Nano", 16, "small dense (1B-3B)"},
}};

}  // namespace

ModeProfile ModeSelector::select(const HardwareInfo& hw) {
  const double ram_gib = hw.total_ram_gib();
  for (const auto& p : kProfiles) {
    if (ram_gib + 0.5 >= static_cast<double>(p.min_ram_gib)) return p;
  }
  return kProfiles.back();  // Nano floor
}

ModeProfile ModeSelector::profile(RuntimeMode m) {
  for (const auto& p : kProfiles) {
    if (p.mode == m) return p;
  }
  return kProfiles.back();
}

const char* to_string(RuntimeMode m) { return ModeSelector::profile(m).name; }

}  // namespace us4
