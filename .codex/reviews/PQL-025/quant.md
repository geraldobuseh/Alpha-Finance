# PQL-025 Quantitative Correctness Review

QUANT REVIEW PASSED (independent design review).

Starting cash and recorded fills/fees determine cash, quantity, cost basis and realized
P&L. Historical closes mark holdings; they must never substitute for execution prices.
Use inclusive timestamp cutoff and preserve canonical equal-time ledger ordering.
Order IDs are identity, not chronology. Valid later events cannot change earlier state.

The entire supplied ledger is validated first, including its future suffix. Reject
wrong portfolio, duplicates, out-of-order trades, oversells/overspending and arithmetic
failures rather than silently filtering them away. Invalid future events therefore
reject the request; this is ledger integrity validation, not valuation look-ahead.

State-only replay supports arbitrary timestamps. Daily bars require an explicitly
completed exchange close. No intraday-price inference or stale/future price fallback.
The caller owns portfolio lifetime (seed has no timestamp), session calendar and raw
corporate-action share basis. No external cash flows/dividend/split model is introduced.

Single-close daily return is unavailable; cumulative return reuses the original seed
and fee-aware valuation rules. Financial tests use mathematically obvious synthetic
prices, not external APIs or claims of strategy profitability.
