# PQL-030 Architecture Review

ARCHITECTURE READY. Independent architect reviewed the proposed design.

Add excess_return(strategy_return, spy_return) to existing analytics/returns module
and extend ReturnsTest. No new target, persistence schema or benchmark orchestration.
Finite fractional simple-return inputs must be at least -1; the difference itself
can be below -1. Caller ensures comparable periods and return conventions.
Pure deterministic subtraction preserves analytics as a consumer of portfolio state.
Tests must cover signs, equal returns, invalid inputs and numerical extremes.
