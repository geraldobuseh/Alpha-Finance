# PQL-017: Architecture Review

ARCHITECTURE READY. Recorded from architect agent's design review.

MarketState owns one typed symbol/price/time quote. FakeBroker borrows a trusted
Portfolio reference and configures a nonnegative flat fee. Execution contains IDs
and either a rejection or allocation-free scalar fill receipt. Existing domain
construction barriers reject zero quantities and malformed symbols.

Portfolio::applyTransaction remains the only mutation boundary. The result is
staged before mutation and is statically nothrow copy/move constructible, avoiding
postcommit allocation failure. No raw request overload, provider dependency or
speculative historical/live implementation is added. No blocking findings.
