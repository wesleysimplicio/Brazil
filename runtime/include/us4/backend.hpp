#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "us4/hardware.hpp"

namespace us4 {

enum class BackendKind {
  MLX,             // Apple MLX (primary tensor path; integrated in later sprint)
  Metal,           // hand-written Metal kernels (later sprint)
  Ane,             // Apple Neural Engine, M5+ (later sprint)
  NeonAccelerate,  // ARM NEON + Accelerate/BLAS
  GenericCpu,      // portable scalar fallback (always available)
};

// Compute interface exercised by the runtime. The skeleton needs a single
// real primitive; richer ops arrive with the MLX/Metal backends.
class IBackend {
 public:
  virtual ~IBackend() = default;
  virtual BackendKind kind() const = 0;
  virtual std::string name() const = 0;
  virtual bool available() const = 0;
  // Reason a backend is unavailable (e.g. "not integrated in skeleton").
  virtual std::string unavailable_reason() const { return {}; }

  // Row-major matrix-vector product: out[i] = sum_j w[i*cols + j] * x[j].
  virtual void matvec(const float* w, const float* x, float* out,
                      std::size_t rows, std::size_t cols) const = 0;
};

struct BackendCandidate {
  BackendKind kind;
  std::string name;
  bool available;
  std::string reason;  // when unavailable
};

class BackendSelector {
 public:
  // Ordered preference list (best first) describing what the platform exposes.
  static std::vector<BackendCandidate> candidates(const HardwareInfo& hw);

  // Best available backend, or one matching `prefer` (case-insensitive kind
  // name) when supplied. Falls back to GenericCpu, which is always available.
  static std::unique_ptr<IBackend> select(const HardwareInfo& hw,
                                          const std::string& prefer = "");
};

const char* to_string(BackendKind);
bool parse_backend_kind(const std::string& s, BackendKind& out);

}  // namespace us4
