/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: DenseIndex - maps a unit id (0 .. maxIds-1) to a dense slot (0 .. Size()-1) and back.
// Why: per-unit state lives in structure-of-arrays columns (Column<T>) indexed by slot, so a hot loop walks
//   slots 0 .. Size()-1 with no gaps (LUA-FREE-PLAN §2.2 p1).
// Add and Remove are O(1). Remove fills the hole with the last slot (swap-remove) and returns that Move; every column
//   of the system applies the same Move (Column<T>::Apply). So slot order is NOT id order.
// Synced code that reaches a result in unit order (§2.2 p5) walks AscendingIds(): the ids sorted on demand into a buffer
//   reserved at Init. It is sorted again only after an Add or Remove.
// Steady state: no allocation (all storage is sized by Init). No engine include.

#ifndef KT1_CORE_COMMON_DENSE_INDEX_H
#define KT1_CORE_COMMON_DENSE_INDEX_H

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Core {

class DenseIndex {
public:
	using Id = int32_t;
	using Slot = int32_t;
	static constexpr Slot NONE = -1;

	// what a Remove did: the data in slot `from` now belongs in slot `to`, and slot `from` is no longer used.
	// from == to when the removed id held the last slot; from == NONE when nothing was removed.
	struct Move {
		Slot from = NONE;
		Slot to = NONE;
	};

	// ids are 0 .. maxIds-1 (the engine's unit limit); allocates once
	void Init(int32_t maxIds) {
		slotOf.assign(static_cast<size_t>(std::max<int32_t>(0, maxIds)), NONE);
		idOf.clear();
		idOf.reserve(slotOf.size());
		sorted.clear();
		sorted.reserve(slotOf.size());
		sortedValid = true;
	}

	void Clear() {
		for (const Id id: idOf)
			slotOf[id] = NONE;
		idOf.clear();
		sorted.clear();
		sortedValid = true;
	}

	int32_t Size() const { return static_cast<int32_t>(idOf.size()); }
	int32_t Capacity() const { return static_cast<int32_t>(slotOf.size()); }
	bool InRange(Id id) const { return id >= 0 && id < Capacity(); }
	bool Has(Id id) const { return InRange(id) && slotOf[id] != NONE; }
	Slot SlotOf(Id id) const { return InRange(id) ? slotOf[id] : NONE; }
	Id IdOf(Slot slot) const { return idOf[slot]; }

	// the new slot (the old Size()); the slot it already has if present; NONE if the id is out of range
	Slot Add(Id id) {
		if (!InRange(id))
			return NONE;
		if (slotOf[id] != NONE)
			return slotOf[id];
		const Slot slot = Size();
		idOf.push_back(id);
		slotOf[id] = slot;
		sortedValid = false;
		return slot;
	}

	// removes the id; apply the returned Move to every column of the system
	Move Remove(Id id) {
		Move move;
		if (!Has(id))
			return move;
		const Slot hole = slotOf[id];
		const Slot last = Size() - 1;
		const Id lastId = idOf[last];
		idOf[hole] = lastId;
		slotOf[lastId] = hole;
		slotOf[id] = NONE;   // after the line above: when id is lastId it ends up NONE
		idOf.pop_back();
		sortedValid = false;
		move.from = last;
		move.to = hole;
		return move;
	}

	// the ids in slot order (the order a hot loop over columns sees)
	std::span<const Id> SlotIds() const { return {idOf.data(), idOf.size()}; }

	// the ids in ascending order, for synced iteration that reaches a result; valid until the next Add or Remove
	std::span<const Id> AscendingIds() {
		if (!sortedValid) {
			sorted.assign(idOf.begin(), idOf.end());   // within the capacity reserved at Init
			std::sort(sorted.begin(), sorted.end());   // introsort: no allocation
			sortedValid = true;
		}
		return {sorted.data(), sorted.size()};
	}

private:
	std::vector<Slot> slotOf;   // by id
	std::vector<Id> idOf;       // by slot
	std::vector<Id> sorted;     // AscendingIds' buffer
	bool sortedValid = true;
};

} // namespace KT1::Core

#endif
