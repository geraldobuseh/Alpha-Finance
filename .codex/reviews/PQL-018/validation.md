# PQL-018: Validation Report

VALIDATION PASSED. Validator static review found no blockers. Root executed the
builds/tests below; note spacing observations were corrected.

All acceptance criteria pass: commission and bps are configurable, fills differ
from reference quotes, and resulting cash/basis/P&L incorporate both frictions.
Six new tests bring FakeBroker to16 fixture tests, including invalid configuration,
zero-cost compatibility, deterministic runs, replay and unchanged rejected state.

Windows: initialized VS2022 x64 environment, then executed:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC C++20 Debug build passed; all10 default CTest suites passed.

Linux:

```powershell
docker run --rm --pull never --network none --mount 'type=bind,source=C:/Users/geral/personal-quant-lab,target=/workspace,readonly' --entrypoint sh personal-quant-lab-persistence_tests /workspace/test/offline/run.sh
```

GCC14.2 C++20 Debug build passed; all10 suites passed with networking disabled.
Offline runner disables sanitizers and optional persistence/provider targets.

Remaining debt: CI and optional database/provider suites were not rerun; no
allocation-failure injection. Fixed-bps costs are synthetic assumptions, with no
liquidity calibration, tick rounding or order-book validation. Experiment-level
configuration persistence and reference-price attribution are outside this scope.
