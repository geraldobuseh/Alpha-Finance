# PQL-019: Quantitative Correctness Review

QUANT REVIEW PASSED for interface design. Recorded from quant reviewer; no runtime
tests were executed by that reviewer.

Const detached inputs preserve existing ownership. Proposals cannot authorize
spending; broker validation remains authoritative for costs, holdings, duplicates
and chronology, while the ledger records consequences. Explicit fixture IDs avoid
inventing a production allocation policy. Required tests separate proposals from
successful execution and cost-based rejection. Callers must supply market and
portfolio data aligned to the decision horizon; const does not prevent leakage.
No strategy-performance or benchmark claims apply to this interface ticket.
