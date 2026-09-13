# Historical replay (PQL-025)

`HistoricalReplay` reconstructs recorded portfolio behavior; it does not rerun a
strategy or simulate fills. Starting cash and recorded transaction quantities, fill
prices and fees are the financial source of truth. Historical prices mark holdings;
they never replace recorded execution prices.

```cpp
#include "simulation/historical_replay.hpp"

// State at any timestamp, including intraday; no closing-price assumption.
auto state = pql::HistoricalReplay::at(id, starting_cash, transactions, cutoff);

// State plus valuation at a completed exchange session close.
auto result = pql::HistoricalReplay::atClose(
    id, starting_cash, transactions, historical_bars, session, close_timestamp);
// result.state: cash, positions, cost basis, realized P&L and accepted history.
// result.valuation: used closing marks, position/total value and cumulative return.
```

## Cutoff and ordering

The cutoff is inclusive: events executed exactly at it are included. The supplied
ledger must use authoritative execution order, including ties in timestamps. Order
IDs identify events; sorting by ID can change the economics. The engine never sorts,
silently repairs, or drops corrupt ledger input.

The complete supplied ledger is validated using `Portfolio::replay` before its
historical prefix is selected and replayed. Wrong portfolio IDs, duplicate order
IDs, out-of-order events, overspending, overselling and unrepresentable arithmetic
reject the request with `ReplayError`, including invalid events in the future suffix.
Valid future events cannot affect the returned historical state. This matches the
existing persistence replay policy and prevents a future event from hiding an
out-of-order earlier event.

Starting cash has no funding timestamp in the current domain. The caller must
ensure the cutoff lies within the portfolio's intended lifetime. Before the first
trade, reconstruction returns starting cash and no positions.

## Historical prices

`atClose` selects only the requested session's bars from the supplied multi-session
raw USD-equity history. Every open holding needs a matching close. Missing or duplicate
selected marks fail through `ValuationError`. No current, future, or stale price is
used as a fallback. Other sessions do not participate in this valuation; this is not
a general historical-market-data quality audit.

The caller supplies an actual completed exchange close on the session's UTC date,
including holiday/early-close conventions. An arbitrary intraday timestamp must use
`at` for state reconstruction; a daily bar cannot establish the price known intraday.
Raw prices must agree with ledger share quantities. Adjusted prices, dividends,
splits and external deposits/withdrawals require accounting support outside this ticket.

The isolated close has no preceding daily valuation, so daily return is unavailable.
Cumulative return retains original starting cash and recorded fees, following PQL-024.
Use `DailyValuation::calculate` with an explicit prior value when assembling a
consecutive daily valuation sequence. No scheduler or database write occurs here.

## Determinism and tiny example

Start with $1,000. Buy 2 shares at $100 with a $1 fee: cash is $799. Mark them
at $110: holdings are $220 and total value is $1,019. Later sell 1 share at $120
with a $1 fee: cash is $918, quantity is 1. Reconstructing the first close still
gives $799 cash and 2 shares even when the later sale is in the input.

Repeated identical inputs yield exact equal owned snapshots and valuation fields
in the tested build. No wall clock, randomness, network, mutable global state or
parallel reduction participates. Input vectors are unchanged; results survive their
destruction. Different compiler/platform floating-point behavior is not promised
bit-for-bit identical without verification. No performance claim is made; the simple
implementation validates the full ledger and replays a prefix on each request.
