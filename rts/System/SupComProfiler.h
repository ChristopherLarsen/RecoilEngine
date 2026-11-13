/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_PROFILER_H
#define SUPCOM_PROFILER_H

// supcom: springsetting SupComProfiler (default 0, stock). 1 keeps the engine time profiler recording every timer (the
// Draw::*, Sim::* and Lua sub-timers read by Spring.GetProfilerTimeRecord) without the /debug overlay, whose own drawing
// would distort a draw-cost measurement. Diagnosis only: timers do not touch the simulation.
namespace SupComProfiler {
	bool On();
}

#endif
