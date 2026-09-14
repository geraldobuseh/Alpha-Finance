# PQL-026 Validation Report

VALIDATION PASSED. All acceptance criteria satisfied; independent C++ and quant
reviews approved the final implementation with no unresolved blocking findings.

## Acceptance evidence
Daily and cumulative returns reuse (current-base)/base and preserve persisted binary
operation order. Annualized returns implement the derived geometric ACT/365 Fixed
equivalent with positive explicit elapsed calendar days. notes/008 derives gain/base,
wealth multipliers and annualization, and explains 100 -> 110 -> 99 is a 1% loss.

## Executed validation
MSVC developer environment: cmake --build build succeeded, then
ctest --test-dir build --output-on-failure: 18/18 suites passed (0.91s).
Independent validator ran final pql_returns_tests.exe: 10/10 tests passed.
Final clean docker compose -f compose.yaml -f compose.test.yaml run --rm --no-deps
--pull never persistence_tests: GCC14.2 C++20 build succeeded; 21/21 suites passed
(2.80s), including PostgreSQL snapshot and provider/ingestion regression suites.
Migration/schema fixture checks passed in the disposable test database.
clang-format --dry-run --Werror on all changed C++ files passed; git diff --check passed.

## Numerical findings and regression evidence
Review caught severe-loss cancellation and positive-subnormal gross-ratio rounding.
Both were fixed and independently re-reviewed. Ten tests cover known gains/losses,
compounding, exact old-operation compatibility, 365/366/730-day timing, zero bases,
invalid inputs/durations, total loss, tiny changes, overflow, severe losses and subnormal
ratios. The earlier Linux build compiled the pre-fix source before the new regression
arrived and failed that regression; the final clean build above passed all tests.

## Remaining validation debt
Sanitizers and cross-platform bit-for-bit annualized results are unverified. No schema
change, production deployment or automatic inference of portfolio inception is made.
Callers supply elapsed duration and funding assumptions; no external cash-flow model
or performance claim is introduced. Work remains uncommitted for human governance.
