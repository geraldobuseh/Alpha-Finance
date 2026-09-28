# PQL-031 Validation Review

Status: passed

Validation performed:

- MSVC Debug build via CMake: passed.
- Focused CTest filter `StrategyScorecardTest`: passed.
- Full CTest suite: 22/22 suites passed.
- Full `src` and `test` clang-format dry-run with `--Werror`: passed.
- `git diff --check`: passed, with only CRLF conversion warnings.

Regression coverage includes:

- Full numeric scorecard oracle.
- Cash/no-trade singleton scorecard with unavailable risk metrics.
- Positive excess return during negative absolute return using a real open
  position ledger.
- Misaligned, empty, unordered, zero-start, and invalid-frequency windows.
- Transactions outside the scorecard window.
- Undefined internal returns after zero equity.
- Cash-only equity that contradicts replayed cash.
- Open-position equity below replayed cash.
- Negative SPY observations.
- Determinism and input immutability.

Remaining validation debt: Linux/GCC, sanitizers, and remote CI were not rerun in
this final pass.
