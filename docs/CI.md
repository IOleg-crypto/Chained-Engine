# CI/CD

Chained Decos uses **GitHub Actions** (`.github/workflows/`).

## Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| `ci.yml` | `push` to `main`/`develop`/`opengl`, `pull_request` to `main`/`opengl` | Orchestrator: format + Linux + Windows |
| `format.yml` | via `ci.yml` | `clang-format-18 --dry-run --Werror` on changed C++ files (skips `thirdparty/`) |
| `linux.yml` | via `ci.yml` | Ubuntu, clang+gcc, Debug+Release, `ctest` + managed tests |
| `windows.yml` | via `ci.yml` | Windows, clang/vs2026/gcc, Debug+Release, `ctest` + managed tests |
| `valgrind.yml` | nightly (`0 3 * * *` UTC) + manual | `memcheck` on unit tests only |
| `release.yml` | tags / manual | Release packaging |
| `deploy-sdk.yml` | tags / manual | SDK publishing |

## PR semantics: test the merge, not the branch

On `pull_request`, `actions/checkout@v4` (without an explicit `ref`) checks out the
**merge commit** (`refs/pull/N/merge`) — the result of merging the PR into its base
branch. Never set `ref: github.event.pull_request.head.sha` if you want PR status to
reflect the merged result.

Push CI runs only on `main`/`develop`/`opengl`, so feature branches are not built twice.
After merge, a fresh push run validates the real branch tip.

Required status checks (GitHub → Settings → Branches) must be bound to the **PR-run**
checks (`CI Passed` from `ci.yml`), not to push-run checks.

## Memory safety strategy

| Layer | When | Tool |
|---|---|---|
| Gate | every PR | **ASan + UBSan** (`ENABLE_SANITIZERS=ON`, Linux Debug jobs) |
| Leak gate | every PR (Linux) | **LSan** via ASan, suppressions in `.github/lsan.supp` |
| Deep memcheck | nightly | **Valgrind** on unit tests (`engine_tests_unit`, `engine_tests_editor`) |

Notes:

- Valgrind and ASan are mutually exclusive — the Valgrind job builds **Release** with `ENABLE_SANITIZERS=OFF`.
- Valgrind is intentionally **not** on every PR (10–50× slowdown + thirdparty/.NET noise).
- Valgrind job starts as `continue-on-error: true` (collect noise first, gate later).
- Suppressions: `.github/valgrind.supp` (memcheck), `.github/lsan.supp` (LSan).
- Integration tests (physics/GL/Coral) run under ASan/UBSan on PR, not under Valgrind.

## Local reproduction

```bash
# ASan+UBSan build (Linux)
cmake --preset linux-clang -B build/linux-clang -DENABLE_SANITIZERS=ON
cmake --build build/linux-clang --config Debug

# Valgrind on unit tests (Linux, Release, no sanitizers)
cmake --preset linux-clang -B build/linux-clang -DENABLE_SANITIZERS=OFF
cmake --build build/linux-clang --config Release --target engine_tests_unit engine_tests_editor
valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 \
  --suppressions=.github/valgrind.supp \
  build/linux-clang/bin/Release/engine_tests_unit
```
