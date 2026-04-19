# Spec: Databricks-Style Key Execution (Issue #10)

**Status:** Ready for implementation  
**Target release:** 1.2.0  
**Labels:** enhancement

---

## Complexity Assessment

### Scope — 4/5
Four subsystems are touched: the core public API (`splinter.h`/`splinter.c`) gains `splinter_run()`; the CLI command layer gains a new `run` command; a new shared language-provider module (`splinter_cli_languages.c`) decouples the Lua/WASM embedding from individual command files; and CMake gets a new `--enable-executable-keys` / `HAVE_EXEC_KEYS` feature gate. The signal arena is read but not structurally modified.

### Uncertainty — 2/5
The feature is well-scoped by the owner's follow-up comments. The API shape (`splinter_run()`), CLI syntax (`run <key> [with_options=...] [options=...]`), CMake flag name, and runtime scope (Lua + WASM only, no external exec) are all specified. The main open questions are around return-value semantics and interpreter option surface, both flagged below.

### Risk — 3/5
`splinter_run()` must not disturb `splinter_get()` hot paths. The CMake gate prevents untrusted exposure by default, which contains the security blast radius. The biggest risk is lifetime management of interpreter state when the CLI runs inside a long-lived process. Note: Valgrind-clean applies only to Splinter's own code; third-party interpreter internals (Lua, WasmEdge) are out of scope for leak checking — we cannot control them, only code defensively around them.

### Effort — M (3–5 dev-days)
New core function, new CLI command, new shared language module, CMake wiring, and tests across Lua and WASM paths. No daemon or IPC changes. Existing Lua and WASM CLI commands provide working embedding code to refactor from.

---

## Problem Statement

Splinter stores arbitrary binary payloads but provides no way to execute them in-place. Users who embed Lua scripts or WASM modules as values must extract the bytes externally, invoke an interpreter themselves, and pipe results back — defeating the "shared memory bus" model. A key-prefixed execution convention (`%lua:`, `%wasm:`) would let the CLI treat those values as first-class runnable artifacts, enabling SIMD workloads, data normalization pipelines, and future inference hooks without any change to the passive storage core.

---

## Proposed Solution

Introduce a `splinter_run()` API function and a `run` CLI command, both gated behind a compile-time `HAVE_EXEC_KEYS` flag (`--enable-executable-keys` in `./configure`). When invoked, `splinter_run()` reads the value at the given key, detects its runtime prefix, and dispatches to the appropriate embedded interpreter (Lua or WasmEdge). Stdout/stderr from the runtime pass through directly to the caller's console. A new shared module (`splinter_cli_languages.c`) consolidates the Lua and WASM embedding logic currently scattered across individual command files, making both the `run` command and the existing `lua`/`wasm` commands link against a single provider.

---

## Functional Requirements

1. Keys prefixed with `%lua:` or `%wasm:` are recognized as executable by `splinter_run()`.
2. `splinter_run()` is a new public API function declared in `splinter.h`; it does not alter the behavior of `splinter_get()`.
3. `splinter_run()` returns a failure code if the target key lacks a recognized executable prefix.
4. The CLI exposes a `run <key>` command that invokes `splinter_run()` and streams runtime stdout/stderr to the console.
5. `run` accepts `with_options=<opts>` to pass flags to the underlying interpreter (initial supported value: `test=true`).
6. `run` accepts `options=<opts>` to control the run command itself (initial supported value: `--unstable`).
7. The entire feature is compiled only when `HAVE_EXEC_KEYS` is defined; without it, `splinter_run()` is not present and the `run` command is absent.
8. `./configure --enable-executable-keys` sets `HAVE_EXEC_KEYS` in `config.h`.
9. Lua and WASM embedding logic is refactored into `splinter_cli_languages.c` / `splinter_cli_languages.h`; the existing `lua` and `wasm` CLI commands link against this module.
10. Splinter's own code must be Valgrind-clean (no leaks or errors in the wrapper, dispatch, and language-module boundary code). Leaks originating inside third-party interpreter internals (Lua, WasmEdge) are explicitly out of scope — suppress them with a project `.valgrindrc` if needed.
11. A prominent warning is emitted (compile-time or startup) when `HAVE_EXEC_KEYS` is enabled, advising against running untrusted code.

---

## Out of Scope

- External process execution (`%python3:`, `%m4:`, etc.) — no process lifecycle management in this iteration.
- `%sql` or LLM-backed transformation keys — depends on the inference shard (not yet merged).
- Return-value capture from the interpreter into the store — stdout/stderr passthrough only; no automatic key-write-back from script output.
- Remote API or Claude Code pipe integration — future work.
- Windows/macOS portability — Linux-only per project constraints.

---

## Open Questions

1. **Interpreter option surface for `with_options`:** Is `test=true` (Lua/WASM sandbox/validation mode) the only value needed at launch, or should arbitrary interpreter flags pass through? The latter simplifies the CLI but widens the attack surface.
2. **Key prefix delimiter:** Is `%lua:` the canonical form (colon as separator), or should `%lua/` or `%lua ` be considered? Tokenizer behavior with colons in key names needs verification.
3. **`splinter_run()` signature:** Should it accept a callback/buffer for captured output, or always write directly to stdout/stderr? A buffer-based approach would allow programmatic callers to capture results, but adds complexity.
4. **Interpreter lifetime:** Should WasmEdge/Lua state be initialized once per CLI session (static) or per `run` invocation (fresh)? Per-invocation is safer for isolation; per-session is faster for repeated runs.
5. **CMake flag discoverability:** Should `./configure --help` list `--enable-executable-keys` alongside other feature flags, and should the default-off state be noted in `splinterrc_example`?

---

## Acceptance Criteria

- [ ] `./configure` without `--enable-executable-keys` produces a build with no `splinter_run` symbol and no `run` CLI command.
- [ ] `./configure --enable-executable-keys` produces a build where `run <key>` executes a `%lua:`-prefixed value via the embedded Lua interpreter and prints output to stdout.
- [ ] `run <key>` with a `%wasm:`-prefixed key executes via WasmEdge and prints output.
- [ ] `run <key>` on a key without a recognized prefix returns a non-zero exit code and a descriptive error message.
- [ ] `splinter_run()` does not alter the behavior of `splinter_get()` on any key.
- [ ] `make tests` passes with zero Valgrind errors in Splinter's own code when `HAVE_EXEC_KEYS` is enabled; interpreter-internal leaks are suppressed via `.valgrindrc`.
- [ ] Existing `lua` and `wasm` CLI commands continue to pass their regression tests after the refactor into `splinter_cli_languages.c`.
- [ ] A compile-time or runtime warning is visible when `HAVE_EXEC_KEYS` is active.

---

## Implementation Plan

### Task 1 — CMake / configure wiring
**Entry:** `configure` script and `CMakeLists.txt` as they exist today.  
**Work:** Add `--enable-executable-keys` option to `configure`; pass `-DHAVE_EXEC_KEYS=1` through to CMake; add `HAVE_EXEC_KEYS` to `config.h.in`; gate the new source files on this definition in `CMakeLists.txt`.  
**Exit:** `./configure --enable-executable-keys && make` succeeds; `./configure && make` succeeds and symbol `splinter_run` is absent from the resulting library.  
**Blockers:** None.

### Task 2 — `splinter_cli_languages.c` refactor
**Entry:** Existing Lua embedding in `splinter_cli_cmd_lua.c` and WASM embedding in `splinter_cli_cmd_wasm.c`.  
**Work:** Extract the interpreter init/teardown and execution logic into `splinter_cli_languages.h` / `splinter_cli_languages.c`. Expose `splinter_lang_run_lua(const char *src, size_t len, const char *interp_opts)` and `splinter_lang_run_wasm(const char *src, size_t len, const char *interp_opts)`. Update `lua` and `wasm` command files to call through this module.  
**Exit:** All existing `splinterctl_tests.sh` lua/wasm cases still pass; Valgrind-clean.  
**Blockers:** None; can be done before or in parallel with Task 3.

### Task 3 — `splinter_run()` core API
**Entry:** `splinter.h` / `splinter.c`.  
**Work:** Declare and implement `splinter_run(splinter_t *s, const char *key, const char *interp_opts, const char *run_opts)` under `#ifdef HAVE_EXEC_KEYS`. Function reads the slot, checks for `%lua:` or `%wasm:` prefix, strips the prefix, dispatches to `splinter_cli_languages.c`. Return codes: `SPLINTER_OK`, `SPLINTER_ERR_NOT_EXECUTABLE`, `SPLINTER_ERR_RUNTIME`.  
**Exit:** Unit test exercises both prefix paths and the no-prefix rejection path; Valgrind-clean.  
**Blockers:** Task 2 (language module must exist to link against).

### Task 4 — `run` CLI command
**Entry:** CLI command framework (`splinter_cli_main.c`, `splinter_cli.h`).  
**Work:** Add `splinter_cli_cmd_run.c`. Parse `run <key> [with_options=...] [options=...]`. Call `splinter_run()`. Handle and print error codes. Register command in the CLI dispatch table under `HAVE_EXEC_KEYS` guard.  
**Exit:** `run <key>` in the REPL and one-shot mode exercises a stored Lua script and prints its output; `run` on a plain key prints an error; `splinterctl_tests.sh` gains at least two test cases.  
**Blockers:** Task 3.

### Task 5 — Tests and Valgrind pass
**Entry:** Tasks 1–4 complete.  
**Work:** Add ctest entries for the executable-keys build variant. Run `valgrind --leak-check=full` against both Lua and WASM run paths. Fix any leaks in Splinter's own wrapper/dispatch code; suppress interpreter-internal leaks in `.valgrindrc`.  
**Exit:** `make tests` green, zero Valgrind errors, `splinterctl_tests.sh` passes.  
**Blockers:** Tasks 1–4.
