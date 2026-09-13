# PQL-023 Architecture Review

Independent architecture review: ARCHITECTURE READY.

Design: single configured Symbol and TradingCosts; latest twenty completed session
closes exclude the current quote. Long-only all-cash entry while flat below MA*0.95;
full exit at or above MA. No pyramiding. Reuse Strategy, sizing and broker boundaries.

Invariants: immutable observations, owned proposals, deterministic ledger-derived
IDs with exhaustion guard, explicit missing/short-history errors and no future
portfolio observations. Normalize the mean to avoid summation overflow.

Required validation: exact thresholds, nonconstant average, latest-20 window,
missing history, costs, execution, replay, no accumulation and ID exhaustion.

Scope: no API/database changes. Calendar completeness, quote freshness and
corporate-action consistency remain caller duties. Universe allocation and
empirical profitability evaluation are separate bets.
