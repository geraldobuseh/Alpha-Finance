# PQL-021: Validation Report

VALIDATION PASSED. Validator static review found no blockers; root executed builds.
The portfolio-wide same-day SPY suppression convention is explicit in documentation.

Seven DCA tests cover the fixed Monday rule, costs, existing holdings, restart/retry,
ID gaps/exhaustion, UTC and pre-epoch boundaries, future history, replay and a40-week
prefunded simulation against an unchanged control. Existing benchmark tests also
pass after shared sizing extraction.

Windows commands in the VS2022 x64 developer environment:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC C++20 Debug build and all13 default CTest suites passed.

Linux command:

```powershell
docker run --rm --pull never --network none --mount 'type=bind,source=C:/Users/geral/personal-quant-lab,target=/workspace,readonly' --entrypoint sh personal-quant-lab-persistence_tests /workspace/test/offline/run.sh
```

GCC14.2 C++20 Debug build and all13 suites passed without networking. The offline
runner disables sanitizers and optional persistence/provider targets. Final changes
after these checks only clarified comments/documentation about execution assumptions.

Remaining debt: CI and optional PostgreSQL/provider suites were not rerun. There is
no external contribution, exchange calendar, concurrent ID allocator or delayed
execution budget guarantee. Tests use the same quote/costs for sizing and immediate
execution except the explicit broker rejection fixture.
