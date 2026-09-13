# PQL-023 Quantitative Correctness Review

Independent quant design review: QUANT REVIEW PASSED.

MA averages twenty completed prior session closes. Signal uses current quote;
fees/slippage affect sizing and execution. Buy strictly below 95% of MA while flat;
sell the full holding inclusively at MA. No shorts, borrowing or pyramiding.

MEDIUM ambiguity resolved: ticket omitted sizing and timing. Header and learning
note explicitly state single-symbol all-cash sizing and prior-session history.
LOW limitation: caller supplies complete sessions and consistent adjusted prices.

MarketState excludes current-day/future bars. A falling average can cause a losing
exit. Same-quote execution is a simulation assumption. Correct synthetic tests do
not demonstrate an edge. Compare empirically against SPY with consistent dates,
capital, costs, returns and risk metrics. Profitability remains unverified.

Momentum assumes persistence; mean reversion assumes reversal over a specified
horizon. Neither rule is market truth. Boundary, execution and replay coverage
was requested and independently validated in validation.md.
