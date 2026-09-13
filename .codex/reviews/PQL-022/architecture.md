# PQL-022: Architecture Review

ARCHITECTURE READY. Recorded from architect design review.

Validated owned multiasset snapshots supply current quotes and aligned prior-session
history. An explicit universe avoids inadvertently ranking control-only assets.
Momentum remains proposal-only: deterministic full sells then two equal-budget buys.
Atomic paper batch execution is required so a rejected later leg cannot publish
partial financial consequences or trigger the Monday completion gate.

Candidate portfolios and receipts are prepared before one strong assignment.
Nonconst optional<vector<Execution>> returns through a verified noexcept move.
No strategy-side broker injection, portfolio mutation or persistence rewrite.
