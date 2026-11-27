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

	// supcom: springsetting PathRequestsSerial (default 1; 0 is stock). On, the HAPFS path requests of a frame run one
	// after another on the main thread instead of in a for_mt. Stock hands them to worker threads by timing, and each
	// thread has its own path finders: which searches shared a finder (and so its leftover state) depended on load and on
	// the core count, and replays and multiplayer clients diverged (desync 11460, bisected 2026-11-22). The searches run
	// inside a parallel-section flag either way, so the shared path cache gets no adds from them in both modes.
	bool PathRequestsSerial();

	// supcom (PERF-REVIEW-F P1b, 2026-11-27): mod option "pathlanes" (SupComSimSwitch, default on; "0": serial as before). On, a
	// frame's serial path request batch of PATH_LANES or more requests runs in PATH_LANES lanes on the thread pool: lane k takes
	// the k-th contiguous slice of the batch (in batch order) and runs it in order with finder group k, its own path finders, and
	// its own batch cache (SupComPathBatch). What a lane computes then depends only on its slice and on group k's state, never on
	// which thread ran it or on the core count (stock's threaded requests picked finders by thread: desync 11460). The engine keeps
	// at least PATH_LANES finder groups on any machine. Smaller batches run serially in lane 0 (group 0), as before.
	static constexpr int PATH_LANES = 8;
	bool PathLanes();
	// the lane this thread is running (-1: none)
	inline thread_local int pathLane = -1;
}

#endif
