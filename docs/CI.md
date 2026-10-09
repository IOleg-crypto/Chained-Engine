# CI/CD

Chained Decos uses **GitHub Actions** (`.github/workflows/`).

## Workflows

| Workflow | Trigger | Purpose |
| --- | --- | --- |
| `ci.yml` | `push` to `main`/`opengl`/`experimental-opengl`, `pull_request` | Fast orchestrator: Linux sanitizer + Windows Clang |
| `ci.yml` | nightly schedule + manual `full` | Full Linux/Windows matrix through the reusable workflows |
| `linux.yml` | via `ci.yml` | Reusable Ubuntu build, CTest, and optional managed tests |
| `windows.yml` | via `ci.yml` | Reusable Windows build and CTest with optional managed tests |
| `valgrind.yml` | nightly (`0 3 * * *` UTC) + manual | `memcheck` on unit tests only |
| `release.yml` | tags / manual | Release packaging |
| `deploy-sdk.yml` | tags / manual | SDK publishing |

## PR semantics: test the merge, not the branch

On `pull_request`, `actions/checkout@v4` (without an explicit `ref`) checks out the
**merge commit** (`refs/pull/N/merge`) — the result of merging the PR into its base
branch. Never set `ref: github.event.pull_request.head.sha` if you want PR status to
reflect the merged result.

Push CI runs on `main`, `opengl`, and `experimental-opengl`, using the same fast
matrix as PRs. Feature branches are not built twice, and a fresh push run validates
the real branch tip after merge.

The full compiler matrix is intentionally reserved for the nightly schedule or a manual
full run so ordinary changes do not wait for every compiler/configuration combination.

Required status checks (GitHub → Settings → Branches) must be bound to the **PR-run**
checks (`CI Passed` from `ci.yml`), not to push-run checks.

Managed tests run once through the Linux reusable workflow. Windows runs native CTest
only unless a caller explicitly enables its managed-test input.

## Memory safety strategy

| Layer | When | Tool |
| --- | --- | --- |
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
