# Strategy scorecard

## What did I think before?

A list of good-looking metrics can seem self-explanatory. In practice, each number
needs the same reporting window and an explicit convention before comparisons mean
anything.

## What is the concept?

`StrategyScorecard` is a deterministic summary of one strategy run from inception
through a final cutoff. Its "Metric / Value" presentation contains starting and
ending capital, total and SPY return, excess return, maximum drawdown, volatility,
simplified Sharpe, executed-trade count, turnover and fees.

The input curves include a real pre-trade starting-capital observation. Strategy
and SPY observations must have exactly matching timestamps. This captures initial
fees and losses in total return and drawdown without inventing a timestamp.

For volatility and Sharpe, every adjacent interval is treated as one comparable
period return. If the first pre-trade observation is a same-session seed, it is
valid for total return and drawdown but not daily risk. For daily risk metrics,
construct the curve so the inception-to-first observation interval is a full
reporting period comparable to the later intervals.

## Why does Personal Quant Lab need it?

No single metric establishes that a strategy is useful. Return needs its passive
SPY comparison; drawdown and volatility show different forms of risk; Sharpe is a
simplified risk-adjusted view; turnover and fees reveal how much trading was needed.

## Definitions and assumptions

- Total return is ending strategy equity divided by starting capital, minus one.
- SPY return uses its own aligned starting and ending equity.
- `excess_return` is strategy return minus SPY return. It is not financial alpha.
- Maximum drawdown uses every observed strategy equity point, including inception.
- Volatility is sample standard deviation of adjacent simple returns.
- Sharpe is the existing zero-risk-free arithmetic mean divided by sample deviation.
- Number of trades counts accepted transaction fills. It does not count proposals,
  rejected orders, or round trips.
- Turnover is gross executed notional divided by arithmetic mean sampled strategy
  equity. Both buys and sells count; there is no division by two or annualization.
- Fees are summed from transactions. They already reduce ledger cash and returns,
  so the scorecard does not deduct them again.

Optional `periods_per_year` annualizes volatility and Sharpe using their existing
square-root-of-time assumption. It does not annualize turnover or total return.

The caller remains responsible for complete market sessions and comparable currency,
cash-flow, fee, funding, and SPY dividend/corporate-action conventions. Matching
timestamps alone cannot prove that the benchmark actually represents SPY. The
builder rejects equity values that contradict the replayed ledger: cash-only
observations must equal replayed cash, and observations with open positions cannot
fall below replayed cash. Marks for open positions remain trusted input until the
valuation engine provides typed daily valuations.

## What could go wrong?

Starting the curve after the first trade can hide initial fees and drawdown. Using
a same-session seed as a daily return biases volatility and Sharpe. Dropping an
undefined internal return also biases volatility and Sharpe, so the builder rejects
a curve that tries to continue after zero equity. Fees must not be subtracted twice.
Daily excess returns must not be added to produce cumulative excess return.

## Tiny example

A strategy starts at $1,000, rises to $1,100, and ends at $990. Its return is -1%
and its maximum drawdown is -10%. If SPY ends at $1,020 from $1,000, SPY returned
2%, so excess return is -3 percentage points. A $200 buy and $120 sell create $320
of two-sided gross notional; two fills means two trades.

## Remember six months from now

A scorecard is only comparable when every metric describes the same window and
accounting conventions. Positive excess return is outperformance, not proof of alpha.
