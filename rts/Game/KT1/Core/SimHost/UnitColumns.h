/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: a system's per-unit state, so a removal reaches every column. No engine include.
//
// DenseIndex::Remove moves the last slot into the hole, and every column of the system must make the same move. Here the
// columns are registered once and RemoveUnit does it for all of them.
//
//   UnitColumns units;  Column<float> damage;
//   units.Init(maxUnits);  units.Own(damage);                at game setup
//   const auto slot = units.AddUnit(id);  damage[slot] += d;
//   units.RemoveUnit(id);                                     every owned column follows

#ifndef KT1_UNIT_COLUMNS_H
#define KT1_UNIT_COLUMNS_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"

namespace KT1::Core::SimHost {

class UnitColumns {
public:
	UnitColumns() = default;
	UnitColumns(const UnitColumns&) = delete;             // it holds pointers to the columns it owns
	UnitColumns& operator=(const UnitColumns&) = delete;

	// Forgets the owned columns: Own them again afterwards.
	void Init(int32_t maxIds) {
		index.Init(maxIds);
		owned.clear();
	}

	// Sizes the column and keeps it in step from now on. It must live as long as this object is used.
	template<class T> void Own(Column<T>& column, const T& blank = T{}) {
		column.Init(index, blank);
		owned.push_back({&column, [](void* c, const DenseIndex::Move& m) { static_cast<Column<T>*>(c)->Apply(m); }});
	}

	DenseIndex::Slot AddUnit(DenseIndex::Id id) { return index.Add(id); }

	// KT1 hot
	void RemoveUnit(DenseIndex::Id id) {
		const DenseIndex::Move move = index.Remove(id);
		for (const Owned& o : owned) o.apply(o.column, move);
	}

	DenseIndex index;

private:
	struct Owned {
		void* column;
		void (*apply)(void*, const DenseIndex::Move&);
	};
	std::vector<Owned> owned;
};

} // namespace KT1::Core::SimHost

#endif
