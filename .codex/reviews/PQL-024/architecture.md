# PQL-024 Architecture Review

## Objective and design
ARCHITECTURE READY, based on independent review of the existing portfolio and
PostgresUnitOfWork surfaces. Reuse marketValue/totalValue, immutable ledger replay,
portfolio row locks and the existing portfolio_snapshots table.

A pure DailyValuation owns calculated values, session/cutoff, predecessor and used
marks. ValuationRepository exposes calculate-and-store plus validated retrieval.
Migration 003 adds daily provenance/returns and a used-mark child table without
inventing returns for legacy snapshots. Database operations share one transaction.

## Contracts
Explicit exchange close on the supplied UTC session date, raw completed USD-equity
bars, and an explicit expected preceding session. No external flows. Daily return
is unavailable on the first observation or zero denominator; cumulative return uses
immutable starting capital. The caller supplies complete exchange sessions.

## Invariants and resolved risks
No future transactions in a historical valuation. Required marks are unique and
correctly dated. Identical retries are no-ops; conflicts/backfills reject. Late trades
cannot invalidate completed snapshots through the repository. Prior valuations are
checked against the ledger prefix. Generated SQL total is exact decimal addition;
C++ reload recomputes the binary total from validated components deliberately.

## Scope
No calendar service, scheduler, external-flow return model, dashboard, dividends,
corporate-action accounting or rebuild workflow. Large-history performance is
unmeasured; full-chain replay favors correctness over premature optimization.
