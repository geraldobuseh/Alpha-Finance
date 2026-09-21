# Maximum drawdown (PQL-027)

A return describes the change between endpoints. Drawdown describes a decline from
a previously observed high, even if the portfolio later recovers.

For observation t, let P be the greatest value seen up to that observation and V
its current value. The dollar change from the peak is V - P. Divide by the capital
at that peak to express the decline proportionally:

`drawdown = (V - P) / P`.

PQL reports declines as negative fractions. Maximum drawdown is the most severe
decline, so it is the minimum of these signed observations, not the largest signed
number. The peak must occur before its associated losing trough; using the global
maximum and minimum without checking order can invent a drawdown.

## Required example

| Observation | Portfolio value |
| --- | ---: |
| Initial | $1,000 |
| Peak | $1,200 |
| Later trough | $900 |
| Recovery | $1,300 |

`(900 - 1200) / 1200 = -300 / 1200 = -0.25`, or **-25%**.

The peak is $1,200 and the later trough is $900. Ending at $1,300 does not erase
the intervening loss. The largest dollar drop also need not be the largest percentage
drawdown: $100 -> $50 loses 50%, while $1,000 -> $700 loses only 30%.

## Comparing the strategies

| Strategy | Return | Maximum drawdown |
| --- | ---: | ---: |
| A | +15% | -45% |
| B | +11% | -12% |

Assuming comparable periods, costs and data quality, I would favor **B for a
risk-conscious experiment**. It gives up 4 percentage points of return while reducing
the observed peak-to-trough decline by 33 percentage points. That substantially
smaller historical loss may make it easier to stay invested and avoid forced selling.

Recovery magnifies the distinction. After losing a fraction d, the remaining wealth
is 1-d. Returning to the old peak requires a gain of `1/(1-d)-1`:

- A's 45% decline requires approximately 81.8% growth to recover.
- B's 12% decline requires approximately 13.6% growth to recover.

This is a conditional tradeoff, not proof that B is superior or that A is unsuitable
for every investor. These two statistics do not establish statistical significance,
future risk, or a complete risk-adjusted ranking. Compare horizon, costs, SPY returns,
recovery duration, sample quality and out-of-sample behavior. Historical maximum
drawdown is not a limit on future losses. No Calmar or Sharpe ratio is implied by
these numbers; return annualization and measurement conventions have not been supplied.

## API and deterministic conventions

`maximumDrawdown(const std::vector<EquityPoint>&)` consumes timestamped `Money`
values and returns an optional `MaximumDrawdown` containing the signed `fraction`,
`peak` and `trough` points. Map completed valuations to
`EquityPoint{valuation.asOf(), valuation.totalValue()}` in chronological order.

A single chronological pass keeps a running positive peak. Strictly greater values
update the peak; strictly deeper declines update the result. Equal peaks retain their
earliest timestamp and equally severe declines retain the first selected pair.
The result owns copied points and remains valid after the input is destroyed.

- Empty/all-zero input: no defined percentage baseline, so no result.
- Leading zeros: validated, but no ratio until the first positive value.
- Positive singleton, flat or rising curve: zero drawdown; both endpoints identify
  the first positive observation as a no-decline sentinel, not a losing episode.
- Positive peak followed by zero: -100% drawdown.
- Negative values, duplicate/reversed timestamps, or unrepresentable arithmetic:
  `DrawdownError`. The entire curve is validated, including after total loss.

No sorting, input mutation, clock, persistence or randomness participates. Existing
checked cumulative-return arithmetic rejects a positive trough rounded to a false
-100% loss rather than silently clipping it. This calculation does not mutate returns.

The observations must describe the same portfolio on a consistent share/price basis,
without unadjusted external deposits/withdrawals. Sampling frequency matters: daily
closing observations reveal daily closing drawdown, not unobserved intraday losses.
Missing dates are not filled in. The caller owns calendar completeness and comparability.
