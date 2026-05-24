#include "us4/backend.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "backend_internal.hpp"

namespace us4 {

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

// Accelerator that is part of the selection contract but not yet wired up.
// Reports unavailable; never selected; documents the roadmap honestly.
class PlannedBackend final : public IBackend {
 public:
  PlannedBackend(BackendKind kind, std::string reason)
      : kind_(kind), reason_(std::move(reason)) {}
  BackendKind kind() const override { return kind_; }
  std::string name() const override {
    return std::string(to_string(kind_)) + " (planned)";
  }
  bool available() const override { return false; }
  std::string unavailable_reason() const override { return reason_; }
  void matvec(const float*, const float*, float*, std::size_t,
              std::size_t) const override {
    // Unreachable: an unavailable backend is never selected for execution.
  }

 private:
  BackendKind kind_;
  std::string reason_;
};

std::unique_ptr<IBackend> make_kind(BackendKind kind, const HardwareInfo& hw) {
  switch (kind) {
    case BackendKind::GenericCpu:
      return detail::make_generic_cpu();
    case BackendKind::NeonAccelerate:
      return detail::make_neon_accelerate(hw.has_neon);
    case BackendKind::MLX:
      return detail::make_planned(BackendKind::MLX,
                                  "MLX integration lands in a later sprint");
    case BackendKind::Metal:
      return detail::make_planned(BackendKind::Metal,
                                  "Metal kernels land in a later sprint");
    case BackendKind::Ane:
      return detail::make_planned(
          BackendKind::Ane, "ANE offload requires M5+ and a later sprint");
  }
  return detail::make_generic_cpu();
}

// Preference order, best first. The selector skips unavailable entries.
std::vector<BackendKind> preference_order(const HardwareInfo& hw) {
  if (hw.platform == Platform::Apple) {
    return {BackendKind::MLX, BackendKind::Metal, BackendKind::Ane,
            BackendKind::NeonAccelerate, BackendKind::GenericCpu};
  }
  if (hw.arch == CpuArch::Arm64) {
    return {BackendKind::NeonAccelerate, BackendKind::GenericCpu};
  }
  return {BackendKind::GenericCpu};
}

}  // namespace

std::unique_ptr<IBackend> detail::make_planned(BackendKind kind,
                                               std::string reason) {
  return std::make_unique<PlannedBackend>(kind, std::move(reason));
}

std::vector<BackendCandidate> BackendSelector::candidates(
    const HardwareInfo& hw) {
  std::vector<BackendCandidate> out;
  for (BackendKind k : preference_order(hw)) {
    auto b = make_kind(k, hw);
    out.push_back({k, b->name(), b->available(), b->unavailable_reason()});
  }
  return out;
}

std::unique_ptr<IBackend> BackendSelector::select(const HardwareInfo& hw,
                                                  const std::string& prefer) {
  if (!prefer.empty()) {
    BackendKind want;
    if (parse_backend_kind(prefer, want)) {
      auto b = make_kind(want, hw);
      if (b->available()) return b;
      // Requested backend unavailable: fall through to automatic selection.
    }
  }
  for (BackendKind k : preference_order(hw)) {
    auto b = make_kind(k, hw);
    if (b->available()) return b;
  }
  return detail::make_generic_cpu();
}

const char* to_string(BackendKind k) {
  switch (k) {
    case BackendKind::MLX:
      return "mlx";
    case BackendKind::Metal:
      return "metal";
    case BackendKind::Ane:
      return "ane";
    case BackendKind::NeonAccelerate:
      return "neon-accelerate";
    case BackendKind::GenericCpu:
      return "generic-cpu";
  }
  return "generic-cpu";
}

bool parse_backend_kind(const std::string& s, BackendKind& out) {
  const std::string v = lower(s);
  if (v == "mlx") {
    out = BackendKind::MLX;
  } else if (v == "metal") {
    out = BackendKind::Metal;
  } else if (v == "ane") {
    out = BackendKind::Ane;
  } else if (v == "neon" || v == "neon-accelerate" || v == "accelerate") {
    out = BackendKind::NeonAccelerate;
  } else if (v == "cpu" || v == "generic-cpu" || v == "generic") {
    out = BackendKind::GenericCpu;
  } else {
    return false;
  }
  return true;
}

}  // namespace us4
