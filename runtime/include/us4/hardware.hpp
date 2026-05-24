#pragma once

#include <cstdint>
#include <string>

namespace us4 {

enum class Platform { Apple, Linux, Windows, Unknown };
enum class CpuArch { Arm64, X86_64, Other };

// Snapshot of the host machine, produced by HardwareProbe::detect().
struct HardwareInfo {
  Platform platform = Platform::Unknown;
  CpuArch arch = CpuArch::Other;
  unsigned logical_cores = 0;
  std::uint64_t total_ram_bytes = 0;
  bool has_neon = false;        // ARM SIMD
  bool has_avx = false;         // x86 SIMD (AVX2)
  bool apple_silicon = false;
  std::string cpu_brand;        // best-effort, may be empty
  std::string platform_name;    // human-readable

  double total_ram_gib() const;
};

class HardwareProbe {
 public:
  static HardwareInfo detect();
};

const char* to_string(Platform);
const char* to_string(CpuArch);

}  // namespace us4
