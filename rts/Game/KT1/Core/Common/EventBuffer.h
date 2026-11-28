/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: EventBuffer<T> - the events of one sim step, appended in order and read back in the same order.
// Why: systems run once per frame phase over a batch of this step's events (LUA-FREE-PLAN §2.2 p2), not per event.
// Two ways to use it:
//   - same step: producers Push during the step, a later phase reads Pending(), the host calls Flip() at step end;
//   - next step: after Flip() the step's events are Ready() for the whole next step, while new ones go to Pending().
//   Flip swaps two arrays, so neither allocates.
// Order: Ready() and Pending() are in Push order. Synced producers must push in a fixed order (ascending unit id).
// Steady state: no allocation while a step's events stay within the capacity given to Init; HighWater() records the
//   largest step, so the owning system can size its config. No engine include.

#ifndef KT1_CORE_COMMON_EVENT_BUFFER_H
#define KT1_CORE_COMMON_EVENT_BUFFER_H

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace KT1::Core {

template <typename T>
class EventBuffer {
public:
	void Init(int32_t capacity) {
		const size_t n = static_cast<size_t>(std::max<int32_t>(0, capacity));
		pending.clear();
		ready.clear();
		pending.reserve(n);
		ready.reserve(n);
		highWater = 0;
	}

	void Push(const T& event) { pending.push_back(event); }

	template <typename... Args>
	T& Emplace(Args&&... args) { return pending.emplace_back(std::forward<Args>(args)...); }

	// this step's events so far, in Push order
	std::span<const T> Pending() const { return {pending.data(), pending.size()}; }

	// the previous step's events, in Push order (empty before the first Flip)
	std::span<const T> Ready() const { return {ready.data(), ready.size()}; }

	// at step end: this step's events become Ready(), Pending() starts empty; the old Ready() is dropped
	void Flip() {
		highWater = std::max(highWater, static_cast<int32_t>(pending.size()));
		ready.swap(pending);
		pending.clear();
	}

	void Clear() {
		pending.clear();
		ready.clear();
	}

	int32_t HighWater() const { return highWater; }

private:
	std::vector<T> pending;
	std::vector<T> ready;
	int32_t highWater = 0;
};

} // namespace KT1::Core

#endif
