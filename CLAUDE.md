# CLAUDE.md

Guidance for Claude Code working in this repository.

## Standing policy (always follow)

- **Keep `IMPLEMENTED.md` up to date on every modification.** It is the single
  source of truth for installed skills and everything implemented. On any change
  (new skill ported, feature added, refactor, fix), update the relevant table
  and add a newest-first entry to its change log **in the same commit**.
- When integrating an upstream "skill"/project, record it in the
  `IMPLEMENTED.md` skills table (upstream link, what we built, location, status).

## What this repo is

Native implementations of several `wesleysimplicio` projects:

- **US4 V6 runtime** (C++20) in `runtime/` + `apps/` — local LLM inference
  runtime skeleton, plus a native `agents` orchestration kernel (simplicio-prompt)
  and a `virality` For You scorer (x-virality-skills).
- **llm-project-mapper** (Rust, zero-dependency) in `tools/llm-project-mapper/`.

See `IMPLEMENTED.md` for the full skill inventory and `README.md` for the
upstream-vs-ours comparison.

## Build & test

```bash
# US4 runtime (C++)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
ctest --test-dir build

# Project mapper (Rust)
cd tools/llm-project-mapper && cargo test && cargo clippy --all-targets
```

CLI: `us4-cli --probe | run | agents | virality analyze`.

## Conventions

- C++ builds clean under `-Wall -Wextra -Wpedantic`; Rust under `cargo clippy`.
- No hardcoded benchmark numbers — report measured wall-clock only.
- Mapper output is guarded by a golden snapshot test; regenerate intentional
  changes with `UPDATE_GOLDEN=1 cargo test --test regression`.
- Regenerate the repo map after structural changes:
  `./tools/llm-project-mapper/target/release/llm-project-mapper .`
