#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "adapter_internal.hpp"
#include "us4/adapter.hpp"

namespace us4::detail {

namespace {

// Dense decoder-only adapter (Qwen / Llama / Gemma share this shape). MoE, MLA
// and ternary families get their own adapters in later sprints.
class DenseAdapter final : public IUS4V6Adapter {
 public:
  DenseAdapter(ModelFamily family, std::string family_name,
               std::vector<ModelConfig> models)
      : family_(family),
        family_name_(std::move(family_name)),
        models_(std::move(models)) {}

  ModelFamily family() const override { return family_; }
  std::string family_name() const override { return family_name_; }

  AdapterCaps caps() const override {
    AdapterCaps c;
    c.gqa = true;  // all three families use grouped-query attention
    return c;
  }

  bool supports(const std::string& model_id) const override {
    return find(model_id) != nullptr;
  }

  std::vector<std::string> model_ids() const override {
    std::vector<std::string> ids;
    ids.reserve(models_.size());
    for (const auto& m : models_) ids.push_back(m.id);
    return ids;
  }

  ModelConfig config_for(const std::string& model_id) const override {
    if (const ModelConfig* m = find(model_id)) return *m;
    return {};
  }

  ExecutionPlan plan(const ModelConfig& cfg, const HardwareInfo&,
                     const ModeProfile& mode,
                     const IBackend& backend) const override {
    ExecutionPlan p;
    p.mode = mode.mode;
    p.backend = backend.kind();
    // KV window scales with the memory tier; clamped to a skeleton ceiling.
    static const std::uint32_t kWindowByTier[] = {262144, 131072, 65536, 49152,
                                                   32768,  24576,  16384};
    const int idx = static_cast<int>(mode.mode);
    p.kv_window = kWindowByTier[std::clamp(idx, 0, 6)];
    p.speculative = false;  // enabled in a later sprint, behind a flag
    p.notes = "dense GQA path; hidden=" + std::to_string(cfg.hidden) +
              " layers=" + std::to_string(cfg.layers);
    return p;
  }

 private:
  const ModelConfig* find(const std::string& id) const {
    for (const auto& m : models_) {
      if (m.id == id) return &m;
    }
    return nullptr;
  }

  ModelFamily family_;
  std::string family_name_;
  std::vector<ModelConfig> models_;
};

}  // namespace

std::vector<std::unique_ptr<IUS4V6Adapter>> make_builtin_adapters() {
  std::vector<std::unique_ptr<IUS4V6Adapter>> out;

  out.push_back(std::make_unique<DenseAdapter>(
      ModelFamily::Qwen, "Qwen",
      std::vector<ModelConfig>{
          {"qwen-0.5b", ModelFamily::Qwen, 896, 24, 14, 151936, 500000000ull},
          {"qwen-1.8b", ModelFamily::Qwen, 2048, 24, 16, 151936,
           1800000000ull},
          {"qwen-7b", ModelFamily::Qwen, 3584, 28, 28, 151936, 7000000000ull},
      }));

  out.push_back(std::make_unique<DenseAdapter>(
      ModelFamily::Llama, "Llama",
      std::vector<ModelConfig>{
          {"llama-1b", ModelFamily::Llama, 2048, 16, 32, 128256,
           1000000000ull},
          {"llama-8b", ModelFamily::Llama, 4096, 32, 32, 128256,
           8000000000ull},
      }));

  out.push_back(std::make_unique<DenseAdapter>(
      ModelFamily::Gemma, "Gemma",
      std::vector<ModelConfig>{
          {"gemma-2b", ModelFamily::Gemma, 2304, 26, 8, 256000, 2000000000ull},
      }));

  return out;
}

}  // namespace us4::detail
