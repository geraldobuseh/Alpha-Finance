# PQL-032 Validation Report

Decision: VALIDATION PASSED

Acceptance criteria:
- PASS: required member and free run signatures compile.
- PASS: trading days processed in supplied order through strategy, broker,
  transaction-driven portfolio updates, and daily snapshots.
- PASS: deterministic repeated results with equivalent initial strategy state.

Executed by the implementer: MSVC vcvars64 environment initialization,
cmake --build build -j 4, focused CTest BacktestEngineTest, and full
ctest --test-dir build --output-on-failure. Initial 12 focused tests passed.

Independent Validator added a regression for a held asset missing its mark,
then executed MSVC initialization and cmake --build build, full CTest, and the
direct backtest test executable. Final results: 23/23 suites and 13/13 focused
tests passed. Changed test clang-format --dry-run --Werror passed.
Engine header/source formatting was checked by the implementer. git diff --check
passed, with only Windows CRLF conversion warnings.

Coverage: exact API shape, daily execution/valuation, weekend gap preservation,
no-order days, repeated results, malformed chronological inputs, quote/mark
agreement, duplicate/missing marks, future market history and initial ledger,
prior holdings/lifetime return baseline, commission/slippage, ledger replay,
input preservation, rejected orders/multi-leg batches, and valuation failure.
The synthetic cost oracle yields equity 997 then 1037 after a price rise.

No blocking findings. Existing domain/execution/valuation suites remain green.

Remaining validation debt: Linux/GCC, sanitizers, remote CI, optional PostgreSQL
and real-provider suites were not run. No database/schema/API changes were needed.
Governance: implementation is ready for human review; no merge/deployment performed.
