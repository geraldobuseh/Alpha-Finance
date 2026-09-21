# PQL-028 Quantitative Correctness Review

QUANT REVIEW PASSED for financial design and implementation. Sample standard deviation
is sqrt(sum((r-mean)^2)/(n-1)); outputs use fractional units. [-.01,0,.01] yields .01.
Constant negative returns can have zero volatility despite losing money. Sample
variance's n-1 correction does not imply an unbiased standard-deviation estimator.

Annualization uses sigma*sqrt(periods_per_year) with an explicit positive convention,
not ACT/365 annualized return. Square-root scaling relies on stable variance and
negligible serial covariance; it approximates compounded simple-return uncertainty.
Autocorrelation, regime changes and heavy tails limit interpretation.

Volatility treats upside/downside symmetrically and does not establish expected return,
maximum drawdown, tail probability, liquidity risk or a future loss bound. Compare SPY
using matched periods, dates, costs, sample convention and missing-data policy.

Undefined small samples are absent; invalid data must never be dropped or replaced
with zero. Negative simple returns below -1 are outside the current portfolio model.
Unrepresentable arithmetic fails explicitly. No unresolved financial finding remains.
