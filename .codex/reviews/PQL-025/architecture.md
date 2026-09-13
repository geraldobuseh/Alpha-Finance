# PQL-025 Architecture Review

ARCHITECTURE READY. Independent review selected a pure reconstruction layer composing
Portfolio::replay and DailyValuation, without database/provider or strategy changes.

HistoricalReplay::at returns owned state at any inclusive timestamp cutoff.
HistoricalReplay::atClose selects exact-session prices from historical bars and returns
state plus completed-session valuation. Intraday state and daily pricing remain distinct.

Full-ledger validation precedes prefix selection so malformed future events cannot hide
an out-of-order earlier transaction. Preserve authoritative input order including ties;
never sort by order ID. Errors surface instead of repairing financial input. Historical
marks cannot be replaced by stale/current/future quotes. Returned values own their data.

Validation requires known fee-bearing buys/sales, cutoff inclusion, equal-time order,
invalid ledger identities/chronology, future exclusions, missing marks, repeated equality
and equivalence with direct prefix replay. No new schema or persistence work is needed.

Caller obligations: portfolio lifetime, completed exchange close, raw share/price basis.
Sequence scheduling, strategy backtesting, cash flows, corporate actions and performance
optimization remain separate bets.
