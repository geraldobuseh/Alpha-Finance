# PQL-026 Architecture Review

ARCHITECTURE READY. Extract reusable pure daily/cumulative/annualized returns into
analytics/returns.hpp/.cpp in pql_domain. DailyValuation delegates to the module and
translates ReturnError to its existing ValuationError contract.

Preserve subtraction-before-division and representability checks for simple returns:
persisted snapshots compare their recomputed returns exactly. The public functions
also reject negative Money inputs; Money itself allows negative values.

Chosen time basis: explicit std::chrono::days with ACT/365 Fixed. No inference from
snapshot count or first post-trade observation. This avoids adding a second trading-day
convention. No schema changes or automatic annualization of dateless seeds.

Invariants: absent zero-denominator returns; positive elapsed duration; finite results;
total loss valid; tiny nonzero results and remaining wealth never silently erased.
Numerical stability may use logarithms without requiring representable cumulative return.

Required validation includes exact persistence compatibility, compounding, two-year
100 -> 121 annual10%, losses, zero/negative inputs, tiny changes and extreme ratios.
External cash flows, annualized database fields and return-series reporting are separate bets.
