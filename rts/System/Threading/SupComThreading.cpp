/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComThreading.h"

#include "System/Config/ConfigHandler.h"

CONFIG(bool, PathCacheMainThreadOnly).defaultValue(true).description("supcom: only the main thread outside parallel sections adds to the shared path cache, and parallel sections restore the thread's previous section flag (1, same results at any thread count); stock lets worker threads add to it concurrently (0).");

CONFIG(bool, PathRequestsSerial).defaultValue(true).description("supcom: the HAPFS path requests of a frame run serially on the main thread (1, the same result on any machine and load); stock runs them on worker threads (0).");

namespace SupComThreading {
	bool PathRequestsSerial()
	{
		static int on = -1;
		if (on < 0) {
			if (configHandler == nullptr)
				return true;
			on = configHandler->GetBool("PathRequestsSerial") ? 1 : 0;
		}
		return (on != 0);
	}

	bool PathCacheMainThreadOnly()
	{
		// every thread reads the same value, so the race on the first read is benign
		if (pathCacheMainThreadOnly.load(std::memory_order_relaxed) < 0) {
			if (configHandler == nullptr)
				return true;
			pathCacheMainThreadOnly.store(configHandler->GetBool("PathCacheMainThreadOnly")? 1: 0, std::memory_order_relaxed);
		}
		return PathCacheMainThreadOnlyCached();
	}
}
