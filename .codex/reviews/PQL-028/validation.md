# PQL-028 Validation Report

VALIDATION PASSED for implementation. Independent validator ran pql_volatility_tests.exe:
8/8 passed and found no blocking code/test findings.

Acceptance: sample return standard deviation implemented with n-1. Optional annualization
is implemented now using an explicit sqrt(periods_per_year) factor. docs/volatility.md
explains derivation, interpretation, sampling conventions, missing observations and limits.

Executed MSVC C++20 build using vcvars64.bat, cmake --build build, then
ctest --test-dir build --output-on-failure: 20/20 suites passed (0.68 seconds).
clang-format --dry-run --Werror on changed C++ files and git diff --check passed.

Synthetic tests verify known .01 sample deviation, sample versus population distinction,
252/12/1 annualization factors, empty/singleton absence, constants, invalid singleton,
NaN/infinity/<-1 rejection, translation/sign invariance, adjacent huge values, tiny
values, explicit underflow/annual overflow and deterministic unchanged inputs.

Validation debt: sanitizers, Linux and remote CI were not run. Persistence/provider
suites were not rerun because no persistence or provider code changed. Calendar quality,
equal-period sampling and annualization-model validity remain caller responsibilities.
No unrelated changes, performance claims or deployment. Work remains uncommitted.
