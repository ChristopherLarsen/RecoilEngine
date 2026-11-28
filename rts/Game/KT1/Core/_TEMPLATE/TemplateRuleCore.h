/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 template: the logic of a synced system. No engine include: tested engine-free (Test/_TEMPLATE/TemplateRuleTest.cpp).
// Copy to Core/<System>/<System>Core.h (and a .cpp when it grows). A reader learns the system from this file: its data
// and its decisions; docs/engine/rules/<system>.md says why.
//
// The example: every tracked unit holds a charge that rises by `gain` at each visit, up to `cap`. A DueWheel says which
// units are due, so no step scans the army.
//
// What a copy keeps (LUA-FREE-PLAN 2.2, 2.7):
//   - per-unit state in columns beside one DenseIndex (UnitColumns keeps them in step on removal);
//   - synced maths in float through DetMath, in the replaced Lua's order;
//   - periods as step counts the adapter makes from config seconds (Core::Rate), never a frame number;
//   - `// KT1 hot` on every function that runs per unit; no allocation after Init.

#ifndef KT1_TEMPLATE_RULE_CORE_H
#define KT1_TEMPLATE_RULE_CORE_H

#include <cstdint>
#include <span>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Common/DetMath.h"
#include "Game/KT1/Core/Common/DueWheel.h"
#include "Game/KT1/Core/SimHost/Phase.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"

namespace KT1::Core::Template {

// the config in sim units, made once at game setup
struct RuleParams {
	int32_t periodSteps = 1;
	float gain = 0;
	float cap = 0;
};

inline float Charge(float old, const RuleParams& p) { return DetMath::Clamp(old + p.gain, 0, p.cap); }

class RuleState {
public:
	// every allocation happens here (the wheel grows a bucket once if more units share it)
	void Init(int32_t maxIds, const RuleParams& p) {
		units.Init(maxIds);
		units.Own(charge);
		units.Own(dueStep);
		units.Own(serial);
		const int32_t buckets = p.periodSteps + 1;
		wheel.Init(buckets, maxIds / buckets + 1, 0);
		nextSerial = 0;
	}

	void Add(int32_t id, const RuleParams& p) {
		if (units.index.Has(id)) return;
		const DenseIndex::Slot slot = units.AddUnit(id);
		if (slot == DenseIndex::NONE) return;
		serial[slot] = ++nextSerial;
		Schedule(id, slot, p);
	}

	void Remove(int32_t id) {
		const DenseIndex::Slot slot = units.index.SlotOf(id);
		if (slot == DenseIndex::NONE) return;
		wheel.Cancel(id, dueStep[slot]);   // a later unit with this id must not inherit the visit
		units.RemoveUnit(id);
	}

	// Exactly once per sim step, before Visit: the ids due on it, ascending. A sliced step takes them once and hands the
	// same list to every slice.
	std::span<const int32_t> TakeDue() { return wheel.Take(); }

	// KT1 hot
	void Visit(std::span<const int32_t> due, const SimHost::IdRange& range, const RuleParams& p) {
		for (const int32_t id : due) {
			const DenseIndex::Slot slot = units.index.SlotOf(id);
			if (slot == DenseIndex::NONE || !range.Contains(id)) continue;
			charge[slot] = Charge(charge[slot], p);
			Schedule(id, slot, p);
		}
	}

	SimHost::UnitColumns units;
	Column<float> charge;
	Column<int32_t> dueStep;
	Column<uint32_t> serial;   // tells two units with one id apart (Snapshot<T>)

private:
	// KT1 hot
	void Schedule(int32_t id, DenseIndex::Slot slot, const RuleParams& p) {
		dueStep[slot] = wheel.Now() + p.periodSteps;
		wheel.Schedule(id, dueStep[slot]);
	}

	DueWheel wheel;
	uint32_t nextSerial = 0;
};

} // namespace KT1::Core::Template

#endif
