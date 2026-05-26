---
name: simplicio-cli
description: MANDATORY before any coding in this repo - route every feature, bugfix, refactor, or source edit through the simplicio-cli task-to-code pipeline (diff + test + verify loop). Not optional.
---

# simplicio-cli (MANDATORY coding pipeline)

<EXTREMELY-IMPORTANT>
In THIS repository you MUST run every coding task through `simplicio-cli`
before writing or editing source code. This is not optional and applies to
new features, bug fixes, refactors, and any edit under `runtime/`, `apps/`,
`tests/`, or build files (`CMakeLists.txt`, etc.).

If `simplicio-cli` is not installed or no provider key is configured, STOP
and tell the user. Do NOT silently hand-write the change without it.
</EXTREMELY-IMPORTANT>

## Why

`simplicio-cli` wraps each objective in a 6-layer contract and runs a
diff + test + verify loop (auto-fix/retry), which materially improves the
correctness of generated changes over ad-hoc editing.

## Setup (once)

```bash
pip install -r requirements-dev.txt   # or: pip install simplicio-cli==0.2.3
simplicio smoke                        # one proof call: connect + generate
```

Provider config is via env vars (provider-agnostic — no model is hard-coded):

- `SIMPLICIO_MODEL` (**required**) — model id exactly as the provider expects,
  e.g. `anthropic/claude-opus-4`, `openai/gpt-4.1`, `deepseek/deepseek-chat`.
- `SIMPLICIO_API_KEY` — the key (falls back to `OPENROUTER_API_KEY`, then `ANTHROPIC_API_KEY`).
- `SIMPLICIO_BASE_URL` — OpenAI-compatible endpoint. Leave empty + use
  `ANTHROPIC_API_KEY` for native Anthropic. Examples:
  OpenRouter `https://openrouter.ai/api/v1`, local Ollama `http://localhost:11434/v1`.
- `SIMPLICIO_TEST_CMD` — command the verify loop runs to check the change
  (e.g. the project's test/build command).

`simplicio smoke` prints `model=… base=… key=set|MISSING` so you can confirm config.

## Required workflow for every coding task

1. **Index** (once per stack/session) — cache embeddings of the codebase:
   ```bash
   simplicio index --stack <stack>
   ```
2. **Task** — turn the objective into a verified change (diff + tests + verify):
   ```bash
   simplicio task "<objective>" \
     --stack <stack> \
     --target <file-or-dir> \
     --criteria "<acceptance criteria>" \
     --constraints "<limits / what not to change>"
   ```
3. **Review & apply** — read the produced diff and tests, apply them, then run
   the project's own build/tests to confirm.
4. **Benchmark (optional)** — `simplicio bench` to score against the real suite.

`<stack>` describes the codebase context (this project is a C++/CMake LLM
runtime under `runtime/`, `apps/`, `tests/`).

## Checklist (create a todo per item)

- [ ] simplicio-cli installed (`simplicio --help` works) and `simplicio smoke` passes
- [ ] `simplicio index` run for the current stack
- [ ] Change generated via `simplicio task ...` (not hand-written)
- [ ] Generated diff + tests reviewed and applied
- [ ] Project build/tests pass
