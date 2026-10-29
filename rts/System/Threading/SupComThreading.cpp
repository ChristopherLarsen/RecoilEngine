/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComThreading.h"

#include "System/Config/ConfigHandler.h"

CONFIG(bool, PathCacheMainThreadOnly).defaultValue(true).description("supcom: only the main thread outside parallel sections adds to the shared path cache, and parallel sections restore the thread's previous section flag (1, same results at any thread count); stock lets worker threads add to it concurrently (0).");

namespace SupComThreading {
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
