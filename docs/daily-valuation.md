# Daily portfolio valuation (PQL-024)

`DailyValuation::calculate` is a deterministic, database-free calculation over an
owning portfolio snapshot, a session date, an explicit closing timestamp, completed
raw USD-equity bars, and an optional preceding valuation.

- Cash is ledger cash through the close, including executed trade fees.
- Position value is the sum of open quantities times their session closing prices.
- Total value is cash plus position value. Realized P&L is already reflected in cash.
- Daily return is `(total - previous_total) / previous_total`.
- Cumulative return is `(total - starting_cash) / starting_cash`.

Returns are fractional: 0.01 means 1%. The first daily return is unavailable, not
zero. A zero denominator also produces an unavailable return. Losing all positive
capital is a valid -1 return. Cumulative return retains first-day trading costs;
1000 -> 1100 -> 990 means +10%, then -10%, and -1% cumulative, not 0%.

These formulas assume no external deposits or withdrawals, matching the current
trade-only ledger. External flows, dividends and corporate actions require explicit
accounting work before these formulas can describe those scenarios correctly.

## Session and pricing contract

The caller supplies an actual exchange closing instant on the session's UTC date,
including early closes and daylight-saving changes. The engine does not infer close
time from midnight or run a scheduler. Bars must match that session, with unique
symbols and prices for every open holding. Closed positions need no price. Extra
valid marks are allowed but only used marks are audited. Raw closes must agree with
the ledger's share basis; adjusted total-return prices cannot substitute for raw bars.

The caller supplies consecutive intended exchange sessions. The persistence method's
`previous_session` must match the last saved session (null only for the first), so
an expected but missing session fails explicitly. Calendar correctness still belongs
to the caller. A supplied older session is not automatically evidence that intervening
dates were holidays. SPY comparisons should use the same session sequence.

## Atomic persistence

Apply `db/migrate.sql`, including migration 003. In a `PostgresUnitOfWork`, call
`valueDaily(id, session, close, source, bars, previous_session)`, then `commit()`.
The method locks the portfolio, validates the full ledger, replays the prefix through
the inclusive close, validates the preceding chain, calculates all fields and only
then inserts the snapshot and its used marks. Destruction without commit rolls back.
Errors poison the unit; a caught error cannot accidentally commit earlier writes.

Snapshots retain session, source, cutoff, ledger sequence and predecessor alongside
cash, aggregate position value and returns. `daily_valuation_marks` retains the raw
closing price of each valued holding. Identical retries are no-ops; conflicts in
close, source, marks or values reject. New historical backfills and trades executed
at or before a completed snapshot reject rather than silently making returns stale.
A governed rebuild workflow is a separate ticket. Trades later than a close can
already exist when an earlier valuation is calculated: only the historical prefix
contributes to that snapshot.

`dailyValuation(id, session)` recomputes the stored chain from audited marks and
ledger prefixes before returning a result. Missing snapshots return null; a missing
portfolio or inconsistent persisted data raises an error. This intentionally favors
correctness over large-history performance; profile before adding cached projections.

PostgreSQL's existing generated `total_value` is the exact decimal sum of canonical
stored cash and position values. The C++ result recomputes the binary-double sum
from validated components; returns use that same C++ total before and after reload.
The SQL and C++ sums can differ in their final rounding digit. The strict canonical
reader for independently stored amounts and returns is unchanged.

## Validation and operational boundary

Synthetic tests establish arithmetic and boundary behavior; PostgreSQL tests cover
migration constraints, reconnect, retries, historical replay, concurrent writers and
rollback. No external market API is used in these tests. The writer is recoverable by
transaction rollback, with explicit conflict errors as failure signals and returned
completed values as success signals. It does not deploy scheduling, change a live
portfolio or demonstrate any investment strategy's profitability.
