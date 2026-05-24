#include "us4/runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include "us4/tokenizer.hpp"

namespace us4 {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

std::uint32_t mix(std::uint32_t a, std::uint32_t b) {
  a *= 2654435761u;
  a ^= b * 40503u + 0x9e3779b9u;
  a ^= a >> 15;
  a *= 2246822519u;
  a ^= a >> 13;
  return a;
}

// Deterministic value in [-1, 1].
float unit(std::uint32_t h) {
  return (static_cast<float>(h % 20001u) - 10000.0f) / 10000.0f;
}

// Synthetic embedding for a token id into a hidden vector.
void embed(std::uint32_t token, float* x, std::size_t hidden) {
  for (std::size_t k = 0; k < hidden; ++k) {
    x[k] = unit(mix(token, static_cast<std::uint32_t>(k)));
  }
}

}  // namespace

Runtime::Runtime() = default;

ProbeReport Runtime::probe(const std::string& backend_pref) const {
  ProbeReport r;
  r.hw = HardwareProbe::detect();
  r.mode = ModeSelector::select(r.hw);
  r.backend_candidates = BackendSelector::candidates(r.hw);
  auto backend = BackendSelector::select(r.hw, backend_pref);
  r.selected_backend = backend->kind();
  r.backend_name = backend->name();
  for (IUS4V6Adapter* a : registry_.all()) {
    for (const auto& id : a->model_ids()) r.known_models.push_back(id);
  }
  return r;
}

RunResult Runtime::run(const RunRequest& req) const {
  RunResult res;

  IUS4V6Adapter* adapter = registry_.find_for(req.model_id);
  if (adapter == nullptr) {
    res.error = "unknown model id: '" + req.model_id +
                "' (use --probe to list known models)";
    return res;
  }

  const HardwareInfo hw = HardwareProbe::detect();
  const ModeProfile mode = ModeSelector::select(hw);
  auto backend = BackendSelector::select(hw, req.backend_pref);

  const ModelConfig cfg = adapter->config_for(req.model_id);
  res.config = cfg;
  res.plan = adapter->plan(cfg, hw, mode, *backend);

  // Skeleton compute dimensions. Real weights replace these in a later sprint.
  const std::size_t hidden = std::min<std::uint32_t>(cfg.hidden, 256u);
  const std::size_t vocab = std::min<std::uint32_t>(cfg.vocab, 512u);
  if (hidden == 0 || vocab == 0) {
    res.error = "invalid model config (zero hidden/vocab)";
    return res;
  }

  Tokenizer tok;

  // Prefill: tokenize the prompt.
  auto t_prefill = Clock::now();
  std::vector<std::uint32_t> prompt_tokens =
      tok.encode(req.prompt, static_cast<std::uint32_t>(vocab));
  res.stats.prefill_ms = ms_since(t_prefill);
  res.stats.prompt_tokens = prompt_tokens.size();

  // Deterministic projection matrix W[vocab x hidden], seeded by model + seed.
  std::uint32_t seed = mix(static_cast<std::uint32_t>(req.seed & 0xffffffffu),
                           static_cast<std::uint32_t>(prompt_tokens.size()));
  for (char c : req.model_id) seed = mix(seed, static_cast<unsigned char>(c));

  std::vector<float> w(vocab * hidden);
  for (std::size_t i = 0; i < w.size(); ++i) {
    w[i] = unit(mix(seed, static_cast<std::uint32_t>(i)));
  }

  std::vector<float> x(hidden);
  std::vector<float> logits(vocab);
  std::uint32_t last =
      prompt_tokens.empty() ? seed % vocab : prompt_tokens.back();
  embed(last, x.data(), hidden);

  // Decode: autoregressive greedy loop over the real backend matvec.
  std::vector<std::uint32_t> generated;
  generated.reserve(req.max_tokens);
  auto t_decode = Clock::now();
  for (std::uint32_t step = 0; step < req.max_tokens; ++step) {
    backend->matvec(w.data(), x.data(), logits.data(), vocab, hidden);
    std::uint32_t best = 0;
    float best_val = logits[0];
    for (std::size_t i = 1; i < vocab; ++i) {
      if (logits[i] > best_val) {
        best_val = logits[i];
        best = static_cast<std::uint32_t>(i);
      }
    }
    generated.push_back(best);
    embed(best, x.data(), hidden);
  }
  res.stats.decode_ms = ms_since(t_decode);
  res.stats.generated_tokens = generated.size();
  if (res.stats.decode_ms > 0.0) {
    res.stats.tokens_per_sec =
        static_cast<double>(generated.size()) / (res.stats.decode_ms / 1000.0);
  }

  res.tokens = generated;
  res.text = tok.decode(generated);
  res.ok = true;
  return res;
}

}  // namespace us4
