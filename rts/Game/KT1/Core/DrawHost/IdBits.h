/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw host (Core, no engine include): a fixed-size set of ids kept as one bit each, so a set of unit ids is walked
// in ascending order and compared without sorting or allocating.

#ifndef KT1_DRAWHOST_IDBITS_H
#define KT1_DRAWHOST_IDBITS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace KT1 {

class IdBits {
public:
	// the ids are 0 .. capacity - 1; allocates, so call once per game
	void Init(int capacity) {
		cap = std::max(capacity, 0);
		words.assign(static_cast<size_t>(cap + 63) / 64, 0u);
		count = 0;
	}

	int Capacity() const { return cap; }
	int Count() const { return count; }
	bool InRange(int id) const { return id >= 0 && id < cap; }

	bool Test(int id) const { return InRange(id) && ((words[id >> 6] >> (id & 63)) & 1u) != 0; }

	// true when the id was not in the set before; ids out of range are ignored
	bool Set(int id) {
		if (!InRange(id) || Test(id)) return false;
		words[id >> 6] |= uint64_t(1) << (id & 63);
		count++;
		return true;
	}

	void Clear() {
		std::fill(words.begin(), words.end(), uint64_t(0));
		count = 0;
	}

	// fn(id) for every id in the set, ascending
	template<typename Fn> void ForEach(Fn&& fn) const {
		for (size_t w = 0; w < words.size(); w++) {
			uint64_t bits = words[w];
			for (int b = 0; bits != 0; b++, bits >>= 1) {
				if ((bits & 1u) != 0) fn(static_cast<int>(w * 64) + b);
			}
		}
	}

	// the set as an ascending list; no allocation once `out` has held Capacity() ids
	void AppendTo(std::vector<int>& out) const { ForEach([&out](int id) { out.push_back(id); }); }

	bool operator==(const IdBits& o) const { return count == o.count && words == o.words; }

	void Swap(IdBits& o) {
		words.swap(o.words);
		std::swap(cap, o.cap);
		std::swap(count, o.count);
	}

private:
	std::vector<uint64_t> words;
	int cap = 0;
	int count = 0;
};

} // namespace KT1

#endif
