/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the draw side's end of the sim's display events (Host/DisplayEvents.h, F05). The sim publishes each step's
// sounds, effects, log lines, debug lines and pings; the draw host plays them once per drawn frame, in emit order,
// before any draw system's Update (game-build docs/engine/LUA-FREE-PLAN.md 2.2 principle 7).
// In an engine without DisplayEvents both calls do nothing.

#ifndef KT1_HOST_DISPLAYSINK_H
#define KT1_HOST_DISPLAYSINK_H

namespace KT1 {
namespace DisplaySink {

bool Present();   // the engine has a display event queue to drain
void Replay();    // plays and forgets everything published since the last call

} // namespace DisplaySink
} // namespace KT1

#endif
