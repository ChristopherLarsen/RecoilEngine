# KT1 Core/Common

Engine-free building blocks every KT1 system uses (LUA-FREE-PLAN §2.2, §2.3; work package F02). Header-only, standard
library only: no engine include anywhere in `Core/`. Each header's top comment says what it is for, its order rules
and its allocation rule.

| Header | What |
|---|---|
| `Rate.h` | seconds to sim steps from the one sim rate: `Rate::Every(s)` (period, at least 1), `Rate::Steps(s)` (delay, at least 0) |
| `DenseIndex.h` | unit id to dense slot and back, O(1) add and swap-remove, `AscendingIds()` for synced iteration |
| `Column.h` | one structure-of-arrays column indexed by slot; `Apply(move)` mirrors each remove |
| `SortedIdList.h` | a small id set in ascending order in one flat array |
| `EventBuffer.h` | one step's events in push order; `Flip()` hands them to the next step |
| `DueWheel.h` | ids by due step; `Take()` once per step returns that step's ids ascending and unique |
| `DetMath.h` | the Lua's number maths in float (this engine's `LUA_NUMBER` is float, luaconf.h) and in the Lua's evaluation order (lerp, clamp, Lua 5.1 mod, floor(x + 0.5), dot, dist, sum) |
| `FixedPool.h` | a fixed number of T by integer handle, last released reused first |

Steady state allocates nothing: storage is sized at `Init`. The tests count every `operator new` to prove it.

Tests: `../../Test/Common/`, run by game-build `tools/engine/core_tests.py` (engine-free). Every suite runs at 30
and at 10 steps per second.
