# Native ports: US4 V6 runtime + LLM Project Mapper

This repository contains native, from-scratch implementations of two projects by
[@wesleysimplicio](https://github.com/wesleysimplicio), built and tested here:

| Project | Upstream | Ours (this repo) | Where |
| --- | --- | --- | --- |
| US4 V6 — Universal State Runtime | [`us4-v6-simplicio-apple`](https://github.com/wesleysimplicio/us4-v6-simplicio-apple) | C++20 Sprint-01 skeleton | `runtime/`, `apps/` |
| LLM Project Mapper | [`llm-project-mapper`](https://github.com/wesleysimplicio/llm-project-mapper) | Rust rewrite of the mapping engine | `tools/llm-project-mapper/` |

This README compares the **latest upstream version** of each project with the
version implemented here. Scope is stated honestly: these are focused,
fully-building-and-tested slices, not feature-complete reimplementations.

---

## US4 V6 — Universal State Runtime

A local LLM inference runtime targeting Apple Silicon. Our port implements
Sprint 01 ("Foundations and Skeleton"): the runtime skeleton, CLI contract,
hardware probe, mode selector, and backend-selection contract.

### Latest upstream vs. ours

| Aspect | Upstream (latest) | This repo (ours) |
| --- | --- | --- |
| Language | C++ 78.5% + TS/JS/Shell tooling | C++20 |
| Build system | CMake + Ninja | CMake + Ninja |
| Scope | Full runtime; 12-sprint roadmap toward v1.0 | Sprint 01 skeleton (foundations) |
| Backends | MLX, Metal, NEON/Accelerate, ANE (M5+) | Selection contract for all four; **NEON/Accelerate + scalar CPU implemented**, MLX/Metal/ANE declared-but-unavailable with safe fallback |
| Model families | Dense (Qwen/Llama/Gemma), MoE (DeepSeek/Kimi/MiniMax/GLM), ternary (BitNet/PT-BitNet) | Dense adapter (Qwen/Llama/Gemma) |
| Inference | Real on-device inference with model weights | Deterministic **stub** over a real backend `matvec` (no weights yet); timings are measured wall-clock, never hardcoded |
| Mode selection | RAM-tiered runtime modes | 7 RAM tiers (128 GB Full → 16 GB Nano) |
| CLI | `us4-cli --probe` / `run` | Same contract: `--probe`, `run --model … --prompt … --max-tokens …` |
| Tests | GoogleTest + Playwright E2E | 5 CTest suites |
| Target platform | Apple Silicon (M1–M5) | Portable: builds and tests on Linux x86; Apple paths behind `__APPLE__`/availability guards |
| Releases | No formal release; scaffold complete | `0.1.0` (skeleton) |

### Orchestration kernel (simplicio-prompt, native)

The runtime also ports the [`simplicio-prompt`](https://github.com/wesleysimplicio/simplicio-prompt)
agent-orchestration logic natively in C++ (`runtime/src/agents.cpp`): YOOL
atomic capabilities, content-addressed Tuples, a HAMT registry (32-way, 6
levels), a Linda tuple-space, and `batch_spawn(depth, branching, threshold)`
for **lazy** hierarchical fan-out — representing 1M+ virtual agents without
materializing them. It includes a receipt LRU+TTL cache, a provider circuit
breaker, jittered backoff, and bounded lane concurrency. A local `llm.generate`
yool routes work straight into the US4 inference runtime.

```bash
./build/apps/us4-cli agents --depth 6 --branching 32 --tasks 2
# -> virtual_agents=1073741824 while only 3 agents are active
```

### Not yet ported

Real model loading and MLX/Metal/ANE kernels, MoE and ternary adapters,
KV-cache memory tiering, speculative decoding, and continuous batching — these
are later sprints in the upstream roadmap.

---

## LLM Project Mapper

Scans a codebase and emits a structured `.llm-project-mapper.json` so AI agents
understand a project before they program it. The upstream is a full
**scaffolding starter** (it also generates `.specs/`, `.agents/`, `.skills/`,
agent instruction files, CI, and E2E tests). Our version reimplements the
**mapping engine** — the part that "maps the projects it will program" — in
Rust, focused on speed and correctness.

### Latest upstream vs. ours

| Aspect | Upstream v0.3.2 (2026-05-18) | This repo (ours) |
| --- | --- | --- |
| Language | TypeScript 42.6% / JavaScript 36.7% (+ Shell/PowerShell/Python) | **Rust**, zero runtime dependencies |
| Install / run | `npx @wesleysimplicio/llm-project-mapper` | `cargo build --release` → `./llm-project-mapper` |
| Scope | Full scaffolder: auto-map **plus** generate `.specs/.agents/.skills/.claude`, instruction files, CI, E2E | Focused mapping engine (produces `.llm-project-mapper.json`) |
| Stack detection | Node, Python, Go, .NET, Rust, Java, … | 12 ecosystems (Node, Python, Go, Rust, PHP, CMake, .NET, JVM, Ruby, Dart, Elixir, Swift) **with dependency parsing** |
| Output schema | Scaffolding + map | `.llm-project-mapper.json` (`schema: llm-project-mapper/v1`) |
| Performance | Node.js (interpreter startup per run) | Compiled binary, **parallel line counting** (`std::thread::scope`); ~**15× faster** on this repo (62 ms → 4 ms) |
| Dependencies | npm ecosystem | none (Rust `std` only) |
| Tests | Playwright smoke tests | 19 tests incl. a **golden-snapshot regression** test |

### Why Rust here, but C++ for the runtime?

The mapper is I/O- and CPU-bound work (walk, read, count) that was interpreted —
a clear win for a compiled, parallel language. The US4 runtime was kept in C++
on purpose: it is already a top-tier performance language and the upstream spec
depends on MLX/Metal/Accelerate (C/C++/Obj-C) interop, so a Go port would be
slower and a Rust port would be a lateral move that loses those bindings.

---

## Build & test

### US4 V6 runtime (C++)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build                # 5/5

./build/apps/us4-cli --probe
./build/apps/us4-cli run --model qwen-0.5b --prompt "hi" --max-tokens 8
./build/apps/us4-cli agents --depth 4 --branching 32   # orchestration kernel
```

### LLM Project Mapper (Rust)

```bash
cd tools/llm-project-mapper
cargo build --release
cargo test                            # 19/19 (incl. golden regression)

./target/release/llm-project-mapper /path/to/project --summary
```

To accept an intentional change to the mapper output, regenerate the golden
snapshot: `UPDATE_GOLDEN=1 cargo test --test regression`.

---

## Licensing

The repository `LICENSE` is Apache-2.0. The `llm-project-mapper` crate declares
MIT, matching its upstream. All upstream references above belong to their
respective authors.
