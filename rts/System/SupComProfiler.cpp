/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComProfiler.h"

#include "System/Config/ConfigHandler.h"

CONFIG(bool, SupComProfiler).defaultValue(false).description("supcom: 1 keeps the time profiler recording all timers (Draw::*, Sim::*, Lua) without the /debug overlay, for Spring.GetProfilerTimeRecord; 0 is stock (only special timers record until /debug).");

namespace {
	// read once the config exists; until then the profiler stays as stock
	int ReadConfig() { return (configHandler == nullptr)? -1: (configHandler->GetBool("SupComProfiler")? 1: 0); }

	struct Register { Register() { SupComProfiler::reader = &ReadConfig; } } registerReader;
}
