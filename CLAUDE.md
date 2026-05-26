# Project guidance for Claude Code

## MANDATORY: use simplicio-cli for ALL coding tasks

Before writing or modifying ANY code in this repository, you MUST run the
task through the **simplicio-cli** task-to-code pipeline. This is not
optional. "Coding" includes new features, bug fixes, refactors, and any
edit to source files under `runtime/`, `apps/`, `tests/`, or build files.

simplicio-cli wraps each objective in a 6-layer contract (diff + test +
verify loop) that materially improves correctness. The detailed workflow
and exact command syntax live in the skill at
`.claude/skills/simplicio-cli/SKILL.md` — invoke/read it before coding.

Minimum loop for any coding task:

1. `simplicio index --stack <stack>` — cache embeddings for this repo (once per stack/session).
2. `simplicio task "<objective>" --stack <stack> --target <file> --criteria "<acceptance>" --constraints "<limits>"` — generate the verified change (diff + test + verify).
3. Review the produced diff/tests, apply, and run the project's own tests.

Provider configuration (env vars, provider-agnostic):
`SIMPLICIO_MODEL` (required, e.g. `anthropic/claude-opus-4`) plus a key —
`SIMPLICIO_API_KEY` (or `OPENROUTER_API_KEY` / `ANTHROPIC_API_KEY`). For
OpenAI-compatible/local providers set `SIMPLICIO_BASE_URL`. Optionally set
`SIMPLICIO_TEST_CMD` to the project's test command for the verify loop.
Run `simplicio smoke` to verify provider setup.

Install (if missing): `pip install -r requirements-dev.txt`
(or `pip install simplicio-cli==0.2.3`).

If simplicio-cli cannot run (e.g. no provider key configured), STOP and tell
the user rather than silently coding without it.
