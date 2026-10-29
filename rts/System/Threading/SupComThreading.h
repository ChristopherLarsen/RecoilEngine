/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_THREADING_H
#define SUPCOM_THREADING_H

#include <atomic>

// supcom: springsetting PathCacheMainThreadOnly (default 1; 0 is stock). On, only the main thread outside parallel
// sections adds paths to the shared HAPFS path cache (IPathFinder::GetPath), and a parallel section restores the thread's
// previous "in a parallel section" flag when it ends instead of clearing it (MultithreadedSection). Stock lets worker
// threads add to the cache concurrently (a data race; which search cached what depended on thread count and scheduling,
// so same-seed games diverged), and a nested section on a worker cleared the flag for the rest of the thread's life.
namespace SupComThreading {
	// the setting as last read (-1: not read yet, counts as on); header-only so that code without the config
	// (the thread pool's unit test) links
	inline std::atomic<int> pathCacheMainThreadOnly{-1};

	inline bool PathCacheMainThreadOnlyCached() { return (pathCacheMainThreadOnly.load(std::memory_order_relaxed) != 0); }

	/// reads the springsetting (once the config exists) into pathCacheMainThreadOnly and returns it
	bool PathCacheMainThreadOnly();
}

#endif
