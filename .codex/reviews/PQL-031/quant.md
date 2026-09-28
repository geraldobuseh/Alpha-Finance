# PQL-031 Quant Review

Status: passed

The scorecard uses documented, initial conventions:

- Total return: ending strategy equity divided by starting strategy equity,
  minus one.
- SPY return: aligned benchmark ending equity divided by aligned benchmark
  starting equity, minus one.
- Excess return: `strategy return - SPY return`. It is not called alpha.
- Maximum drawdown: peak-to-later-trough drawdown across strategy equity points.
- Volatility: sample standard deviation of adjacent simple strategy returns.
- Sharpe: existing simplified zero-risk-free Sharpe calculation.
- Trades: accepted transaction fill count.
- Turnover: two-sided gross executed notional divided by mean sampled strategy
  equity, with no division by two and no annualization.
- Fees: summed from transactions and not deducted a second time.

Final rereview found the previous ledger/equity inconsistency and negative SPY
observation gaps resolved. Comparable reporting periods and trusted benchmark
composition remain caller responsibilities and are documented.
