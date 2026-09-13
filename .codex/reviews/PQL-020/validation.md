# PQL-020: Validation Report

Validator static review found no blockers. Runtime checks were executed by root.

Acceptance coverage: equal fixed $1,000 starting cash, immediate SPY purchase,
holding through changing prices, detached control used alongside paper valuation,
and independent paper execution. Seven benchmark tests additionally cover fractional
quantities, rounding overspend, commission/slippage, replay, failed initialization,
start-time guards and pair copy isolation. Existing broker friction tests exercise
the extracted shared execution-price calculation.

Windows: initialized VS2022 x64 developer environment, then ran:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC C++20 Debug build passed and all12 default CTest suites passed after the final
test correction. clang-format22.1.8 checks on changed C++ files and git diff --check
passed.

Linux command:

```powershell
docker run --rm --pull never --network none --mount 'type=bind,source=C:/Users/geral/personal-quant-lab,target=/workspace,readonly' --entrypoint sh personal-quant-lab-persistence_tests /workspace/test/offline/run.sh
```

The first GCC14.2 build found a dangling-else warning from an unbraced GoogleTest
assertion, treated as an error. Explicit braces fixed the test; production code
compiled in that first run. The final GCC14.2 C++20 Debug build passed; all12 suites
passed with networking disabled. VALIDATION PASSED on Windows and Linux.

Remaining validation debt: CI, sanitizers and optional PostgreSQL/provider suites
were not rerun. No allocation-failure injection. Setup is in-memory; persisted
paper/control associations and IDs remain future application work. Raw-price
valuation excludes dividends/corporate actions and is not total-return SPY.
