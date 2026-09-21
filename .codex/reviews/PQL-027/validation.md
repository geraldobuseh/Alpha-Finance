# PQL-027 Validation Report

VALIDATION PASSED. Independent validator ran pql_drawdown_tests.exe:9/9 passed, and
reviewed the financial explanation. No unresolved blocking findings.

Acceptance: calculate and show peak, later trough and signed maximum decline. Synthetic
1200 ->900 case yields -25% and keeps it after recovery. docs/maximum-drawdown.md derives
the formula and compares A/B conditionally using returns, drawdowns and recovery burden.

Original and completion-audit validation: MSVC C++20 build and all19 local CTest suites
passed. clang-format --dry-run --Werror and git diff --check passed. Tests cover empty,
allzero, no decline, leading zeros, tied peaks/troughs, invalid timestamps/negative values,
malformed suffix after total loss, tiny declines, extreme values and repeated equality.

Completion audit saved the four review records after an earlier approval-service usage
limit prevented writing them. Code and tests were already implemented and reviewed.

Validation debt: sanitizers, Linux build and remote CI were not run for this ticket.
No persistence changes; database/provider suites were not rerun. Sampling frequency,
calendar completeness and comparable funding conventions remain caller responsibilities.
Changes are uncommitted; Gerald retains merge/governance authority.
