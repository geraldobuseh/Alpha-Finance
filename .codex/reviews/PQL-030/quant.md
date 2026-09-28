# PQL-030 Quantitative Correctness Review

QUANT REVIEW PASSED. Independent quant reviewer approved design and final changes.
No material findings.

The calculation is strategy return minus SPY return. 12% minus 8% means four
percentage points, not relative wealth growth or true financial alpha. Positive
excess can occur during absolute losses. Inputs require aligned periods, currency,
costs, funding and dividend conventions; scalar inputs cannot enforce these.
Missing observations must not become zero. Compound each series before comparing
cumulative returns; do not add daily excess returns. No forecasting or significance
claim follows from this metric. Difference outputs may legitimately fall below -1.
