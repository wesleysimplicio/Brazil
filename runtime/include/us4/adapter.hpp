#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "us4/backend.hpp"
#include "us4/hardware.hpp"
#include "us4/mode.hpp"

namespace us4 {

enum class ModelFamily {
  Qwen,
  Llama,
  Gemma,
  DeepSeek,
  Kimi,
  MiniMax,
  GLM,
  BitNet,
  Ternary,
  Unknown,
};

// Architecture capabilities a family exposes. Drives KV layout, routing and
// memory planning decisions.
struct AdapterCaps {
  bool moe = false;         // mixture-of-experts
  bool mla = false;         // multi-head latent attention
  bool gqa = false;         // grouped-query attention
  bool multimodal = false;  // vision/audio inputs
  bool ternary = false;     // ternary/low-bit weights
};

struct ModelConfig {
  std::string id;
  ModelFamily family = ModelFamily::Unknown;
  std::uint32_t hidden = 0;
  std::uint32_t layers = 0;
  std::uint32_t heads = 0;
  std::uint32_t vocab = 0;
  std::uint64_t params = 0;
};

// Concrete plan the runtime executes for one (model, machine, mode) triple.
struct ExecutionPlan {
  RuntimeMode mode = RuntimeMode::Nano;
  BackendKind backend = BackendKind::GenericCpu;
  std::uint32_t kv_window = 0;
  bool speculative = false;
  std::string notes;
};

// Universal adapter contract. One implementation per model family.
class IUS4V6Adapter {
 public:
  virtual ~IUS4V6Adapter() = default;
  virtual ModelFamily family() const = 0;
  virtual std::string family_name() const = 0;
  virtual AdapterCaps caps() const = 0;
  virtual bool supports(const std::string& model_id) const = 0;
  virtual std::vector<std::string> model_ids() const = 0;
  virtual ModelConfig config_for(const std::string& model_id) const = 0;
  virtual ExecutionPlan plan(const ModelConfig& cfg, const HardwareInfo& hw,
                             const ModeProfile& mode,
                             const IBackend& backend) const = 0;
};

class AdapterRegistry {
 public:
  AdapterRegistry();
  IUS4V6Adapter* find_for(const std::string& model_id) const;
  std::vector<IUS4V6Adapter*> all() const;

 private:
  std::vector<std::unique_ptr<IUS4V6Adapter>> adapters_;
};

const char* to_string(ModelFamily);

}  // namespace us4
