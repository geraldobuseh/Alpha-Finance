# PQL-017: Validation Report

VALIDATION PASSED. Validator static review found no blockers; root implementer
executed the builds/tests below.

## Acceptance evidence

- PASS: configured-price buys and sells through Broker, including changed quotes.
- PASS: flat fee appears in receipts/history and fee-adjusted cash/cost basis/P&L.
- PASS: insufficient cash including fees, insufficient shares and missing holdings
  reject without changing the full portfolio snapshot.
- PASS: zero quantities and malformed symbols reject at existing typed factories;
  valid but unmatched symbols reject in FakeBroker.
- PASS: duplicates, reversed times, overflowing/underflowing amounts and ledger
  rounding failures reject atomically. Rejected IDs can subsequently succeed.
- PASS: zero fees, fractional shares, zero/negative sale proceeds, liquidation,
  deterministic repeated simulation and transaction replay.

## Executed checks

Windows: initialized the VS2022 x64 developer environment, then ran:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC C++20 Debug build passed; all10 default CTest suites passed, including the
10 FakeBroker fixture tests and the Broker compile-time contract checks.

Linux:

```powershell
docker run --rm --pull never --network none --mount 'type=bind,source=C:/Users/geral/personal-quant-lab,target=/workspace,readonly' --entrypoint sh personal-quant-lab-persistence_tests /workspace/test/offline/run.sh
```

GCC14.2 C++20 Debug build passed; all10 default CTest suites passed without network
access. The offline script disables sanitizers and optional persistence/provider
targets. No database or live provider was accessed.

## Remaining validation debt

No allocation-failure injection test was run; strong exception safety follows from
the existing staged portfolio update and statically nothrow receipt copies/moves.
Borrowed portfolio lifetime and sequential use are caller obligations. CI and
optional PostgreSQL/provider suites were not rerun for this in-memory addition.
