# PQL-026 Quantitative Correctness Review

QUANT REVIEW PASSED. Independent review confirmed financial assumptions and learning
note derivations; final focused validator run passed all ten tests.

Gain divided by original capital yields r=(E-B)/B and E=B*(1+r). Repeated wealth
multipliers yield cumulative product(1+r)-1. Solve E/B=(1+a)^years to derive geometric
annualization. ACT/365 Fixed uses actual elapsed calendar days/365, including leap days
in elapsed count. Do not mix calendar days with trading sessions.

100 -> 110 -> 99 is +10%, then -10%, but -1% cumulative. Initial fees remain in total
value and original-capital baseline; no double fee subtraction. These formulas assume
no external deposits/withdrawals. Annualization normalizes history; it is not a forecast.

Zero denominator is absent. Negative values or nonpositive durations reject. Terminal
zero from positive capital is -100%. Portfolio lifetime/starting observation date must
be supplied by caller. SPY comparisons need the same interval and annual convention.

Severe-loss cancellation and subnormal-ratio precision findings were fixed with stable
logarithm selection and explicit regressions. No unresolved financial findings remain.
