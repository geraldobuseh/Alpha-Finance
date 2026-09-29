# Deterministic backtest engine (PQL-032)

`BacktestEngine::run(Strategy&, HistoricalMarketData&, Portfolio)` processes the
supplied sessions in order. The free `pql::run` function uses zero trading costs;
construct an engine with `TradingCosts` to apply commission and adverse slippage.
Keep strategy sizing costs consistent with the engine's execution costs.

Each day supplies a detached `MarketState` and portfolio snapshot to the strategy,
executes its ordered proposals through `FakeBroker`, and records the resulting
ledger-backed portfolio, execution receipts, and `DailyValuation`. No-order days
still produce snapshots. All legs of a day's order batch must fill; a rejection
aborts the run with an order ID, timestamp, and `ExecutionRejection` enum value.
The caller's portfolio is passed by value and remains unchanged on success or
failure. Results own their snapshots and ledger histories.

## Input and timing contract

- Supply at least one trading session, strictly increasing by date and close.
  The engine never sorts, fills calendar gaps, or fetches data from the network.
- Session labels and close timestamps must agree in UTC. The caller supplies
  valid exchange sessions and actual close times; weekends, holidays and early
  closes are not inferred.
- Each market quote must have exactly one same-session closing bar, and its
  price must equal that bar's raw close. Duplicate, missing, extra, or conflicting
  marks fail validation before any strategy callback.
- `MarketState` permits only strictly ordered history before the decision day.
  Strategies see the current quote, but cannot see current-day OHLCV bars or
  future snapshots through the engine interface.
- Initial portfolio transactions must be no later than the first decision.
  Existing holdings must have valuation marks when snapshots are calculated;
  unavailable held-asset prices raise `ValuationError`, with no partial result.

Execution uses an **idealized same-close model**: strategies observe a close quote
and fill at that quote adjusted for configured costs. This is a deterministic
simulation assumption, not proof that an order based on the final close could
have filled at that price in a real market. Corporate actions, dividends, volume
constraints, intraday execution, and pending orders are outside this ticket.

## Reproducibility and interpretation

Identical historical inputs, initial portfolio, costs, and equivalent initial
strategy state produce identical results. The engine does not reset a mutable
`Strategy&`; recreate/reset stateful strategies before repeating a run, including
after failure. Strategy exceptions propagate. The data object is read-only during
the run despite the ticket's mutable-reference interface.

The first daily return is absent. Subsequent daily returns compare consecutive
supplied snapshots. Cumulative returns retain `DailyValuation` semantics: they
use the portfolio's original starting cash, including any pre-run performance.
They are not necessarily the return earned during this run alone.

This engine runs one strategy. SPY comparison, strategy version/parameter
registration, result persistence, and comparative scorecard orchestration remain
separate work; the engine makes no strategy-performance claim.

## Validation

`ctest --test-dir build -R BacktestEngineTest --output-on-failure` runs synthetic
tests for the daily loop, deterministic repetition, no-order sessions, malformed
inputs, future-data exclusion, pre-existing holdings, costs, ledger replay,
caller input preservation, and rejected multi-order batches.
