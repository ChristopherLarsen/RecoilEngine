/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_PROFILER_H
#define SUPCOM_PROFILER_H

// supcom: springsetting SupComProfiler (default 0, stock). 1 keeps the engine time profiler recording every timer (the
// Draw::*, Sim::* and Lua sub-timers read by Spring.GetProfilerTimeRecord) without the /debug overlay, whose own drawing
// would distort a draw-cost measurement. Diagnosis only: timers do not touch the simulation.
// Header-only for the profiler: SupComProfiler.cpp registers the config reader, so code linked without it (unit tests)
// reads "off".
namespace SupComProfiler {
	inline bool (*reader)() = nullptr;
	inline int state = -1;

	inline bool On()
	{
		if (state < 0) {
			if (reader == nullptr)
				return false;
			const int s = reader()? 1: 0;
			if (s < 0)
				return false;
			state = s;
		}
		return (state != 0);
	}
}

#endif
