# KT1 engine-free tests

Every Core library is tested in one binary, `kt1_core_tests`, built without an engine build (LUA-FREE-WORKPACKAGES F03).
In game-build:

    python -I tools/engine/core_tests.py --tree rc-<yours> [--only SYS-] [--clean]

`--tree` is your fork tree, from the WSL home, uncommitted edits included. `--help` lists the other sources.

## Adding tests

- A test file is `Test/<System>/<Name>Test.cpp`. The driver finds it: no list, no CMake line.
- Cases: `KT1_CASE("SYS-ID", "what must hold")` and `KT1_RATE_CASE(...)`; checks: `KT1_CHECK`, `KT1_CHECK_EQ`,
  `KT1_CHECK_MSG` (`KT1Test.h`). Ids are unique across all systems.
- A rate case runs at 30 and at 10 steps per second with `KT1::Core::Rate` already set; `testCase.StepsPerSecond()`
  is the pass's rate. In a plain case the rate is unset, so a conversion there asserts.
- Cases as data: JSON files in game-build `tools/test/core_cases/<system>/` and one `KT1_JSON_CASES("<system>")`
  handler (`KT1CaseLoader.h`).
- No `main()`, no own `operator new` (use `KT1::Test::AllocationCount()`), no `test/` folder under `Core/`.
- Cases run in id order and must not depend on each other. One that fails, throws, asserts, crashes or hangs (60 s)
  fails alone.

## The compiler

The engine's own Windows compiler, with the engine's floating-point flags. `-fsingle-precision-constant` is one of
them: **a bare literal such as `0.1` or `1.0` is a float**, in Core code too. Where double precision matters, build the
value from integers (`double(1) / 10`). Tests keep `assert` on (no `NDEBUG`).
