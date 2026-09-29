# PQL-032 Architecture Review

Decision: ARCHITECTURE READY

Objective: run one strategy over ordered historical trading days using the ticket's
run(Strategy&, HistoricalMarketData&, Portfolio) interface.

Design: owned historical day inputs; Strategy receives detached market/portfolio
state, FakeBroker executes ordered proposals into a local portfolio candidate,
Portfolio records transactions, DailyValuation produces end-of-day observations.
The free run wrapper uses default zero costs. No persistence or API changes.

Invariants: validate all dates and unique, complete quote/mark agreement before
strategy callbacks; reject initial future ledger entries before strategy access;
never mutate the caller's portfolio; never return a partially completed run.
Repeatability requires equivalent initial strategy state. Caller resets mutable
strategies before retry. Sessions/calendar completeness remain caller responsibilities.

Initial review required fixes for conflicting same-close prices and future initial
transactions reaching strategy code. Both are resolved and covered by regressions.
Final independent architecture review: no unresolved required findings.

Adjacent: benchmark orchestration, run metadata persistence, calendar generation,
and generic strategy reset are outside this ticket.
