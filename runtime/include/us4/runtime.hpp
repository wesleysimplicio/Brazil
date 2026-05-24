#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "us4/adapter.hpp"
#include "us4/backend.hpp"
#include "us4/hardware.hpp"
#include "us4/mode.hpp"

namespace us4 {

struct RunRequest {
  std::string model_id;
  std::string prompt;
  std::uint32_t max_tokens = 16;
  std::string backend_pref;  // optional override
  std::uint64_t seed = 0;
};

struct RunStats {
  double prefill_ms = 0.0;
  double decode_ms = 0.0;
  double tokens_per_sec = 0.0;  // measured wall-clock of the stub compute
  std::size_t prompt_tokens = 0;
  std::size_t generated_tokens = 0;
};

struct RunResult {
  bool ok = false;
  std::string error;
  std::string text;
  std::vector<std::uint32_t> tokens;
  ModelConfig config;
  ExecutionPlan plan;
  RunStats stats;
};

struct ProbeReport {
  HardwareInfo hw;
  ModeProfile mode;
  BackendKind selected_backend = BackendKind::GenericCpu;
  std::string backend_name;
  std::vector<BackendCandidate> backend_candidates;
  std::vector<std::string> known_models;
};

// Top-level facade: probe -> mode -> backend -> adapter -> generate.
class Runtime {
 public:
  Runtime();

  ProbeReport probe(const std::string& backend_pref = "") const;
  RunResult run(const RunRequest& req) const;

 private:
  AdapterRegistry registry_;
};

}  // namespace us4
