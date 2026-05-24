#include "us4/hardware.hpp"

#include "us4_test.hpp"

using namespace us4;

static void detects_basic_machine() {
  HardwareInfo hw = HardwareProbe::detect();
  US4_CHECK(hw.platform != Platform::Unknown);
  US4_CHECK(hw.logical_cores >= 1);
  US4_CHECK(hw.total_ram_bytes > 0);
  US4_CHECK(hw.total_ram_gib() > 0.0);
}

static void arch_consistency() {
  HardwareInfo hw = HardwareProbe::detect();
  // NEON implies ARM; apple_silicon implies Apple + ARM.
  if (hw.has_neon) US4_CHECK(hw.arch == CpuArch::Arm64);
  if (hw.apple_silicon) {
    US4_CHECK(hw.platform == Platform::Apple);
    US4_CHECK(hw.arch == CpuArch::Arm64);
  }
}

static void string_helpers() {
  US4_CHECK(std::string(to_string(Platform::Linux)) == "Linux");
  US4_CHECK(std::string(to_string(CpuArch::X86_64)) == "x86_64");
}

int main() {
  US4_RUN(detects_basic_machine);
  US4_RUN(arch_consistency);
  US4_RUN(string_helpers);
  US4_MAIN_END();
}
