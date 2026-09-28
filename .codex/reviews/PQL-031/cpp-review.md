# PQL-031 C++ Review

Status: passed

The implementation follows the existing C++20 style and project domain model:

- Public API uses `Money`, `PortfolioSnapshot`, `EquityPoint`, and existing
  analytics result types rather than primitive-only boundaries.
- The builder is deterministic and does not mutate portfolio or curve inputs.
- Helper functions keep validation, prefix replay, turnover, and return sampling
  local to the scorecard module.
- Arithmetic checks reject unrepresentable totals instead of silently repairing
  financial values.
- Exceptions from composed analytics are translated into `ScorecardError`.
- No global state, owning raw pointers, concurrency, or speculative abstraction
  was introduced.

The main accepted limitation is intentional: open-position market values are
trusted from the supplied equity curve until the valuation engine provides typed
daily valuations and marks.
