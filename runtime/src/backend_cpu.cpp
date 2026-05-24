#include <memory>
#include <string>

#include "backend_internal.hpp"
#include "us4/backend.hpp"

#if defined(US4_HAVE_ACCELERATE)
#include <Accelerate/Accelerate.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace us4::detail {

namespace {

void matvec_scalar(const float* w, const float* x, float* out,
                   std::size_t rows, std::size_t cols) {
  for (std::size_t i = 0; i < rows; ++i) {
    const float* row = w + i * cols;
    float acc = 0.0f;
    for (std::size_t j = 0; j < cols; ++j) acc += row[j] * x[j];
    out[i] = acc;
  }
}

// Portable scalar backend. Always available; the correctness reference path.
class GenericCpuBackend final : public IBackend {
 public:
  BackendKind kind() const override { return BackendKind::GenericCpu; }
  std::string name() const override { return "generic-cpu (scalar)"; }
  bool available() const override { return true; }
  void matvec(const float* w, const float* x, float* out, std::size_t rows,
              std::size_t cols) const override {
    matvec_scalar(w, x, out, rows, cols);
  }
};

// ARM NEON / Apple Accelerate backend. Available only when the host exposes
// NEON; otherwise the selector skips it. Numerically matches the scalar path.
class NeonAccelerateBackend final : public IBackend {
 public:
  explicit NeonAccelerateBackend(bool available) : available_(available) {}
  BackendKind kind() const override { return BackendKind::NeonAccelerate; }
  std::string name() const override {
#if defined(US4_HAVE_ACCELERATE)
    return "neon-accelerate (BLAS sgemv)";
#else
    return "neon-accelerate (NEON)";
#endif
  }
  bool available() const override { return available_; }
  std::string unavailable_reason() const override {
    return available_ ? "" : "host has no ARM NEON unit";
  }
  void matvec(const float* w, const float* x, float* out, std::size_t rows,
              std::size_t cols) const override {
#if defined(US4_HAVE_ACCELERATE)
    cblas_sgemv(CblasRowMajor, CblasNoTrans, static_cast<int>(rows),
                static_cast<int>(cols), 1.0f, w, static_cast<int>(cols), x, 1,
                0.0f, out, 1);
#elif defined(__ARM_NEON)
    for (std::size_t i = 0; i < rows; ++i) {
      const float* row = w + i * cols;
      float32x4_t acc = vdupq_n_f32(0.0f);
      std::size_t j = 0;
      for (; j + 4 <= cols; j += 4) {
        acc = vmlaq_f32(acc, vld1q_f32(row + j), vld1q_f32(x + j));
      }
      float sum = vaddvq_f32(acc);
      for (; j < cols; ++j) sum += row[j] * x[j];
      out[i] = sum;
    }
#else
    matvec_scalar(w, x, out, rows, cols);
#endif
  }

 private:
  bool available_;
};

}  // namespace

std::unique_ptr<IBackend> make_generic_cpu() {
  return std::make_unique<GenericCpuBackend>();
}

std::unique_ptr<IBackend> make_neon_accelerate(bool available) {
  return std::make_unique<NeonAccelerateBackend>(available);
}

}  // namespace us4::detail
