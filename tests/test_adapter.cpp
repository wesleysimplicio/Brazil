#include "us4/adapter.hpp"

#include "us4/backend.hpp"
#include "us4_test.hpp"

using namespace us4;

static void registry_routes_known_models() {
  AdapterRegistry reg;
  IUS4V6Adapter* qwen = reg.find_for("qwen-0.5b");
  US4_CHECK(qwen != nullptr);
  US4_CHECK(qwen->family() == ModelFamily::Qwen);
  US4_CHECK(qwen->supports("qwen-0.5b"));

  US4_CHECK(reg.find_for("llama-8b") != nullptr);
  US4_CHECK(reg.find_for("gemma-2b") != nullptr);
  US4_CHECK(reg.find_for("does-not-exist") == nullptr);
}

static void config_and_caps() {
  AdapterRegistry reg;
  IUS4V6Adapter* a = reg.find_for("qwen-0.5b");
  ModelConfig cfg = a->config_for("qwen-0.5b");
  US4_CHECK_EQ(cfg.hidden, 896u);
  US4_CHECK_EQ(cfg.layers, 24u);
  US4_CHECK(a->caps().gqa);
  US4_CHECK(!a->caps().moe);
}

static void plan_reflects_mode_and_backend() {
  AdapterRegistry reg;
  IUS4V6Adapter* a = reg.find_for("qwen-0.5b");
  ModelConfig cfg = a->config_for("qwen-0.5b");

  HardwareInfo hw;
  hw.platform = Platform::Linux;
  hw.arch = CpuArch::X86_64;
  hw.total_ram_bytes = 16ull * 1024 * 1024 * 1024;

  auto backend = BackendSelector::select(hw);
  ModeProfile nano = ModeSelector::profile(RuntimeMode::Nano);
  ExecutionPlan p = a->plan(cfg, hw, nano, *backend);
  US4_CHECK(p.mode == RuntimeMode::Nano);
  US4_CHECK_EQ(p.backend, BackendKind::GenericCpu);
  US4_CHECK_EQ(p.kv_window, 16384u);
  US4_CHECK(!p.speculative);
}

int main() {
  US4_RUN(registry_routes_known_models);
  US4_RUN(config_and_caps);
  US4_RUN(plan_reflects_mode_and_backend);
  US4_MAIN_END();
}
