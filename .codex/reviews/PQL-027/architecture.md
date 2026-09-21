# PQL-027 Architecture Review

ARCHITECTURE READY. Independent architecture review selected a pure maximumDrawdown
function over timestamped Money observations, returning signed fraction and owned
peak/trough points. Reuse checked cumulativeReturn arithmetic in pql_domain.

A chronological pass maintains a positive running peak. Strict comparisons preserve
earliest tied peaks and first equally deep troughs. Reject negative values and
non-increasing timestamps. Continue validation after total loss. Empty/all-zero curves
return absence; no decline returns zero with both endpoints at the first positive point.

No database or strategy behavior changes. Sampling completeness, comparable portfolio
values and no unadjusted external cash flows are caller contracts. Daily samples do
not expose intraday drawdown. Recovery duration and richer risk analytics are separate bets.

Required tests: 1200 -> 900, proportional versus dollar losses, chronology, recoveries,
ties, total loss, malformed suffixes, arithmetic extremes and deterministic ownership.
