/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: SortedIdList - a set of ids kept in ascending order in one flat array.
// Why: small id sets (a group's members, a factory's queue owners, units in a zone) that synced code walks in id order
//   (LUA-FREE-PLAN §2.2 p5) without a node-based std::set. Insert and Erase are O(n) moves, Contains is O(log n).
//   For whole-army sets use DenseIndex instead.
// Steady state: no allocation while Size() stays within the capacity given to Init (past it the array grows once).
// No engine include.

#ifndef KT1_CORE_COMMON_SORTED_ID_LIST_H
#define KT1_CORE_COMMON_SORTED_ID_LIST_H

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Core {

class SortedIdList {
public:
	using Id = int32_t;

	void Init(int32_t capacity) {
		ids.clear();
		ids.reserve(static_cast<size_t>(std::max<int32_t>(0, capacity)));
	}

	void Clear() { ids.clear(); }
	int32_t Size() const { return static_cast<int32_t>(ids.size()); }
	bool Empty() const { return ids.empty(); }

	bool Contains(Id id) const { return std::binary_search(ids.begin(), ids.end(), id); }

	// false if already present
	bool Insert(Id id) {
		const auto it = std::lower_bound(ids.begin(), ids.end(), id);
		if (it != ids.end() && *it == id)
			return false;
		ids.insert(it, id);
		return true;
	}

	// false if absent
	bool Erase(Id id) {
		const auto it = std::lower_bound(ids.begin(), ids.end(), id);
		if (it == ids.end() || *it != id)
			return false;
		ids.erase(it);
		return true;
	}

	// ascending
	std::span<const Id> Ids() const { return {ids.data(), ids.size()}; }
	auto begin() const { return ids.begin(); }
	auto end() const { return ids.end(); }

private:
	std::vector<Id> ids;
};

} // namespace KT1::Core

#endif
