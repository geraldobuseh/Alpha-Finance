# PQL-022: Validation Report

VALIDATION PASSED. Validator static review found no blockers; root executed the
builds and tests below. The duplicate batch declaration reported by C++ review was
removed before the builds.

## Acceptance evidence

- PASS: 20-period simple returns use 21 aligned completed closes, selecting the
  last 21 when more are supplied. Synthetic AAPL/MSFT winners are mathematically
  explicit; ranking is deterministic for ties and all-negative returns.
- PASS: two equal-budget buys follow ranking; weekly rotation sells holdings before
  new buys, with commission/slippage and retained cash accounted for.
- PASS: Monday-only cadence, history-based repeat suppression and recreated-strategy
  behavior preserve the dedicated-account contract.
- PASS: later-leg failure rolls back earlier staged sells/buys; same-Monday retry
  succeeds, stale committed batches reject and the SPY control remains unchanged.
- PASS: missing/short/misaligned history, current/future bars, duplicates, wrong
  symbols, missing held quotes, numeric extremes and ID exhaustion are covered.

Ten new Momentum tests supplement the existing strategy, broker, DCA and benchmark
regressions. Snapshot ownership and multiasset symbol lookup are also tested.

## Executed checks

Windows, in the VS2022 x64 developer environment:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC C++20 Debug build passed; all 14 default CTest suites passed.

Linux:

```powershell
docker run --rm --pull never --network none --mount 'type=bind,source=C:/Users/geral/personal-quant-lab,target=/workspace,readonly' --entrypoint sh personal-quant-lab-persistence_tests /workspace/test/offline/run.sh
```

GCC 14.2 C++20 Debug build passed; all 14 suites passed with networking disabled.
The runner disables sanitizers and optional persistence/provider targets.
clang-format 22.1.8 checks for changed C++ files and git diff --check passed.

## Remaining validation debt

CI, sanitizers and optional PostgreSQL/provider suites were not rerun. Allocation
failures were inspected, not injected; batch-specific start-time rejection is not
directly exercised. Aligned history does not prove calendar completeness, freshness,
publication availability or total-return adjustment. Dedicated accounts, matching
quote/cost settings and immediate atomic execution remain caller obligations.
