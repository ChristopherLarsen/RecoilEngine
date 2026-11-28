/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimProbe core: the test system's per-unit state, built from event batches (Rules/SimProbe). No engine include.
//
// A unit is tracked from UnitCreated to UnitDestroyed. For each: the damage it has taken and the number of unit events
// about it. Apply events in the order they were raised: a unit id is reused after its unit dies.

#ifndef KT1_SIM_PROBE_CORE_H
#define KT1_SIM_PROBE_CORE_H

#include <cstdint>

#include "Game/KT1/Core/SimHost/SimEventLog.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"

namespace KT1::Core::SimProbe {

// The unit event types come first in EventType.
constexpr bool IsUnitEvent(SimHost::EventType t) { return t <= SimHost::EventType::UnitIdle; }

class State {
public:
	void Init(int32_t maxIds) {
		units.Init(maxIds);
		units.Own(damage);
		units.Own(events);
	}

	// KT1 hot
	void Apply(const SimHost::Event& e) {
		if (!IsUnitEvent(e.type)) return;
		if (e.type == SimHost::EventType::UnitCreated) units.AddUnit(e.id);
		const DenseIndex::Slot slot = units.index.SlotOf(e.id);
		if (slot == DenseIndex::NONE) return;
		++events[slot];
		if (e.type == SimHost::EventType::UnitDamaged) damage[slot] += e.value;   // float, as the Lua twin adds it
		if (e.type == SimHost::EventType::UnitDestroyed) units.RemoveUnit(e.id);
	}

	SimHost::UnitColumns units;
	Column<float> damage;
	Column<int32_t> events;
};

} // namespace KT1::Core::SimProbe

#endif
