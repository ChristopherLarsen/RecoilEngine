/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: DueWheel - ids waiting for a due step, handed out one step at a time in ascending id order.
// Why: a system visits only the ids due this step instead of scanning the whole army (LUA-FREE-PLAN §2.2 p2). A unit
//   with a 0.5 s period is rescheduled at step + Rate::Every(0.5) each time it is taken.
// How: a ring of `buckets` arrays covers the next `buckets` steps; a due step further out waits in an overflow heap and
//   moves into its bucket as the wheel turns. Take() returns the next step's ids sorted and without duplicates (§2.2 p5).
// Rules: call Take() exactly once per sim step, from the step after Init's startStep on. Schedule a due step at or before
//   the last taken step and it is due on the next one. Cancel is O(bucket); a system whose units die often may instead
//   check the id on Take.
// Steady state: no allocation while a bucket stays within perBucket and the overflow within overflow (Init); past that
//   the array grows once. No engine include.

#ifndef KT1_CORE_COMMON_DUE_WHEEL_H
#define KT1_CORE_COMMON_DUE_WHEEL_H

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace KT1::Core {

class DueWheel {
public:
	using Id = int32_t;
	using Step = int32_t;

	// startStep: the step before the first Take (the first Take returns startStep + 1)
	void Init(int32_t buckets, int32_t perBucket, int32_t overflow, Step startStep = -1) {
		ring.assign(static_cast<size_t>(std::max<int32_t>(1, buckets)), {});
		for (std::vector<Id>& b: ring)
			b.reserve(static_cast<size_t>(std::max<int32_t>(0, perBucket)));
		current.clear();
		current.reserve(static_cast<size_t>(std::max<int32_t>(0, perBucket)));
		far.clear();
		far.reserve(static_cast<size_t>(std::max<int32_t>(0, overflow)));
		now = startStep;
	}

	// the last step taken
	Step Now() const { return now; }

	void Schedule(Id id, Step due) {
		due = std::max(due, now + 1);
		if (due - now <= Span())
			ring[Bucket(due)].push_back(id);
		else
			PushFar(due, id);
	}

	// removes every entry of id due at `due`; false if there was none
	bool Cancel(Id id, Step due) {
		due = std::max(due, now + 1);
		if (due - now <= Span()) {
			std::vector<Id>& b = ring[Bucket(due)];
			const auto end = std::remove(b.begin(), b.end(), id);
			const bool found = end != b.end();
			b.erase(end, b.end());
			return found;
		}
		const auto end = std::remove(far.begin(), far.end(), Entry{due, id});
		const bool found = end != far.end();
		far.erase(end, far.end());
		std::make_heap(far.begin(), far.end(), Later);
		return found;
	}

	// advances one step; the ids due on it, ascending and unique, valid until the next Take
	std::span<const Id> Take() {
		++now;
		current.clear();
		current.swap(ring[Bucket(now)]);   // the bucket keeps current's old array: no allocation either way
		// the bucket just emptied now stands for step now + Span(): bring in the far entries that reached the ring
		while (!far.empty() && far.front().first - now <= Span()) {
			const Entry e = far.front();
			std::pop_heap(far.begin(), far.end(), Later);
			far.pop_back();
			if (e.first <= now)
				current.push_back(e.second);
			else
				ring[Bucket(e.first)].push_back(e.second);
		}
		std::sort(current.begin(), current.end());
		current.erase(std::unique(current.begin(), current.end()), current.end());
		return {current.data(), current.size()};
	}

	// entries waiting (duplicates counted)
	int32_t Waiting() const {
		size_t n = far.size();
		for (const std::vector<Id>& b: ring)
			n += b.size();
		return static_cast<int32_t>(n);
	}

private:
	using Entry = std::pair<Step, Id>;   // (due, id)

	// a max-heap on Later is a min-heap on (due, id)
	static bool Later(const Entry& a, const Entry& b) { return a > b; }

	int32_t Span() const { return static_cast<int32_t>(ring.size()); }
	size_t Bucket(Step step) const { return static_cast<size_t>(step % Span() + Span()) % ring.size(); }

	void PushFar(Step due, Id id) {
		far.emplace_back(due, id);
		std::push_heap(far.begin(), far.end(), Later);
	}

	std::vector<std::vector<Id>> ring;   // ring[step % size]: steps now+1 .. now+size
	std::vector<Id> current;             // Take's result
	std::vector<Entry> far;              // heap of entries beyond the ring
	Step now = -1;
};

} // namespace KT1::Core

#endif
