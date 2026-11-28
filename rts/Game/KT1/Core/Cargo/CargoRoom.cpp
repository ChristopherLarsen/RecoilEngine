/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core: transport room (CargoRoom.h), as spc_cargo.lua has it. Float maths in the Lua's order.

#include "Game/KT1/Core/Cargo/CargoRoom.h"

#include <algorithm>

namespace KT1::Core::Cargo {

namespace {
constexpr float EPS = 1e-6f;   // DESIGN (the Lua's EPS): room counters are fractions (carry_use 0.25 ...)
constexpr int32_t SMALL = 0;
}

Room Used(const Transport& t, const RoomModel& m) {
	Room u{};
	for (const int32_t a : t.aboard) {
		const int32_t z = (a >= 0 && a < CLASSES) ? a : SMALL;   // Lua: alloc[c] = sizeOf[cDef] or "small"
		for (int32_t k = 0; k < CLASSES; ++k) u[k] = u[k] + m.use[z][k];
	}
	return u;
}

int32_t Place(const Transport& t, int32_t size, const Room& extra, const RoomModel& m) {
	if (size < 0 || size >= CLASSES) return NO_SIZE;
	const Room u = Used(t, m);
	const Room& need = m.need[size];
	for (int32_t k = 0; k < CLASSES; ++k) {
		// Lua: need[k] > 0 and sl[k] - u[k] - (extra and extra[k] or 0) >= need[k] - EPS
		if (need[k] > 0 && t.slots[k] - u[k] - extra[k] >= need[k] - EPS) return size;
	}
	return NO_SIZE;
}

void SelectLoad(const Transport& t, std::span<const int32_t> sizes, const RoomModel& m, std::vector<int32_t>& taken,
	std::vector<int32_t>& left) {
	taken.clear();
	left.clear();
	const int32_t n = static_cast<int32_t>(sizes.size());
	if (!t.hasSlots) {
		// Lua: room = math.max(0, (transportCapacity or 1) - #aboard)
		const int32_t room = std::max<int32_t>(0, t.capacity - static_cast<int32_t>(t.aboard.size()));
		for (int32_t i = 0; i < n; ++i) (static_cast<int32_t>(taken.size()) < room ? taken : left).push_back(i);
		return;
	}
	Room extra{};
	for (int32_t i = 0; i < n; ++i) {
		const int32_t p = Place(t, sizes[i], extra, m);
		if (p != NO_SIZE) {
			for (int32_t k = 0; k < CLASSES; ++k) extra[k] = extra[k] + m.use[p][k];
			taken.push_back(i);
		} else {
			left.push_back(i);
		}
	}
}

bool Full(const Transport& t, const RoomModel& m) {
	if (!t.hasSlots) return static_cast<int32_t>(t.aboard.size()) >= t.capacity;
	const Room u = Used(t, m);
	for (int32_t z = 0; z < CLASSES; ++z) {
		for (int32_t k = 0; k < CLASSES; ++k) {
			if (m.need[z][k] > 0 && t.slots[k] - u[k] >= m.need[z][k] - EPS) return false;
		}
	}
	return true;
}

} // namespace KT1::Core::Cargo
