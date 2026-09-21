# PQL-028 Architecture Review

ARCHITECTURE READY. Independent review selected pure sampleVolatility in pql_domain,
returning per-period standard deviation, optional annualized value and observation count.
Fractional doubles match existing return outputs. Annualization is explicitly opt-in
with a positive periods-per-year value. No schema, provider or portfolio changes.

Use sample denominator n-1; fewer than two observations yields absence after validating
all supplied data. Require finite returns >= -1. Constant valid series yields zero.
Center before scaling so nearby large values retain differences; normalized squared
deviations avoid overflow/underflow from raw squares. Reject unrepresentable results.

Same ordered inputs produce identical owned results without mutation. Caller supplies
comparable equally spaced periods and frequency. No inferred calendars, cash-flow model,
rolling window framework or return conversion pipeline is needed for this ticket.
