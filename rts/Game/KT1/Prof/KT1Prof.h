/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Prof: a cheaper engine profiler (perf_dbg / "debug 1"), game-build docs/engine/rules/INDEX.md, coordinator 2026-11-26.
// Unsynced, never touches sim state. Stock TimeProfiler.cpp calls it from two one-line hooks marked "// KT1 hook: Prof".
//   (a) thread timers (SCOPED_MT_TIMER) add into a per-thread slot table with relaxed atomics, no lock; merged once per Update
//   (b) no "Misc::Profiler::AddTime" self-timing on every call
//   (c) no per-call thread-graph samples (threadProfiles) in cheap mode
//   (d) ThreadPool::RunTask / AddTask / WaitFor timers dropped unless KT1ProfThreadPool=1
// Config (springsettings): KT1CheapProfiler (default 1; 0 = stock behaviour), KT1ProfThreadPool (default 0).

#ifndef KT1_PROF_H
#define KT1_PROF_H

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "System/StringHash.h"
#include "System/Threading/SpringThreading.h"
#include "System/Config/ConfigHandler.h"
#include "System/TimeProfiler.h"

namespace KT1Prof {
	struct Slot { std::atomic<uint32_t> key{0}; std::atomic<int64_t> ns{0}; };
	static constexpr unsigned SLOTS = 256;   // distinct thread-timer names per thread (open addressing; overflow falls back to stock)
	struct Table { std::array<Slot, SLOTS> s; };

	inline std::atomic<bool>& Cheap() { static std::atomic<bool> b{true}; return b; }
	inline std::atomic<bool>& PoolTimers() { static std::atomic<bool> b{false}; return b; }
	inline spring::mutex& TablesMutex() { static spring::mutex m; return m; }
	inline std::vector<Table*>& Tables() { static std::vector<Table*> v; return v; }   // never freed: threads outlive no table reader

	inline Table& Mine() {
		thread_local Table* t = nullptr;
		if (t == nullptr) {
			t = new Table();
			std::lock_guard<spring::mutex> lk(TablesMutex());
			Tables().push_back(t);
		}
		return *t;
	}

	// read the two keys once the config exists (called from CTimeProfiler::Update on the main thread)
	inline void ReadConfig() {
		static bool done = false;
		if (done || configHandler == nullptr) return;
		done = true;
		Cheap() = configHandler->GetInt("KT1CheapProfiler") != 0;
		PoolTimers() = configHandler->GetInt("KT1ProfThreadPool") != 0;
	}

	// true when handled here (the caller returns); false: the stock path runs
	inline bool AddTime(CTimeProfiler& tp, unsigned hash, spring_time dt, bool threadTimer, spring::mutex& profileMutex) {
		if (!Cheap().load(std::memory_order_relaxed)) return false;
		if (threadTimer) {
			static constexpr uint32_t RUN = hashString("ThreadPool::RunTask"), ADD = hashString("ThreadPool::AddTask"), WAIT = hashString("ThreadPool::WaitFor");
			if ((hash == RUN || hash == ADD || hash == WAIT) && !PoolTimers().load(std::memory_order_relaxed)) return true;
			Table& t = Mine();
			const uint32_t k = hash ? hash : 1u;
			for (unsigned i = 0, j = k % SLOTS; i < SLOTS; ++i, j = (j + 1) % SLOTS) {
				Slot& s = t.s[j];
				uint32_t cur = s.key.load(std::memory_order_relaxed);
				if (cur == 0) { s.key.store(k, std::memory_order_release); cur = k; }   // only this thread writes its own keys
				if (cur == k) { s.ns.fetch_add(dt.toNanoSecsi(), std::memory_order_relaxed); return true; }
			}
			return false;   // table full: stock path
		}
		std::lock_guard<spring::mutex> lk(profileMutex);   // main-thread timers: uncontended now that workers no longer take it
		tp.AddTimeRaw(hash, spring_notime, dt, false, false);
		return true;
	}

	// fold every thread's sums into the profile (caller holds profileMutex)
	inline void Merge(CTimeProfiler& tp) {
		if (!Cheap().load(std::memory_order_relaxed)) return;
		std::lock_guard<spring::mutex> lk(TablesMutex());
		for (Table* t: Tables()) {
			for (Slot& s: t->s) {
				const uint32_t k = s.key.load(std::memory_order_acquire);
				if (k == 0) continue;
				const int64_t ns = s.ns.exchange(0, std::memory_order_relaxed);
				if (ns != 0) tp.AddTimeRaw(k, spring_notime, spring_time::fromNanoSecs(ns), false, false);
			}
		}
	}
}

#endif
