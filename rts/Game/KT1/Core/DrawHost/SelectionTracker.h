/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw host (Core, no engine include): the selection as an ascending id list with a change counter, so a draw system
// rebuilds its data only when the counter moved.

#ifndef KT1_DRAWHOST_SELECTIONTRACKER_H
#define KT1_DRAWHOST_SELECTIONTRACKER_H

#include <cstdint>
#include <vector>

#include "IdBits.h"

namespace KT1 {

class SelectionTracker {
public:
	void Init(int capacity) {
		bits.Init(capacity);
		next.Init(capacity);
		ids.clear();
		ids.reserve(capacity);
		changes = 0;
	}

	// `selected`: any range of unit ids, in any order. True when the set differs from the last call's.
	template<typename Range> bool Update(const Range& selected) {
		next.Clear();
		for (const int id : selected) next.Set(id);
		if (next == bits) return false;
		bits.Swap(next);
		ids.clear();
		bits.AppendTo(ids);
		changes++;
		return true;
	}

	bool Contains(int id) const { return bits.Test(id); }
	const std::vector<int>& Ids() const { return ids; }   // ascending
	uint32_t Changes() const { return changes; }          // starts at 0 (empty); +1 per differing set

private:
	IdBits bits, next;
	std::vector<int> ids;
	uint32_t changes = 0;
};

} // namespace KT1

#endif
