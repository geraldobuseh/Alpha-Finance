# Return volatility (PQL-028)

Volatility measures how widely returns vary around their mean. PQL calculates the
sample standard deviation of fractional simple returns, not the deviation of prices.
A return of 0.01 means 1%; a volatility of 0.01 is a one-percentage-point standard
deviation per observation period, not a 1% expected gain.

## Calculation and example

For n returns, their mean is `mean = sum(r) / n`. Each deviation `r - mean` measures
a departure from that center. Squaring avoids positive and negative departures
cancelling. Sum the squared deviations, divide by n-1, then take the square root
so the result has the same units as returns:

`sample volatility = sqrt(sum((r - mean)^2) / (n - 1))`.

Estimating the mean uses one degree of freedom. The n-1 correction makes sample
variance unbiased under independent, identically distributed observations; it does
not make its square root an unbiased standard-deviation estimator. Population
standard deviation would divide by n and is a different requested statistic.

For returns [-1%, 0%, +1%], the mean is zero. Squared fractional deviations sum to
0.0002. Dividing by 2 gives variance 0.0001; its square root is 0.01, or 1% per period.
Three consistently negative returns [-1%, -1%, -1%] have zero volatility despite
losing money. [+1%, +2%, +3%] has the same volatility as [-1%, -2%, -3%].

## API and annualization

```cpp
#include "analytics/volatility.hpp"

const std::vector<double> daily_returns{-0.01, 0.0, 0.01};
auto observed = pql::sampleVolatility(daily_returns);
auto scaled = pql::sampleVolatility(daily_returns, 252);
// observed->per_period = 0.01, observed->annualized is absent.
// scaled->annualized is about 0.158745, or 15.8745%.
// Both results report observations = 3.
```

Annualization is available now but always opt-in:

`annualized volatility = per-period volatility * sqrt(periods_per_year)`.

When equally distributed period returns have no serial covariance, the variance
of their sum is the sum of variances: k * variance. Taking its square root gives
sqrt(k) scaling. For compounded simple returns, this is a conventional approximation,
not an exact compounded-wealth distribution. Correlation adds covariance terms;
changing regimes or unstable variance can invalidate the scaling assumption.

252 for trading-session daily returns, 52 for weekly returns and 12 for monthly
returns are explicit caller conventions. The API does not infer frequency, count
calendar dates or silently supply 252. Do not use 252 with monthly observations.
This is different from the ACT/365 geometric annualized-return formula in PQL-026.

Inputs must cover comparable, equally spaced return intervals on a consistent
portfolio, cost and funding basis. Exclude the first undefined daily return by
choosing a valid observation window; do not replace it with zero. Missing internal
observations need an explicit data policy rather than silently shortening the sample
or mixing a multi-day return with one-day observations.

## Meaning and limits

Volatility describes observed dispersion. It treats upside and downside deviations
symmetrically and ignores their ordering. It does not tell you:

- Whether returns are profitable or their mean is positive.
- Maximum drawdown, recovery time or the order of gains and losses.
- A maximum possible loss or a guaranteed future risk level.
- Tail-loss probability without additional distribution assumptions. Heavy tails
  make normal-distribution rules unreliable; variance can also be unstable.
- Liquidity, leverage, model, counterparty or permanent-loss risk by itself.

Use it alongside return, maximum drawdown, fees, turnover and benchmark performance.
Compare portfolio and SPY using the same dates, intervals, missing-data policy,
sample denominator and annualization convention. A small sample yields an uncertain
estimate; displaying an annualized number does not create a year's worth of evidence.

## Validation and numerical behavior

The API returns absence for fewer than two observations after validating every input.
Finite fractional returns must be at least -1, matching the nonnegative portfolio
model. NaN, infinities, lower returns and a supplied zero annualization frequency raise
VolatilityError. Constant valid series returns zero. Invalid inputs are never dropped.

The implementation centers before scaling, then sums normalized squared deviations.
This preserves nearby large values and avoids unnecessary overflow or underflow from
squaring raw returns. A nonconstant sample that becomes unrepresentable zero, or an
annualized value that overflows, raises an explicit error. Repeating identical ordered
inputs is deterministic and leaves inputs unchanged. Cross-platform bitwise equality
and exact equality after reordering are not promised.
