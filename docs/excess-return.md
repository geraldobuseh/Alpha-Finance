# Excess return against SPY

## What did I think before?

Beating a benchmark might sound like alpha. A raw difference alone does not
establish skill or adjust for market exposure and risk.

## What is the concept?

A simple return measures change relative to starting wealth. To compare two
returns over the same period, subtract the benchmark's change per starting dollar:

`excess_return = strategy_return - spy_return`

Inputs are fractional simple returns: 12% and 8% are `0.12` and `0.08`.
Their difference is `0.04`, or **4 percentage points**. This is neither true
financial alpha nor relative wealth growth, which would instead be
`(1 + strategy_return) / (1 + spy_return) - 1` when its denominator is positive.

## Why does Personal Quant Lab need it?

SPY is the passive baseline. This metric shows whether the strategy outperformed
that baseline before making any claim about risk-adjusted performance.
It belongs alongside drawdown, volatility and costs.

## What could go wrong?

The caller must align periods, currency, external cash-flow treatment, costs and
dividend/corporate-action conventions. Comparing price-only SPY returns against
strategy total returns would be misleading. Missing returns must remain missing;
check optional return values before calling the function, never substitute zero.

The API accepts finite simple returns at least -1, matching the current
nonnegative-wealth model. It throws `ReturnError` for invalid inputs, nonfinite
results or subtraction that completely loses a nonzero operand to rounding.
The difference itself has no -1 floor: -100% minus +100% is -200 percentage points.

Do not sum daily excess returns to obtain cumulative excess return. Compound each
series separately, then subtract their cumulative returns. The metric alone says
nothing about statistical significance, beta, factor exposure or future success.

## Tiny example

A strategy ending at $1,120 from $1,000 returns 12%; SPY ending at $1,080 from
$1,000 returns 8%. Excess return is 4 percentage points. A strategy losing 10%
while SPY loses 20% also has positive excess return: +10 percentage points,
despite losing money.

## Remember six months from now

Use `excess_return` for a comparable return difference. Positive excess return
means outperforming SPY over that period, not necessarily making money or proving
financial alpha.
