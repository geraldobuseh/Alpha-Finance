# Returns and compounding

## What did I think before?

A gain of 10% followed by a loss of 10% might seem to cancel. Percentages apply to
changing balances, so adding them does not describe the investor's wealth change.

## What is the concept?

Let B be the beginning value and E the ending value, with no deposits or withdrawals.
The gain is E - B. Dividing by the capital that earned it gives the fractional return:

`r = (E - B) / B`, hence `E = B * (1 + r)`.

For consecutive closing values this is daily return. For original starting capital
and the final value it is cumulative return. Fees already reflected in portfolio
value must not be subtracted again. A return of 0.10 means 10%, not 0.10%.

Apply the wealth equation twice:

`V1 = V0 * (1 + r1)`

`V2 = V1 * (1 + r2) = V0 * (1 + r1) * (1 + r2)`

Therefore cumulative return is `product(1 + daily_return) - 1`. The intermediate
balances cancel when the ratios are multiplied. Adding returns omits the interaction
term: for two periods the result is `r1 + r2 + r1*r2`.

To derive annualization, ask which constant annual compound rate a would produce
the observed growth over y years. It must satisfy:

`E = B * (1 + a)^y`.

Divide by B, take the power 1/y and subtract 1:

`a = (E/B)^(1/y) - 1`.

PQL uses ACT/365 Fixed: `y = elapsed_calendar_days / 365`. Thus the exponent is
`365 / elapsed_calendar_days`. Count leap days in elapsed time; the basis stays 365.
Trading-session counts are not calendar-day counts. The caller supplies a positive
`std::chrono::days` duration measured from the starting-value observation; the first
post-trade snapshot does not establish inception. Annualization is an equivalent
historical compound rate, not a prediction, and short samples can annualize dramatically.

## Why does Personal Quant Lab need it?

Daily, cumulative and annualized returns answer different questions about the same
wealth path. Reuse one calculation boundary so portfolio reporting and experiments
agree. Compare portfolio and SPY over consistent intervals and with the same annual
basis. Cumulative excess return is not automatically risk-adjusted alpha.

## What failure would occur if we misunderstood it?

Adding daily returns can report a gain where wealth fell. Using the first post-fee
snapshot as original capital hides initial costs. Treating deposits as investment
gains overstates performance; these formulas assume no external flows, matching the
current ledger. Flow-aware returns require a separate model.

A zero beginning value makes division undefined, so the API returns no value. Negative
portfolio values and nonpositive annualization durations are rejected. A positive
balance ending at zero has a valid -100% return. Nonfinite results or a positive ending
value rounded into a complete loss are explicit errors, not silently clipped values.

## What tiny example makes it intuitive?

$100 gains 10% to become $110. Losing 10% of $110 loses $11, leaving $99:

`1.10 * 0.90 - 1 = -0.01`, a 1% cumulative loss.

To recover from a 10% loss, the remaining $90 needs $10/$90 = 11.111...% growth.
A 50% loss needs a 100% gain to recover.

$100 becoming $121 over 730 calendar days has 21% cumulative return. Its annual
compound equivalent is 10%, since `100 * 1.10 * 1.10 = 121` and 730/365 = 2.

## What should I remember six months from now?

Returns multiply through wealth factors. Always name the baseline, elapsed interval,
annual basis and funding assumptions. The implementation preserves the existing
subtraction-before-division daily/cumulative calculation for persisted-value consistency.
Annualization uses log1p/expm1 near unchanged values and log differences for extreme
ratios, retaining small changes while avoiding unnecessary intermediate overflow.
