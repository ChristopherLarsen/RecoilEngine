/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComProfiler.h"

#include "System/Config/ConfigHandler.h"

CONFIG(bool, SupComProfiler).defaultValue(false).description("supcom: 1 keeps the time profiler recording all timers (Draw::*, Sim::*, Lua) without the /debug overlay, for Spring.GetProfilerTimeRecord; 0 is stock (only special timers record until /debug).");

namespace SupComProfiler {
	bool On()
	{
		static int on = -1;
		if (on < 0) {
			if (configHandler == nullptr)
				return false;
			on = configHandler->GetBool("SupComProfiler")? 1: 0;
		}
		return (on != 0);
	}
}
