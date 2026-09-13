# PQL-024 Quantitative Correctness Review

QUANT REVIEW PASSED. Independent financial design review and final source/test
inspection found no unresolved financial correctness issue.

Cash includes executed trades and fees. Position value is sum(quantity * raw close).
Total is cash plus positions; realized P&L and fees must not be added/subtracted a
second time. Daily return = (total - preceding total) / preceding total. Cumulative
return = (total - starting cash) / starting cash, retaining first-day fees.

First daily return is absent. Zero denominators are absent; positive capital becoming
zero has a valid -1 return. Fractions are not percent points. These formulas require
no external deposits/withdrawals, consistent with the current trade-only ledger.

Historical prices cannot mark future holdings. Prior snapshots must match the same
portfolio, seed and ledger prefix. Complete sessions and correct closing instants
remain caller responsibilities; a date label alone cannot verify an exchange calendar.
Raw prices must match the ledger share basis. No dividend/corporate-action model is
introduced. Compare SPY on consistent sessions/capital/costs; excess return is not alpha.

Synthetic examples: 799 cash plus 200 holdings after a one-dollar buy fee gives999,
not1000; 1000 -> 1100 -> 990 yields daily +10%, -10%, cumulative -1%. Tests also cover
zero capital, total loss, missing/wrong-session prices and numerical extremes.
