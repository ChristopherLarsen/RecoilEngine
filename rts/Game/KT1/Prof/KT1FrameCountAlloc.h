/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FrameCount, allocation part (NEW-ENGINE-PLAN r6 Phase 0 item 5; game-build docs/engine/FRAMECOUNT.md).
// Header-only so System/MemoryOverride.cpp (one hook line in each counted operator new, "// KT1 hook: FrameCount") needs no
// Game library. Off (the default): one relaxed load of On() per allocation. On: one relaxed add into this thread's slot, so
// threads do not share a counter cache line. Never touches sim state; the totals are read once per drawn frame (KT1FrameCount.cpp).

#ifndef KT1_FRAMECOUNT_ALLOC_H
#define KT1_FRAMECOUNT_ALLOC_H

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace KT1FrameCountAlloc {
	static constexpr unsigned SLOTS = 64;   // threads beyond 64 share slots (still exact: the adds are atomic)
	struct alignas(64) Slot { std::atomic<uint64_t> n{0}; std::atomic<uint64_t> bytes{0}; };

	inline std::atomic<bool> on{false};
	inline std::atomic<unsigned> nextSlot{0};
	inline Slot slots[SLOTS];

	inline void Count(std::size_t size) {
		if (!on.load(std::memory_order_relaxed)) return;
		thread_local unsigned mine = SLOTS;   // constant-initialised: safe inside operator new
		if (mine == SLOTS) mine = nextSlot.fetch_add(1, std::memory_order_relaxed) % SLOTS;
		slots[mine].n.fetch_add(1, std::memory_order_relaxed);
		slots[mine].bytes.fetch_add(size, std::memory_order_relaxed);
	}

	// running totals since the engine started (all threads)
	inline void Totals(uint64_t& n, uint64_t& bytes) {
		n = 0; bytes = 0;
		for (const Slot& s: slots) { n += s.n.load(std::memory_order_relaxed); bytes += s.bytes.load(std::memory_order_relaxed); }
	}
}

#endif
