#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "us4/runtime.hpp"
#include "us4/version.hpp"

namespace {

void print_usage() {
  std::cout
      << "us4-cli - " << us4::kEdition << " v" << us4::kVersionString << "\n\n"
      << "Universal State Runtime for local LLM inference (Apple edition).\n\n"
      << "Usage:\n"
      << "  us4-cli --probe [--backend <kind>]\n"
      << "  us4-cli run --model <id> --prompt <text> [options]\n"
      << "  us4-cli --version | --help\n\n"
      << "run options:\n"
      << "  --model <id>        model id, e.g. qwen-0.5b (see --probe)\n"
      << "  --prompt <text>     input prompt\n"
      << "  --max-tokens <n>    tokens to generate (default 16)\n"
      << "  --backend <kind>    force backend: mlx|metal|ane|neon|cpu\n"
      << "  --seed <n>          deterministic seed (default 0)\n\n"
      << "Note: this is the Sprint-01 skeleton. Generation uses a deterministic\n"
      << "stub compute path (no model weights yet); timings are real wall-clock\n"
      << "of that path, never hardcoded benchmark claims.\n";
}

std::string next_arg(int argc, char** argv, int& i, const std::string& flag) {
  if (i + 1 >= argc) {
    std::cerr << "error: missing value for " << flag << "\n";
    std::exit(2);
  }
  return argv[++i];
}

int cmd_probe(const std::string& backend_pref) {
  us4::Runtime rt;
  us4::ProbeReport r = rt.probe(backend_pref);

  std::cout << "== US4 V6 probe ==\n";
  std::cout << "platform     : " << r.hw.platform_name << " ("
            << us4::to_string(r.hw.arch) << ")\n";
  if (!r.hw.cpu_brand.empty())
    std::cout << "cpu          : " << r.hw.cpu_brand << "\n";
  std::cout << "cores        : " << r.hw.logical_cores << "\n";
  std::cout << std::fixed << std::setprecision(1);
  std::cout << "ram          : " << r.hw.total_ram_gib() << " GiB\n";
  std::cout << "simd         : "
            << (r.hw.has_neon ? "NEON " : "") << (r.hw.has_avx ? "AVX2 " : "")
            << (!r.hw.has_neon && !r.hw.has_avx ? "scalar" : "") << "\n";
  std::cout << "apple silicon: " << (r.hw.apple_silicon ? "yes" : "no") << "\n";
  std::cout << "mode         : " << r.mode.name << " (>=" << r.mode.min_ram_gib
            << " GiB) -> " << r.mode.target_models << "\n";
  std::cout << "backend      : " << r.backend_name << "\n";

  std::cout << "candidates   :\n";
  for (const auto& c : r.backend_candidates) {
    std::cout << "  - " << c.name << " : "
              << (c.available ? "available" : "unavailable");
    if (!c.available && !c.reason.empty()) std::cout << " (" << c.reason << ")";
    std::cout << "\n";
  }

  std::cout << "models       :";
  for (const auto& m : r.known_models) std::cout << " " << m;
  std::cout << "\n";
  return 0;
}

int cmd_run(int argc, char** argv, int start) {
  us4::RunRequest req;
  bool have_model = false;
  for (int i = start; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--model") {
      req.model_id = next_arg(argc, argv, i, a);
      have_model = true;
    } else if (a == "--prompt") {
      req.prompt = next_arg(argc, argv, i, a);
    } else if (a == "--max-tokens") {
      req.max_tokens = static_cast<std::uint32_t>(
          std::strtoul(next_arg(argc, argv, i, a).c_str(), nullptr, 10));
    } else if (a == "--backend") {
      req.backend_pref = next_arg(argc, argv, i, a);
    } else if (a == "--seed") {
      req.seed = static_cast<std::uint64_t>(
          std::strtoull(next_arg(argc, argv, i, a).c_str(), nullptr, 10));
    } else {
      std::cerr << "error: unknown run option: " << a << "\n";
      return 2;
    }
  }
  if (!have_model) {
    std::cerr << "error: run requires --model <id>\n";
    return 2;
  }
  if (req.max_tokens == 0) req.max_tokens = 16;

  us4::Runtime rt;
  us4::RunResult res = rt.run(req);
  if (!res.ok) {
    std::cerr << "error: " << res.error << "\n";
    return 1;
  }

  std::cout << "model   : " << res.config.id << " ("
            << us4::to_string(res.config.family) << ", "
            << res.config.params / 1000000ull << "M params)\n";
  std::cout << "backend : " << us4::to_string(res.plan.backend) << "\n";
  std::cout << "mode    : " << us4::to_string(res.plan.mode)
            << "  kv_window=" << res.plan.kv_window << "\n";
  std::cout << "prompt  : " << req.prompt << "\n";
  std::cout << "output  : " << res.text << "\n";
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "stats   : prompt=" << res.stats.prompt_tokens
            << " gen=" << res.stats.generated_tokens
            << " prefill=" << res.stats.prefill_ms << "ms"
            << " decode=" << res.stats.decode_ms << "ms"
            << " tok/s=" << res.stats.tokens_per_sec << " (stub compute)\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }

  std::string first = argv[1];
  if (first == "--help" || first == "-h") {
    print_usage();
    return 0;
  }
  if (first == "--version" || first == "-v") {
    std::cout << us4::kVersionString << "\n";
    return 0;
  }
  if (first == "--probe") {
    std::string backend_pref;
    for (int i = 2; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--backend" && i + 1 < argc) {
        backend_pref = argv[++i];
      } else {
        std::cerr << "error: unknown probe option: " << a << "\n";
        return 2;
      }
    }
    return cmd_probe(backend_pref);
  }
  if (first == "run") {
    return cmd_run(argc, argv, 2);
  }

  std::cerr << "error: unknown command: " << first << "\n\n";
  print_usage();
  return 2;
}
