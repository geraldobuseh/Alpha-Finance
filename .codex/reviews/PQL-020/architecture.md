# PQL-020: Architecture Review

ARCHITECTURE READY. Recorded from architect design review.

PaperPortfolioPair stages distinct, equally funded accounts and completes the SPY
purchase before publication. Snapshot-only benchmark access and executePaper retain
cost and ownership boundaries. Brokers are stack-local, not references stored into
movable members. Fractional sizing shares effective-price calculation with execution,
with a bounded conservative rounding fallback. Default copy construction and deleted
assignment preserve paired state. No persistence rewrite or scheduler is required.
