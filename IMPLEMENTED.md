# Implementation Log & Installed Skills

> **Convention (project policy):** keep this file current. Every modification to
> the repo updates the relevant section below (installed skills and/or the change
> log). This is the single source of truth for "what has been built and which
> upstream skills are integrated."

## Installed skills / integrated upstream projects

| Skill / project | Upstream | What we implemented here | Location | Status |
| --- | --- | --- | --- | --- |
| US4 V6 — Universal State Runtime | [`us4-v6-simplicio-apple`](https://github.com/wesleysimplicio/us4-v6-simplicio-apple) | C++20 Sprint-01 skeleton: hardware probe, RAM-tiered mode selector, multi-backend selection contract, dense adapter (Qwen/Llama/Gemma), `us4-cli` (`--probe`, `run`) | `runtime/`, `apps/` | Skeleton (no real weights) |
| LLM Project Mapper | [`llm-project-mapper`](https://github.com/wesleysimplicio/llm-project-mapper) | Zero-dependency **Rust** rewrite of the mapping engine: stack detection (12 ecosystems), parallel line counting, `.llm-project-mapper.json` (schema `llm-project-mapper/v1`) | `tools/llm-project-mapper/` | Working, ~15x faster than Node ref |
| simplicio-prompt orchestration kernel | [`simplicio-prompt`](https://github.com/wesleysimplicio/simplicio-prompt) | Native C++: YOOL/Tuple/HAMT, Linda tuple-space, `batch_spawn` lazy fan-out (1M+ virtual agents), receipt cache, circuit breaker, backoff, lane pool; `llm.generate` yool routes to the runtime | `runtime/src/agents.cpp`, `us4-cli agents` | Working |
| X virality skill | [`x-virality-skills`](https://github.com/wesleysimplicio/x-virality-skills) | Native C++: source-grounded For You ranking (22 weighted signals, video gating, OON + author-diversity decay, hard filters) + text heuristic with actionable checklist | `runtime/src/virality.cpp`, `us4-cli virality analyze` | Working |

## Components in this repo

- **`runtime/`** — US4 C++ library: `hardware`, `mode`, `backend*`, `adapter*`,
  `tokenizer`, `runtime`, `hash` (SHA-256), `agents` (orchestration kernel),
  `virality` (X For You scoring).
- **`apps/us4-cli`** — CLI: `--probe`, `run`, `agents`, `virality analyze`.
- **`tools/llm-project-mapper/`** — Rust project mapper (binary + library).
- **Tests** — C++ CTest (`tests/`, 8 suites) and Rust `cargo test`
  (`tools/llm-project-mapper/`, 19 incl. golden regression).
- **`CLAUDE.md`** — guidance + the standing policy to keep this file updated.

## How to build & test

```bash
# US4 runtime (C++)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build

# Project mapper (Rust)
cd tools/llm-project-mapper && cargo test
```

## Change log (newest first)

- **Add `CLAUDE.md`** with the standing policy (update this log on every change)
  for cross-session durability.
- **Add x-virality-skills + this notes file.** Native `us4::virality` module
  (For You ranking + checklist), `us4-cli virality analyze`, `x.virality.analyze`
  yool registered in the agents kernel, `test_virality` (8/8 CTest total).
  Started this implementation log per project policy.
- **Add native simplicio-prompt orchestration kernel** (`ac46115`). HAMT, tuples,
  tuple-space, `batch_spawn`, resilience; `us4-cli agents`; SHA-256;
  `test_hash` + `test_agents`.
- **Rewrite README** (`8a222b7`) comparing upstream latest vs. our implementations.
- **Add golden-snapshot regression test** (`7ee8807`) for the mapper.
- **Rewrite llm-project-mapper in Rust** (`c3570ce`) — zero-dep, parallel, ~15x faster.
- **Add llm-project-mapper** (`8165eaf`) — initial Node.js implementation (superseded).
- **Implement US4 V6 runtime skeleton** (`aa4a6d1`) — Sprint 01 foundations.
