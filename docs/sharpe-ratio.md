# Simplified Sharpe-style return (PQL-029)

The initial API measures arithmetic mean return per unit of observed sample volatility:

`per-period Sharpe = mean(return) / sample_standard_deviation(return)`.

This deliberately assumes a **zero per-period risk-free return**. It is a modeling
convention, not a statement about current interest rates or funding costs. A fuller
Sharpe calculation would use excess returns over a matched risk-free return series;
that data and its compounding/time conventions are outside this first implementation.

## Derivation and example

A return has fractional units; so does its standard deviation. Dividing average return
by that variability gives a dimensionless measure of reward relative to dispersion.
Use an arithmetic average and the same n-1 sample deviation introduced in PQL-028.
Do not divide geometric cumulative or annualized return by sample daily volatility.

For [0%, 2%], the mean is 1%. Squared deviations from the mean sum to 0.0002;
division by n-1 = 1 and the square root give standard deviation 0.0141421.
The ratio is `0.01 / 0.0141421 = 0.707107` per observation period.
A reversed-sign example [-2%, 0%] produces -0.707107. [-1%, 0%, +1%] has a defined
zero ratio because its mean is zero while dispersion is positive.

```cpp
#include "analytics/sharpe.hpp"

auto observed = pql::sampleSharpe({0.0, 0.02});
auto annual = pql::sampleSharpe({0.0, 0.02}, 252);
// observed->per_period is approximately 0.707107; annualized is absent.
// annual->annualized is approximately 11.22497; observations is 2.
```

The large annualized value from two observations illustrates how little a large ratio
alone proves. A tiny sample is not reliable evidence of sustained performance.

## Sampling and annualization assumptions

Supply finite fractional simple returns >= -1 over comparable, equally spaced periods.
0.01 means 1%. The current nonnegative portfolio model does not support losses below
-100%. Use consistent fees and funding assumptions; no external cash-flow correction
is introduced. Missing observations cannot be replaced by zero or silently omitted.
Choose a valid window after the first undefined daily return.

Annualization is opt-in with a positive periods-per-year convention:

`annualized Sharpe = per-period Sharpe * sqrt(periods_per_year)`.

With zero risk-free return, conventional arithmetic mean scaling gives k*mean, while
volatility scales as sqrt(k)*deviation. Their quotient yields sqrt(k)*Sharpe. This
assumes stable return moments and negligible serial covariance; it is not an exact
model of compounded simple-return wealth. 252 daily trading observations, 52 weekly
observations or 12 monthly observations are explicit assumptions, never inferred.
This differs from ACT/365 geometric return annualization.

## Undefined results and errors

Fewer than two returns, or zero sample dispersion, yields no ratio. Constant gains,
constant losses and all-zero returns all have a zero denominator: none receives an
infinite or fabricated zero Sharpe. All input values and supplied frequency validate
before an undefined shortcut. Invalid values, zero frequency and unrepresentable
arithmetic raise SharpeError.

The implementation reuses sampleVolatility and computes a scaled, compensated mean.
It avoids raw-sum overflow and preserves small residuals when gains and losses cancel.
It annualizes the ratio directly, avoiding unnecessary overflow of annualized volatility.
If scaling, the mean, deviation or ratio enters the subnormal range (where floating-point
values have reduced relative precision), or a nonzero intermediate becomes zero, the
calculation rejects explicitly even when a more sophisticated algorithm might recover
a finite ratio. Constant samples still return absence before these arithmetic checks.
Inputs are unchanged; repeated
identical ordered inputs give identical results in the same build.

## What it does not establish

A higher historical ratio indicates more arithmetic mean return per unit of measured
variability under these conventions. It does not prove an investment edge or forecast
future outcomes. Standard deviation treats upside and downside symmetrically and does
not measure maximum drawdown, liquidity risk, tail-loss severity or recovery time.
Skewness, heavy tails, smoothing/illiquidity, serial correlation, small samples and
selection of the best backtest can all make a high ratio misleading.

Negative ratios need particular care: a losing strategy can appear less negative merely
because volatility increased. Do not rank strategies on this single number. Compare
returns, drawdown, volatility, fees and SPY using matched windows and sampling conventions.
A real risk-free series, uncertainty estimates and dependence corrections are future
work to justify with a research question, not hidden assumptions in this first version.
