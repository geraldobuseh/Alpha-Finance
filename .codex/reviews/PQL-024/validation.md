# PQL-024 Validation Report

VALIDATION PASSED. Independent validator authored eight pure tests and reviewed
implementation and integration coverage; implementer added the ninth prefix regression
and executed final builds. No unresolved CRITICAL/HIGH findings remain.

## Acceptance evidence
PASS cash, position value, total value, daily return and cumulative return: synthetic
unit fixtures verify fee-aware cash, multiple holdings, first observation, positive/
negative/zero returns, compounded returns, total loss and undefined denominators.
PASS persistence after calculation: PostgreSQL tests verify committed reconnect,
audited marks, historical cutoff, idempotence/conflicts, rollback, late-event rejection,
expected predecessor, exact component roundtrip and concurrent serialized writers.

## Executed commands and results
- MSVC developer environment: cmake --build build; ctest --test-dir build --output-on-failure.
  Final result: 15/15 suites passed (0.59s), C++20 /W4 /WX.
- build/test/pql_daily_valuation_tests.exe --gtest_brief=1: all nine tests passed.
- docker compose -f compose.yaml -f compose.test.yaml run --rm --no-deps --pull never persistence_tests.
  Final result: GCC14.2 C++20 build succeeded; 18/18 suites passed (2.55s), including
  PostgreSQL integration and existing provider/ingestion tests. Six new database tests.
- Migration003 applied successfully; schema tests cover required daily provenance,
  return bounds/NaN, source, date/cutoff, predecessor and foreign keys, unique session,
  positive closing marks, and existing legacy rows. Fixtures rolled back.
- clang-format applied to changed C++; git diff --check passed.

## Defects resolved
The new marks table initially lacked a fixture required by the generic schema-test
loop; added a fixture and schema suite passed. Independent C++ review found a pure
prior-ledger mismatch case; prefix replay/checks and regression resolve it.

## Validation debt and limits
Sanitizers were disabled in both builds. No production database migration or scheduler
was deployed. Exchange-close timing, consecutive sessions and corporate-action share
basis remain explicit caller contracts. No deposits/withdrawals or performance claims.
The disposable integration database was created/dropped by the existing guarded runner.

## Adjacent finding
The current feature/portfolio-snapshots branch starts at the Momentum commit and does
not contain the PQL-023 files created in the earlier conversation turn. This ticket
does not recreate unrelated strategy work.
