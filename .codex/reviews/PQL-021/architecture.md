# PQL-021: Architecture Review

ARCHITECTURE READY. Recorded from architect design review.

Stateless DCA uses accepted ledger state, UTC Monday and fixed25 cash budget.
Highest accepted ID plus one is guarded against overflow and explicitly scoped to
sequential fresh-snapshot execution. Shared fractional sizing avoids divergence
from the benchmark. No allocator framework, scheduler or cash deposits introduced.
Rejected proposals can retry; accepted same-day SPY buys suppress new proposals.
