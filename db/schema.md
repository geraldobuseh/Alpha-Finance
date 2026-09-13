# PQL-010: column constraint rationale

## Storage contract

All columns are NOT NULL unless explicitly described as nullable below. A missing value would otherwise bypass CHECK/FK validation or make replay or valuation ambiguous. Primary keys reject duplicate identities and NULLs. Foreign keys use restrictive defaults: deleting referenced financial evidence must fail rather than cascade.

Amounts use unconstrained PostgreSQL NUMERIC: supplied decimal scale is preserved instead of silently rounded. `positive_amount` rejects zero, negatives, NaN and either infinity; `nonnegative_amount` permits zero; `finite_amount` permits signed profit/loss. This is exact decimal storage, while the current C++ engine uses double. A future adapter must define conversion, range, and rounding explicitly and test replay across that boundary. Currency is USD only; this schema does not add FX conversion.

`event_time` rejects infinite timestamps and stored instants not aligned to milliseconds, matching the domain clock. PostgreSQL parses at microsecond resolution before this check; the future input adapter must reject precision loss before binding. Time-zone-aware storage identifies instants; display time zone does not change the instant. `nonblank_text` rejects empty/whitespace-only strings. Symbols preserve case and use the current domain grammar. No wall-clock-dependent CHECKs are used: historical/future experiment timestamps must remain reproducible.

## Columns and invalid states prevented

### assets

| Column | Constraint and invalid state prevented |
| --- | --- |
| asset_id | Positive identity PK: duplicate, missing, or nonpositive asset identity. Explicit IDs remain possible for imports. |
| symbol | UNIQUE, domain-compatible grammar, C collation: duplicate exact symbol, blank/malformed ticker. Composite UNIQUE with asset_id lets event FKs enforce matching identity and symbol. |
| name | Nonblank: unnamed asset. |
| currency | USD CHECK: unsupported currency silently entering USD calculations. |

### market_prices

| Column | Constraint and invalid state prevented |
| --- | --- |
| asset_id | FK assets, part of PK: unknown asset or duplicate observation identity. |
| observed_at | Event time, part of PK: ambiguous instant or duplicate observation. |
| source | Nonblank, part of PK: unattributed prices or provider collisions. |
| adjustment | Enumerated raw/split_adjusted/total_return_adjusted, part of PK: unknown treatment or overwriting one treatment with another. |
| open | Positive; within low/high: nonfinite/nonpositive or impossible opening price. |
| high | Positive; >= low/open/close: impossible upper bound. |
| low | Positive; <= high/open/close: impossible lower bound. |
| close | Positive; within low/high: impossible closing price. |
| volume | Nonnegative: negative or nonfinite volume. Fractional volume is intentionally permitted. |

Rows are observations at an instant, not arbitrary mixed bar intervals. A future ingestion contract must define cadence, source precedence and missing-data policy.

### portfolios

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | Positive identity PK: invalid or duplicate identity. |
| name | Nonblank: unnamed portfolio. Names need not be globally unique. |
| kind | real/simulated CHECK: unknown accounting context. Both represent tracked records; neither enables live execution. |
| currency | USD CHECK and immutable starting-state trigger: mixed-currency replay. |
| starting_cash | Nonnegative and immutable trigger: invalid seed balance or rewriting all historical results. |

### strategies

| Column | Constraint and invalid state prevented |
| --- | --- |
| strategy_id | Positive identity PK: invalid or duplicate identity. |
| name | Nonblank, UNIQUE with version: unnamed or duplicated strategy definition. |
| version | Nonblank, UNIQUE with name and with strategy_id: unversioned definition; composite run FK prevents version/ID disagreement. |

### strategy_runs

| Column | Constraint and invalid state prevented |
| --- | --- |
| run_id | Positive identity PK: invalid or duplicate run. UNIQUE with portfolio_id supports scoped order references. |
| portfolio_id | FK portfolios: experiment for an unknown portfolio. |
| strategy_id | Composite FK strategies: unknown strategy. |
| strategy_version | Nonblank, composite FK strategies: version inconsistent with the strategy identity. |
| range_start | Event time: missing/invalid experiment start. |
| range_end | Event time, >= start: reversed experiment interval. |
| starting_capital | Nonnegative: invalid experiment capital. |
| parameters | JSON object CHECK: scalar/array instead of named configuration. Empty object means defaults. |
| universe | Nonempty JSON array of strings: missing universe or incorrectly typed members. The writer must validate symbol grammar, uniqueness and asset membership. |
| fee_per_order | Nonnegative: invalid fixed per-order fee assumption. |
| slippage_bps | Nonnegative, <=10000: invalid proportional adverse slippage assumption. Actual fills must still have positive prices. |
| adjustment | Enumerated adjustment CHECK: unspecified/unknown price treatment. |
| status | planned/running/completed/failed CHECK: unknown lifecycle state. Transition ordering belongs to the writer. |

### orders

Orders are submitted requests: status remains Pending as in the current C++ model. An execution's existence identifies a fill; this schema does not add a mutable lifecycle or partial fills.

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | FK portfolios, scoped PK: unknown owner or collision with another portfolio's order ID. |
| order_id | Positive, scoped PK: nonpositive or duplicate request within a portfolio. |
| asset_id | FK assets: request for unknown asset. |
| side | Buy/Sell domain: unsupported direction. |
| quantity | Positive: zero, negative, nonfinite order size. |
| order_type | Market-only CHECK: unsupported limit/stop/options order semantics. |
| status | Pending-only CHECK: unsupported request state. |
| submitted_at | Event time: invalid request instant. Wide UNIQUE includes request facts so executions must match them. |
| strategy_run_id | Nullable for manual orders; scoped FK run: unknown run or attaching another portfolio's run. |

### executions

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | Scoped PK and composite order FK: fill in the wrong portfolio. |
| order_id | Scoped PK and composite order FK: unknown request or second fill (partial fills unsupported). |
| asset_id | Composite order FK: fill for a different asset. |
| side | Buy/Sell and composite order FK: fill reversing request direction. |
| quantity | Positive and composite order FK: invalid size or partial/overfill. |
| submitted_at | Event time and composite order FK: changed request time. |
| price | Positive: invalid fill price. |
| fees | Nonnegative: invalid fees; rebates are unsupported. |
| executed_at | Event time, >= submitted_at: execution before request. |

Wide UNIQUE covers fill facts for transaction references. Statement triggers reject UPDATE, DELETE and TRUNCATE of executions and transactions, even on an empty table. Privileged owners can deliberately bypass triggers; production roles remain a separate deployment concern.

### transactions

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | Scoped PK and execution FK: event without an owning executed request. |
| replay_sequence | Positive, scoped PK: missing ordering or conflicting replay positions, including timestamp ties. Gaps are permitted. |
| order_id | UNIQUE with portfolio, composite execution FK: double-booking a fill or recording an unexecuted request. |
| asset_id | Composite asset/symbol and execution FKs: wrong asset identity. |
| symbol | Composite asset FK: symbol inconsistent with asset; matches the immutable domain record. |
| side | Buy/Sell and composite execution FK: direction inconsistent with fill. |
| quantity | Positive and composite execution FK: size inconsistent with fill. |
| price | Positive and composite execution FK: price inconsistent with fill. |
| fees | Nonnegative and composite execution FK: fees inconsistent with fill. |
| executed_at | Event time and composite execution FK: timestamp inconsistent with fill. |

### positions

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | FK portfolios, scoped PK: unknown owner or duplicate holding. |
| asset_id | FK assets, scoped PK: unknown or duplicate asset holding. |
| quantity | Nonnegative: short/nonfinite holding. Zero retains liquidation history. |
| average_cost | Nullable only when quantity=0; positive when held: invented cost for a closed position or missing cost for held shares. |
| realized_pnl | Finite signed: nonfinite results. Legitimate losses are allowed. |
| as_of | Event time: ambiguous projection time. |
| ledger_sequence | Scoped transaction FK: cutoff that does not exist in this portfolio. |

### portfolio_snapshots

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | FK portfolios, scoped PK: unknown owner or duplicate valuation. |
| as_of | Event time, scoped PK: invalid or duplicate valuation instant. |
| ledger_sequence | Nullable for initial state; scoped transaction FK otherwise: unknown/cross-portfolio replay cutoff. |
| cash | Nonnegative: negative/nonfinite long-only cash projection. |
| market_value | Nonnegative: negative/nonfinite holdings valuation. |
| total_value | Generated cash + market_value: caller-supplied total disagreeing with components. |

### benchmark_snapshots

| Column | Constraint and invalid state prevented |
| --- | --- |
| portfolio_id | PK and composite portfolio snapshot FK: comparison without corresponding portfolio valuation. |
| as_of | Event time, PK and snapshot FK, >= range_start: mismatched valuation time or reversed comparison interval. |
| asset_id | Composite asset FK: unknown benchmark asset. |
| symbol | SPY CHECK and composite asset FK: mislabeling a different asset as SPY. |
| range_start | Event time: missing/invalid comparison origin. |
| starting_capital | Nonnegative: invalid comparison capital. |
| market_value | Nonnegative: invalid benchmark value. |
| adjustment | Enumerated CHECK: unknown treatment of splits/distributions. |

### schema_migrations

| Column | Constraint and invalid state prevented |
| --- | --- |
| version | Positive integer PK: duplicate or invalid migration number. |
| description | NOT NULL and trimmed nonempty CHECK: undocumented migration. |
| applied_at | NOT NULL finite timestamptz, defaults to transaction time: missing/infinite application audit time. Administrative metadata need not use the engine's millisecond clock. |

## Required future transactional writer rules

### PQL-014 daily session identity

`market_prices.session_date` is nullable for legacy timestamp observations. When
present it must be a finite date in years1..9999 and agree with observed_at at UTC
midnight; without this constraint, the same daily bar could have contradictory
date/time identities. A partial UNIQUE index on asset/session_date/source/adjustment
prevents duplicate daily records. Source alphavantage.daily additionally requires a
session date and raw adjustment, preventing an incomplete or mislabeled daily row.
These are session keys, not exchange close or availability timestamps.

PQL-011 now implements portfolio locking, sequence allocation, replay validation,
and atomic order/execution/transaction writes for manual simulated fills. See
[the persistence contract](../docs/persistence.md). Projection writes, strategy-run
attribution, benchmark calculations and production permissions remain future work.

Row constraints cannot establish that aggregates equal a replay. The persistence layer must lock/serialize writes per portfolio, allocate replay sequence, enforce nondecreasing execution times, validate available cash and shares, and atomically persist order/execution/transaction consequences. SQL Sell alone does not prove ownership. No strategy may receive a financial mutation interface.

Once results exist, preserve run configuration (version, parameters, universe, capital and cost assumptions) and executed-order run attribution so historical experiments cannot be relabeled by later edits. The writer must separate permitted lifecycle transitions from these frozen inputs.

Positions and snapshots are disposable projections, never the ledger. The writer must derive them from starting cash, transactions ordered by replay_sequence, and explicit valuation prices. It must verify cutoff/time consistency, that NULL snapshot cutoff really denotes initial state, and that average cost/realized P&L/cash match replay. Benchmark comparisons must use aligned capital, dates, adjustment, fees and dividend assumptions. These cross-row calculations and permissions are not implemented by this schema ticket.

Example with zero fees: start $1,000; buy 2 AAPL at $100 (cash $800); sell 1 at $120 (cash $920, 1 share, $20 realized). Mark the remaining share at $120: market value $120, total $1,040, unrealized $20. Transaction sequence defines replay order even if both fills share a timestamp. A stored balance alone cannot explain or reproduce those results.

## Daily valuations (migration 003)

`portfolio_snapshots` additionally stores an optional daily session, price source,
prior snapshot reference, and nullable fractional daily/cumulative returns. Legacy
rows remain unchanged and are not presented as completed daily valuations. A unique
partial index allows at most one daily record per portfolio/session. The predecessor
foreign key remains inside the same portfolio and must precede the current cutoff.

`daily_valuation_marks` stores each used raw close with a symbol foreign key and
parent snapshot identity. The repository calculates before writing and recomputes
on read; it rejects conflicting retries, historical insertions and late ledger events.
These writer invariants do not authorize direct SQL modification of financial data.
See [daily valuation contracts](../docs/daily-valuation.md) for return definitions,
zero denominators, source/mark assumptions and decimal-versus-binary total semantics.
