#include "us4/mode.hpp"

#include "us4_test.hpp"

using namespace us4;

static HardwareInfo with_ram_gib(double gib) {
  HardwareInfo hw;
  hw.total_ram_bytes =
      static_cast<std::uint64_t>(gib * 1024.0 * 1024.0 * 1024.0);
  return hw;
}

static void profiles_have_expected_floors() {
  US4_CHECK_EQ(ModeSelector::profile(RuntimeMode::Full).min_ram_gib, 128u);
  US4_CHECK_EQ(ModeSelector::profile(RuntimeMode::Nano).min_ram_gib, 16u);
}

static void selects_by_ram_tier() {
  US4_CHECK(ModeSelector::select(with_ram_gib(256)).mode == RuntimeMode::Full);
  US4_CHECK(ModeSelector::select(with_ram_gib(128)).mode == RuntimeMode::Full);
  US4_CHECK(ModeSelector::select(with_ram_gib(96)).mode ==
            RuntimeMode::BalancedPlus);
  US4_CHECK(ModeSelector::select(with_ram_gib(64)).mode ==
            RuntimeMode::Balanced);
  US4_CHECK(ModeSelector::select(with_ram_gib(50)).mode ==
            RuntimeMode::Performance);
  US4_CHECK(ModeSelector::select(with_ram_gib(16)).mode == RuntimeMode::Nano);
}

static void floors_at_nano() {
  US4_CHECK(ModeSelector::select(with_ram_gib(8)).mode == RuntimeMode::Nano);
  US4_CHECK(ModeSelector::select(with_ram_gib(4)).mode == RuntimeMode::Nano);
}

int main() {
  US4_RUN(profiles_have_expected_floors);
  US4_RUN(selects_by_ram_tier);
  US4_RUN(floors_at_nano);
  US4_MAIN_END();
}
