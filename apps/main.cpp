#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "us4/agents.hpp"
#include "us4/runtime.hpp"
#include "us4/skills.hpp"
#include "us4/version.hpp"
#include "us4/virality.hpp"

namespace {

void print_usage() {
  std::cout
      << "us4-cli - " << us4::kEdition << " v" << us4::kVersionString << "\n\n"
      << "Universal State Runtime for local LLM inference (Apple edition).\n\n"
      << "Usage:\n"
      << "  us4-cli --probe [--backend <kind>]\n"
      << "  us4-cli run --model <id> --prompt <text> [options]\n"
      << "  us4-cli agents [--depth <n>] [--branching <n>] [options]\n"
      << "  us4-cli virality analyze --text <post>\n"
      << "  us4-cli skills list | show <name> | lint-commit --message <m>\n"
      << "  us4-cli --version | --help\n\n"
      << "run options:\n"
      << "  --model <id>        model id, e.g. qwen-0.5b (see --probe)\n"
      << "  --prompt <text>     input prompt\n"
      << "  --max-tokens <n>    tokens to generate (default 16)\n"
      << "  --backend <kind>    force backend: mlx|metal|ane|neon|cpu\n"
      << "  --seed <n>          deterministic seed (default 0)\n\n"
      << "agents options (simplicio-prompt orchestration kernel):\n"
      << "  --depth <n>         virtual tree depth (default 4)\n"
      << "  --branching <n>     children per node (default 32)\n"
      << "  --threshold <n>     compress active agents above this count\n"
      << "  --tasks <n>         real worker tuples on the infer lane (default 4)\n"
      << "  --model <id>        model for the llm.generate yool (default qwen-0.5b)\n"
      << "  --prompt <text>     prompt for the workers (default \"hello\")\n"
      << "  --max-tokens <n>    tokens per worker (default 6)\n\n"
      << "virality (x-virality-skills, source-grounded For You ranking):\n"
      << "  analyze --text <post>   score a post + actionable checklist\n\n"
      << "skills (native skills subsystem over .claude/skills/):\n"
      << "  list [--dir <path>]            list + register skills as HAMT yools\n"
      << "  show <name> [--dir <path>]     print a skill's metadata\n"
      << "  lint-commit --message <msg>    native conventional-commits validator\n\n"
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

int cmd_agents(int argc, char** argv, int start) {
  int depth = 4;
  int branching = 32;
  int threshold = -1;  // -1 => policy default
  int tasks = 4;
  std::string model = "qwen-0.5b";
  std::string prompt = "hello";
  std::uint32_t max_tokens = 6;

  for (int i = start; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--depth") {
      depth = std::atoi(next_arg(argc, argv, i, a).c_str());
    } else if (a == "--branching") {
      branching = std::atoi(next_arg(argc, argv, i, a).c_str());
    } else if (a == "--threshold") {
      threshold = std::atoi(next_arg(argc, argv, i, a).c_str());
    } else if (a == "--tasks") {
      tasks = std::atoi(next_arg(argc, argv, i, a).c_str());
    } else if (a == "--model") {
      model = next_arg(argc, argv, i, a);
    } else if (a == "--prompt") {
      prompt = next_arg(argc, argv, i, a);
    } else if (a == "--max-tokens") {
      max_tokens = static_cast<std::uint32_t>(
          std::strtoul(next_arg(argc, argv, i, a).c_str(), nullptr, 10));
    } else {
      std::cerr << "error: unknown agents option: " << a << "\n";
      return 2;
    }
  }
  if (depth < 1 || branching < 1) {
    std::cerr << "error: --depth and --branching must be >= 1\n";
    return 2;
  }
  if (max_tokens == 0) max_tokens = 6;

  using namespace us4::agents;
  auto [space, root] = build_default_space();

  // Wire the orchestration kernel to our LLM: a local yool that runs the US4
  // inference runtime. Tuples carrying `llm.generate` route here, no API call.
  us4::Runtime rt;
  space->register_local_yool(
      "llm.generate", [&rt, max_tokens](Tuple& t) -> std::string {
        us4::RunRequest req;
        req.model_id = t.data.count("model") ? t.data["model"] : "qwen-0.5b";
        req.prompt = t.data.count("prompt") ? t.data["prompt"] : "";
        req.max_tokens = max_tokens;
        us4::RunResult res = rt.run(req);
        return res.ok ? res.text : ("error: " + res.error);
      });

  // Our LLM also "knows" the X virality skill as an addressable capability.
  space->register_local_yool(
      "x.virality.analyze", [](Tuple& t) -> std::string {
        auto a = us4::virality::analyze_post(t.data.count("text") ? t.data["text"]
                                                                  : "");
        return std::to_string(a.score.final);
      });

  // Lazy hierarchical fan-out: represent branching**depth virtual agents
  // without materializing them.
  std::optional<int> thr =
      threshold < 0 ? std::nullopt : std::optional<int>(threshold);
  BatchSpawnReceipt br =
      space->batch_spawn(*root, "agent.dev.python", depth, branching, thr);

  // Materialize a handful of real worker tuples on the "infer" lane.
  for (int i = 0; i < tasks; ++i) {
    space->spawn_agent(*root, "llm.generate",
                       {{"lane", "infer"},
                        {"model", model},
                        {"prompt", prompt + " #" + std::to_string(i)}});
  }

  // Drain the lane across bounded workers (llm.generate routes to the runtime).
  LaneWorkerPool pool(*space);
  auto results = pool.run_lane(
      "infer", [](Tuple&) { return std::string("<no-executor>"); });

  SpaceSnapshot snap = space->snapshot();
  auto cap = space->lookup_yool("llm.generate");

  std::cout << "== US4 agents (simplicio-prompt kernel) ==\n";
  std::cout << "batch_spawn  : depth=" << br.depth << " branching=" << br.branching
            << " -> virtual_agents=" << br.virtual_agents << "\n";
  std::cout << "             : root_agent=" << br.root_agent_id
            << " receipt=" << br.receipt_id
            << " threshold=" << br.compression_threshold << "\n";
  std::cout << "registry     : llm.generate -> HAMT addr "
            << (cap ? std::to_string(*cap) : std::string("<unregistered>"))
            << "\n";
  std::cout << "infer lane   : " << results.size() << " worker(s) executed\n";
  for (std::size_t i = 0; i < results.size(); ++i) {
    std::cout << "  [" << i << "] " << results[i] << "\n";
  }
  std::cout << "snapshot     : active=" << snap.active_agents
            << " compressed=" << snap.compressed_agents
            << " virtual=" << snap.virtual_agents
            << " total=" << snap.total_agents << "\n";
  std::cout << "             : tuples=" << snap.tuples
            << " cache_entries=" << snap.cache_entries
            << " lanes=" << snap.lanes.size() << "\n";
  return 0;
}

int cmd_virality(int argc, char** argv, int start) {
  if (start >= argc) {
    std::cerr << "error: virality requires a subcommand: analyze --text <post>\n";
    return 2;
  }
  std::string sub = argv[start];
  if (sub != "analyze") {
    std::cerr << "error: unknown virality subcommand: " << sub << "\n";
    return 2;
  }

  std::string text;
  bool have_text = false;
  for (int i = start + 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--text") {
      text = next_arg(argc, argv, i, a);
      have_text = true;
    } else {
      std::cerr << "error: unknown analyze option: " << a << "\n";
      return 2;
    }
  }
  if (!have_text) {
    std::cerr << "error: analyze requires --text <post>\n";
    return 2;
  }

  us4::virality::Analysis r = us4::virality::analyze_post(text);

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "== X virality analysis (For You ranking) ==\n";
  if (r.score.removed) {
    std::cout << "REMOVED from feed: " << r.score.removal_reason
              << " (hard-limit filter)\n";
    return 0;
  }
  std::cout << "score        : final=" << r.score.final
            << " (combined=" << r.score.combined
            << ", diversity x" << r.score.diversity_multiplier
            << ", oon x" << r.score.oon_multiplier << ")\n";
  std::cout << "top signals  :\n";
  for (std::size_t i = 0; i < r.score.contributions.size() && i < 5; ++i) {
    const auto& c = r.score.contributions[i];
    std::cout << "  " << c.name << " = " << c.value << " (p=" << c.prob
              << " x w=" << c.weight << ")\n";
  }
  std::cout << "checklist    :\n";
  for (const auto& item : r.checklist) {
    std::cout << "  [" << (item.pass ? "x" : " ") << "] " << item.id;
    if (!item.pass) std::cout << " - " << item.note;
    std::cout << "\n";
  }
  if (!r.tips.empty()) {
    std::cout << "tips         :\n";
    for (const auto& t : r.tips) std::cout << "  - " << t << "\n";
  }
  return 0;
}

int cmd_skills(int argc, char** argv, int start) {
  if (start >= argc) {
    std::cerr << "error: skills requires a subcommand: list | show | lint-commit\n";
    return 2;
  }
  std::string sub = argv[start];
  std::string dir = ".claude/skills";
  std::string name;
  std::string message;
  bool have_message = false;

  for (int i = start + 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--dir") {
      dir = next_arg(argc, argv, i, a);
    } else if (a == "--message") {
      message = next_arg(argc, argv, i, a);
      have_message = true;
    } else if (a[0] != '-' && name.empty()) {
      name = a;
    } else {
      std::cerr << "error: unknown skills option: " << a << "\n";
      return 2;
    }
  }

  if (sub == "lint-commit") {
    if (!have_message) {
      std::cerr << "error: lint-commit requires --message <msg>\n";
      return 2;
    }
    us4::skills::CommitLint r = us4::skills::lint_conventional_commit(message);
    std::cout << "== conventional-commits ==\n";
    std::cout << "valid    : " << (r.valid ? "yes" : "no") << "\n";
    std::cout << "type     : " << r.type << "\n";
    if (!r.scope.empty()) std::cout << "scope    : " << r.scope << "\n";
    std::cout << "breaking : " << (r.breaking ? "yes" : "no") << "\n";
    std::cout << "subject  : " << r.subject << "\n";
    for (const auto& e : r.errors) std::cout << "  - error: " << e << "\n";
    return r.valid ? 0 : 1;
  }

  us4::skills::SkillRegistry reg;
  std::size_t loaded = reg.load_dir(dir);

  if (sub == "show") {
    if (name.empty()) {
      std::cerr << "error: show requires a skill name\n";
      return 2;
    }
    const us4::skills::SkillDef* s = reg.find(name);
    if (!s) {
      std::cerr << "error: skill not found: " << name << " (in " << dir << ")\n";
      return 1;
    }
    std::cout << "name        : " << s->name << "\n";
    std::cout << "status      : " << (s->status.empty() ? "(default)" : s->status)
              << "\n";
    if (!s->source.empty()) std::cout << "source      : " << s->source << "\n";
    std::cout << "description : " << s->description << "\n";
    return 0;
  }

  if (sub == "list") {
    if (loaded == 0) {
      std::cerr << "no skills found under " << dir << "\n";
      return 1;
    }
    // Make every skill addressable natively: register as a yool in the kernel.
    us4::agents::TupleSpace space;
    for (const auto& s : reg.all()) {
      const std::string desc = s.description;
      space.register_local_yool("skill." + s.name,
                                [desc](us4::agents::Tuple&) { return desc; });
    }
    std::cout << "== skills (" << loaded << " loaded from " << dir << ") ==\n";
    for (const auto& s : reg.all()) {
      auto addr = space.lookup_yool("skill." + s.name);
      std::cout << "  " << (s.always_on ? "*" : " ") << " " << s.name
                << "  [yool addr " << (addr ? std::to_string(*addr) : "?")
                << "]\n";
      if (!s.description.empty())
        std::cout << "      " << s.description << "\n";
    }
    std::cout << "(* = always-on; all " << loaded
              << " skills registered as HAMT-addressable yools)\n";
    return 0;
  }

  std::cerr << "error: unknown skills subcommand: " << sub << "\n";
  return 2;
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

  if (first == "agents") {
    return cmd_agents(argc, argv, 2);
  }
  if (first == "virality") {
    return cmd_virality(argc, argv, 2);
  }
  if (first == "skills") {
    return cmd_skills(argc, argv, 2);
  }

  std::cerr << "error: unknown command: " << first << "\n\n";
  print_usage();
  return 2;
}
