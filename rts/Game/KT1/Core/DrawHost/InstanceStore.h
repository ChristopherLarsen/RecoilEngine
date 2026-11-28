/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw host (Core, no engine include): the CPU side of Draw/Common/InstanceBatch. One instance per id, kept packed
// (Core/Common's DenseIndex and Column: a removed one is replaced by the last), with the bookkeeping that says which
// slots each GPU buffer region still lacks.
//
// A persistently mapped buffer has several regions used in turn, so that the CPU never writes the one the GPU reads.
// A change must therefore reach every region, each at its next turn: every slot keeps one "stale" bit per region.
//
// T is copied and compared as bytes: it must be trivially copyable and have no padding (or be zeroed before it is filled).

#ifndef KT1_DRAWHOST_INSTANCESTORE_H
#define KT1_DRAWHOST_INSTANCESTORE_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "../Common/Column.h"
#include "../Common/DenseIndex.h"

namespace KT1 {

struct SlotRange {
	int first = 0;
	int count = 0;
};

template<typename T>
class InstanceStore {
	static_assert(std::is_trivially_copyable<T>::value, "an instance is uploaded and compared as bytes");

public:
	static constexpr int MAX_REGIONS = 8;   // bits of a stale mask

	// ids 0 .. idCapacity - 1, so at most idCapacity instances; every allocation happens here
	void Init(int idCapacity, int numRegions) {
		regions = std::min(std::max(numRegions, 1), MAX_REGIONS);
		index.Init(idCapacity);
		data.Init(index);
		stale.Init(index);
		for (Bounds& b : bounds) b = Bounds{};
	}

	int Capacity() const { return index.Capacity(); }
	int Regions() const { return regions; }
	int Size() const { return index.Size(); }                       // instances to draw
	const T* Data() const { return data.Live(index).data(); }       // Size() of them
	int IdAt(int slot) const { return index.IdOf(slot); }
	bool Has(int id) const { return index.Has(id); }

	// adds or changes; a value equal to the one held marks nothing, so an unchanged instance is never uploaded again
	void Set(int id, const T& value) {   // KT1 hot
		int slot = index.SlotOf(id);
		if (slot != Core::DenseIndex::NONE && std::memcmp(&data[slot], &value, sizeof(T)) == 0) return;
		if (slot == Core::DenseIndex::NONE) slot = index.Add(id);
		if (slot == Core::DenseIndex::NONE) return;                  // id out of range
		std::memcpy(&data[slot], &value, sizeof(T));
		Mark(slot);
	}

	void Remove(int id) {
		const Core::DenseIndex::Move move = index.Remove(id);
		if (move.from == Core::DenseIndex::NONE) return;
		data.Apply(move);
		stale.Apply(move);
		if (move.from != move.to) Mark(move.to);                     // the last instance now lives in the hole
	}

	// removes every instance whose id `keep(id)` rejects
	template<typename Keep> void RemoveUnless(Keep&& keep) {
		// downwards: Remove fills a hole from the end, which has been looked at already
		for (int slot = Size() - 1; slot >= 0; slot--) {
			const int id = index.IdOf(slot);
			if (!keep(id)) Remove(id);
		}
	}

	// the GPU buffer was made anew: every region needs everything
	void MarkAll() {
		for (int slot = 0; slot < Size(); slot++) Mark(slot);
	}

	// The slot ranges region `region` has to copy before it is drawn, ascending, and marks them as held by it. Ranges
	// separated by at most `mergeGap` current slots are joined (fewer, longer copies). `out` is cleared first.
	void TakeStale(int region, int mergeGap, std::vector<SlotRange>& out) {   // KT1 hot
		out.clear();
		if (region < 0 || region >= regions) return;
		Bounds& b = bounds[region];
		const uint8_t bit = static_cast<uint8_t>(1u << region);
		const int end = std::min(b.hi + 1, Size());
		for (int slot = b.lo; slot < end; slot++) {
			if ((stale[slot] & bit) == 0) continue;
			stale[slot] = static_cast<uint8_t>(stale[slot] & ~bit);
			if (!out.empty() && slot - (out.back().first + out.back().count) <= mergeGap) {
				out.back().count = slot - out.back().first + 1;
			} else {
				out.push_back({slot, 1});
			}
		}
		b = Bounds{};
	}

private:
	struct Bounds {
		int lo = INT32_MAX;
		int hi = -1;
	};

	void Mark(int slot) {
		stale[slot] = static_cast<uint8_t>((1u << regions) - 1u);
		for (int r = 0; r < regions; r++) {
			bounds[r].lo = std::min(bounds[r].lo, slot);
			bounds[r].hi = std::max(bounds[r].hi, slot);
		}
	}

	Core::DenseIndex index;
	Core::Column<T> data;
	Core::Column<uint8_t> stale;
	Bounds bounds[MAX_REGIONS];
	int regions = 1;
};

} // namespace KT1

#endif
