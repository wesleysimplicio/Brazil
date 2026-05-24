#include "us4/hardware.hpp"

#include <thread>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <unistd.h>

#include <fstream>
#include <string>
#endif

namespace us4 {

double HardwareInfo::total_ram_gib() const {
  return static_cast<double>(total_ram_bytes) / (1024.0 * 1024.0 * 1024.0);
}

namespace {

CpuArch detect_arch() {
#if defined(__aarch64__) || defined(__arm64__)
  return CpuArch::Arm64;
#elif defined(__x86_64__) || defined(_M_X64)
  return CpuArch::X86_64;
#else
  return CpuArch::Other;
#endif
}

bool detect_avx() {
#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2");
#else
  return false;
#endif
#else
  return false;
#endif
}

std::uint64_t detect_ram_bytes() {
#if defined(__APPLE__)
  std::uint64_t mem = 0;
  std::size_t len = sizeof(mem);
  if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) return mem;
  return 0;
#elif defined(__linux__)
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && page_size > 0) {
    return static_cast<std::uint64_t>(pages) *
           static_cast<std::uint64_t>(page_size);
  }
  return 0;
#else
  return 0;
#endif
}

std::string detect_cpu_brand() {
#if defined(__APPLE__)
  char buf[256] = {0};
  std::size_t len = sizeof(buf);
  if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0) {
    return std::string(buf);
  }
  return {};
#elif defined(__linux__)
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  while (std::getline(f, line)) {
    const std::string key = "model name";
    if (line.rfind(key, 0) == 0) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        auto start = line.find_first_not_of(" \t", pos + 1);
        if (start != std::string::npos) return line.substr(start);
      }
    }
  }
  return {};
#else
  return {};
#endif
}

Platform detect_platform() {
#if defined(__APPLE__)
  return Platform::Apple;
#elif defined(__linux__)
  return Platform::Linux;
#elif defined(_WIN32)
  return Platform::Windows;
#else
  return Platform::Unknown;
#endif
}

}  // namespace

HardwareInfo HardwareProbe::detect() {
  HardwareInfo info;
  info.platform = detect_platform();
  info.arch = detect_arch();
  info.logical_cores = std::thread::hardware_concurrency();
  info.total_ram_bytes = detect_ram_bytes();
  info.has_avx = detect_avx();
  info.has_neon = (info.arch == CpuArch::Arm64);
  info.apple_silicon =
      (info.platform == Platform::Apple && info.arch == CpuArch::Arm64);
  info.cpu_brand = detect_cpu_brand();
  info.platform_name = to_string(info.platform);
  return info;
}

const char* to_string(Platform p) {
  switch (p) {
    case Platform::Apple:
      return "Apple";
    case Platform::Linux:
      return "Linux";
    case Platform::Windows:
      return "Windows";
    case Platform::Unknown:
      break;
  }
  return "Unknown";
}

const char* to_string(CpuArch a) {
  switch (a) {
    case CpuArch::Arm64:
      return "arm64";
    case CpuArch::X86_64:
      return "x86_64";
    case CpuArch::Other:
      break;
  }
  return "other";
}

}  // namespace us4
