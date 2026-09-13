# PQL-018: Architecture Review

ARCHITECTURE READY. Recorded from architect's design review.

TradingCosts owns validated commission and named-unit slippage values. Existing
Money constructor delegates to zero-slippage configuration. Adjusted Price drives
affordability and the ledger; commission alone is recorded as fees. No broker
interface, persistence or strategy mutation changes. Invalid arithmetic rejects
before financial mutation. No blocking findings.
