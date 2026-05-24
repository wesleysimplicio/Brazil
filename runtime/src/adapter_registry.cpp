#include <string>
#include <vector>

#include "adapter_internal.hpp"
#include "us4/adapter.hpp"

namespace us4 {

AdapterRegistry::AdapterRegistry() : adapters_(detail::make_builtin_adapters()) {}

IUS4V6Adapter* AdapterRegistry::find_for(const std::string& model_id) const {
  for (const auto& a : adapters_) {
    if (a->supports(model_id)) return a.get();
  }
  return nullptr;
}

std::vector<IUS4V6Adapter*> AdapterRegistry::all() const {
  std::vector<IUS4V6Adapter*> out;
  out.reserve(adapters_.size());
  for (const auto& a : adapters_) out.push_back(a.get());
  return out;
}

const char* to_string(ModelFamily f) {
  switch (f) {
    case ModelFamily::Qwen:
      return "Qwen";
    case ModelFamily::Llama:
      return "Llama";
    case ModelFamily::Gemma:
      return "Gemma";
    case ModelFamily::DeepSeek:
      return "DeepSeek";
    case ModelFamily::Kimi:
      return "Kimi";
    case ModelFamily::MiniMax:
      return "MiniMax";
    case ModelFamily::GLM:
      return "GLM";
    case ModelFamily::BitNet:
      return "BitNet";
    case ModelFamily::Ternary:
      return "Ternary";
    case ModelFamily::Unknown:
      break;
  }
  return "Unknown";
}

}  // namespace us4
