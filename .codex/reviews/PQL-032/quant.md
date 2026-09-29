# PQL-032 Quantitative Correctness Review

Decision: QUANT REVIEW PASSED

Assumptions: idealized same-close decision/fill, raw-price USD equity valuations,
long-only market orders, configurable commission/adverse slippage, explicit zero
cost default. Same-close simulated fills do not prove executable real performance.
History exposed by MarketState strictly precedes the decision day.

Resolved HIGH findings: a quote of 100 and mark of 110 at the same timestamp
previously manufactured immediate gains; validation now rejects disagreement.
A future initial ledger previously reached strategy callbacks before downstream
validation; the engine now rejects it before any callback.

Synthetic cost oracle: two shares at a 100 quote, 1% adverse slippage and a 1 fee
cost 203; cash 797 plus marked holdings 200 gives equity 997. A later 120 close
gives equity 1037. Tests also check ledger reconstruction and caller preservation.

Existing DailyValuation cumulative return uses original starting cash, including
pre-run performance. First daily return is absent. Documentation makes this
baseline and mutable strategy repeatability assumptions explicit.

No unresolved HIGH/CRITICAL financial finding. SPY comparative orchestration,
corporate actions and realistic next-session execution are separate work. The
engine makes no superiority claim. Quant review was static; test execution is
recorded in validation.md.
