# PQL-025 Validation Report

VALIDATION PASSED. Independent validator reviewed acceptance criteria and added
millisecond boundaries, intraday exclusions, direct-prefix equality and cost-basis
assertions. Independent C++ review found no material issues.

## Acceptance evidence
PASS: Starting cash, transactions and historical closes reconstruct owned historical
state and valuation. Synthetic buys, partial/full sells prove cash, quantity, fee-inclusive
average cost, cost basis, realized/unrealized P&L and marked totals.
PASS: Two identical runs produce exact equal complete snapshots and valuation fields.
Inputs remain unchanged; outputs survive input clearing. Valid future trades/prices do
not alter earlier results.
PASS: Deterministic inclusive cutoff, canonical equal-time order, strict full-ledger
validation and exact-session pricing. No clock, randomness or external API participates.

## Executed checks
MSVC developer environment: cmake --build build succeeded, C++20 /W4 /WX.
Validator rebuilt pql_historical_replay_tests after extending tests.
Focused executable: 9/9 historical replay tests passed.
ctest --test-dir build --output-on-failure: 17/17 suites passed.
git diff --check: passed.

Coverage also includes immediately-before/at/after millisecond cutoffs, intraday
same-day exclusion, missing/stale/future/duplicate marks, invalid future suffix,
wrong account, duplicate events, unsorted ledger, negative/zero seed, overflow,
liquidation without marks and equivalence with direct Portfolio::replay prefixes.

## Validation debt and scope
Sanitizers and cross-platform bitwise equality were not verified. PostgreSQL/provider
suites were not rerun: this ticket changes neither schema nor persistence/provider code.
Calendar-close validity, portfolio lifetime and corporate-action share basis remain
caller responsibilities. No scheduling, DB writes, strategy backtest or performance
claim was introduced. No unresolved CRITICAL/HIGH findings remain.
