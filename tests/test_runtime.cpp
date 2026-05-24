#include "us4/runtime.hpp"

#include <algorithm>

#include "us4_test.hpp"

using namespace us4;

static void run_produces_deterministic_output() {
  Runtime rt;
  RunRequest req;
  req.model_id = "qwen-0.5b";
  req.prompt = "hi there";
  req.max_tokens = 8;
  req.seed = 42;

  RunResult a = rt.run(req);
  US4_CHECK(a.ok);
  US4_CHECK_EQ(a.stats.prompt_tokens, 2u);
  US4_CHECK_EQ(a.stats.generated_tokens, 8u);
  US4_CHECK(!a.text.empty());
  US4_CHECK(a.tokens.size() == 8);

  RunResult b = rt.run(req);
  US4_CHECK(b.ok);
  US4_CHECK(a.tokens == b.tokens);  // determinism
  US4_CHECK(a.text == b.text);
}

static void different_seed_changes_output() {
  Runtime rt;
  RunRequest req;
  req.model_id = "qwen-0.5b";
  req.prompt = "hello";
  req.max_tokens = 8;

  req.seed = 1;
  RunResult a = rt.run(req);
  req.seed = 2;
  RunResult b = rt.run(req);
  US4_CHECK(a.ok && b.ok);
  US4_CHECK(a.tokens != b.tokens);
}

static void unknown_model_errors_cleanly() {
  Runtime rt;
  RunRequest req;
  req.model_id = "not-a-model";
  req.prompt = "x";
  RunResult r = rt.run(req);
  US4_CHECK(!r.ok);
  US4_CHECK(!r.error.empty());
}

static void probe_lists_models_and_backend() {
  Runtime rt;
  ProbeReport r = rt.probe();
  US4_CHECK(!r.backend_name.empty());
  US4_CHECK(!r.backend_candidates.empty());
  bool has_qwen = std::find(r.known_models.begin(), r.known_models.end(),
                            "qwen-0.5b") != r.known_models.end();
  US4_CHECK(has_qwen);
}

int main() {
  US4_RUN(run_produces_deterministic_output);
  US4_RUN(different_seed_changes_output);
  US4_RUN(unknown_model_errors_cleanly);
  US4_RUN(probe_lists_models_and_backend);
  US4_MAIN_END();
}
