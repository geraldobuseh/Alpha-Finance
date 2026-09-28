# PQL-029 Architecture Review

ARCHITECTURE READY. Independent review selected sampleSharpe(vector<double>, optional
periods_per_year), returning per-period ratio, optional annualized ratio and sample count.
Reuse sampleVolatility input validation and n-1 dispersion in pql_domain.

Initial risk-free rate is fixed at zero and explicit. Arithmetic mean divided by sample
deviation; optional annualization multiplies the ratio by sqrt(frequency). No inferred
calendar, rate-provider, persistence or geometric-return numerator.

Small samples and zero dispersion return absence after validation. Scaled compensated
summation avoids raw-sum overflow and retains cancellation residuals. Reject invalid or
unrepresentable calculations explicitly. All values are owned, inputs remain unchanged.

Required evidence: known signs/ratios, constants, invalid samples, explicit frequency,
cancellation, large/tiny magnitudes, deterministic behavior and explained assumptions.
