/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/Common: Column<T> - one structure-of-arrays column of per-unit state, indexed by a DenseIndex slot.
// Why: a system keeps each per-unit field in its own column so a hot loop reads only the fields it needs
//   (LUA-FREE-PLAN §2.2 p1). The system that owns the DenseIndex owns its columns (§2.2 p4).
// Rules: size it with Init(index) once; after each DenseIndex::Remove call Apply(move) on every column, in any order.
//   A slot at or past index.Size() always holds the blank value, so a newly added slot starts blank.
// Steady state: no allocation (T should not own heap memory; a T that does allocates when it is copied).
// No engine include.

#ifndef KT1_CORE_COMMON_COLUMN_H
#define KT1_CORE_COMMON_COLUMN_H

#include "DenseIndex.h"

#include <span>
#include <utility>
#include <vector>

namespace KT1::Core {

template <typename T>
class Column {
public:
	using Slot = DenseIndex::Slot;

	// one value per possible slot (the index's capacity), all blank; allocates once
	void Init(const DenseIndex& index, const T& blankValue = T{}) {
		blank = blankValue;
		data.assign(static_cast<size_t>(index.Capacity()), blank);
	}

	T& operator[](Slot slot) { return data[slot]; }
	const T& operator[](Slot slot) const { return data[slot]; }

	// mirror a DenseIndex::Remove: move the last slot's value into the hole and blank the last slot
	void Apply(const DenseIndex::Move& move) {
		if (move.from == DenseIndex::NONE)
			return;
		if (move.from != move.to)
			data[move.to] = std::move(data[move.from]);
		data[move.from] = blank;
	}

	// the live slots 0 .. index.Size()-1, for a hot loop
	std::span<T> Live(const DenseIndex& index) { return {data.data(), static_cast<size_t>(index.Size())}; }
	std::span<const T> Live(const DenseIndex& index) const { return {data.data(), static_cast<size_t>(index.Size())}; }

private:
	std::vector<T> data;
	T blank{};
};

} // namespace KT1::Core

#endif
