# PQL-024 C++20 Review

C++ REVIEW PASSED after one HIGH issue was corrected and re-reviewed independently.

## Finding and resolution
Initial pure calculation checked prior portfolio ID, seed, date and sequence length
but did not prove that the preceding valuation described the current ledger prefix.
The fix replays through the prior cutoff and checks sequence, cash and marked value.
Regression covers late historical events and a divergent prefix with equal event count.
The reviewer confirmed the fix and found no remaining material issues.

## Ownership and arithmetic
Returned values own their data. Borrowed prior pointers/iterators remain within stable
input lifetimes. Errors do not mutate portfolios. Return subtraction before division
preserves small changes and rejects nonfinite/unrepresentable results. Existing money
valuation checks reject overflow and wholly lost components. Optional returns represent
legitimate missing baselines instead of fabricated zeros.

## Persistence
Portfolio locks serialize writers. Historical prefix replay excludes future fills.
Snapshot/mark inserts share a rollback-on-error unit; retries compare source and marks.
Reads recompute the chain and verify amounts, returns and predecessor metadata. Exact
SQL generated totals and recomputed binary C++ totals have an explicit precision policy.

This is source-review evidence; executed build/test evidence is in validation.md.
