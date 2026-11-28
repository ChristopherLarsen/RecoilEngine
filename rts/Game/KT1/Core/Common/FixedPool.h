/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: FixedPool<T> - a fixed number of T, handed out and returned by integer handle.
// Why: short-lived per-event records (a pending shot, a staging slot, a ribbon) without heap traffic in a step, and
//   with handles that survive a copy of the pool, unlike pointers.
// Order: handles are reused last-released first; a fresh pool hands out 0, 1, 2, ... So two clients that acquire and
//   release in the same order get the same handles (synced use needs that order fixed, §2.2 p5).
//   ForEachLive visits live handles in ascending order.
// Steady state: no allocation (all storage is sized by Init; T should not own heap memory). Acquire returns NONE when
//   the pool is full: the owning system decides what to drop and logs it. No engine include.

#ifndef KT1_CORE_COMMON_FIXED_POOL_H
#define KT1_CORE_COMMON_FIXED_POOL_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace KT1::Core {

template <typename T>
class FixedPool {
public:
	using Handle = int32_t;
	static constexpr Handle NONE = -1;

	void Init(int32_t capacity) {
		const size_t n = static_cast<size_t>(std::max<int32_t>(0, capacity));
		items.assign(n, T{});
		live.assign(n, 0);
		freeList.clear();
		freeList.reserve(n);
		for (size_t i = n; i-- > 0;)
			freeList.push_back(static_cast<Handle>(i));   // back() is 0: a fresh pool hands out 0 first
		count = 0;
	}

	// a blank T's handle, or NONE when full
	Handle Acquire() {
		if (freeList.empty())
			return NONE;
		const Handle h = freeList.back();
		freeList.pop_back();
		items[h] = T{};
		live[h] = 1;
		++count;
		return h;
	}

	// false if the handle was not live
	bool Release(Handle h) {
		if (!Live(h))
			return false;
		live[h] = 0;
		freeList.push_back(h);
		--count;
		return true;
	}

	bool Live(Handle h) const { return h >= 0 && h < Capacity() && live[h] != 0; }
	T& operator[](Handle h) { return items[h]; }
	const T& operator[](Handle h) const { return items[h]; }

	int32_t Size() const { return count; }
	int32_t Capacity() const { return static_cast<int32_t>(items.size()); }

	// f(handle, item) for every live handle, ascending
	template <typename F>
	void ForEachLive(F&& f) {
		for (Handle h = 0; h < Capacity(); ++h)
			if (live[h] != 0)
				f(h, items[h]);
	}

private:
	std::vector<T> items;
	std::vector<uint8_t> live;
	std::vector<Handle> freeList;
	int32_t count = 0;
};

} // namespace KT1::Core

#endif
