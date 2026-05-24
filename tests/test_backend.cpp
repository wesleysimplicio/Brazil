#include "us4/backend.hpp"

#include <vector>

#include "us4_test.hpp"

using namespace us4;

static HardwareInfo linux_x86() {
  HardwareInfo hw;
  hw.platform = Platform::Linux;
  hw.arch = CpuArch::X86_64;
  hw.total_ram_bytes = 16ull * 1024 * 1024 * 1024;
  return hw;
}

static HardwareInfo apple_arm() {
  HardwareInfo hw;
  hw.platform = Platform::Apple;
  hw.arch = CpuArch::Arm64;
  hw.has_neon = true;
  hw.apple_silicon = true;
  hw.total_ram_bytes = 64ull * 1024 * 1024 * 1024;
  return hw;
}

static void cpu_backend_matvec_is_correct() {
  auto b = BackendSelector::select(linux_x86());
  US4_CHECK(b->available());
  US4_CHECK_EQ(b->kind(), BackendKind::GenericCpu);
  // [[1,2],[3,4]] * [1,1] = [3,7]
  std::vector<float> w = {1, 2, 3, 4};
  std::vector<float> x = {1, 1};
  std::vector<float> out(2, 0);
  b->matvec(w.data(), x.data(), out.data(), 2, 2);
  US4_CHECK_EQ(out[0], 3.0f);
  US4_CHECK_EQ(out[1], 7.0f);
}

static void apple_prefers_then_falls_back() {
  // MLX/Metal/ANE are planned (unavailable) -> falls back to NEON/Accelerate.
  auto b = BackendSelector::select(apple_arm());
  US4_CHECK(b->available());
  US4_CHECK_EQ(b->kind(), BackendKind::NeonAccelerate);

  auto cands = BackendSelector::candidates(apple_arm());
  US4_CHECK(cands.size() == 5);
  US4_CHECK_EQ(cands.front().kind, BackendKind::MLX);
  US4_CHECK(!cands.front().available);  // not integrated yet
}

static void prefer_unavailable_falls_back_to_cpu() {
  auto b = BackendSelector::select(linux_x86(), "mlx");
  US4_CHECK(b->available());
  US4_CHECK_EQ(b->kind(), BackendKind::GenericCpu);
}

static void parsing_backend_names() {
  BackendKind k;
  US4_CHECK(parse_backend_kind("CPU", k) && k == BackendKind::GenericCpu);
  US4_CHECK(parse_backend_kind("neon", k) && k == BackendKind::NeonAccelerate);
  US4_CHECK(parse_backend_kind("MLX", k) && k == BackendKind::MLX);
  US4_CHECK(!parse_backend_kind("bogus", k));
}

int main() {
  US4_RUN(cpu_backend_matvec_is_correct);
  US4_RUN(apple_prefers_then_falls_back);
  US4_RUN(prefer_unavailable_falls_back_to_cpu);
  US4_RUN(parsing_backend_names);
  US4_MAIN_END();
}
