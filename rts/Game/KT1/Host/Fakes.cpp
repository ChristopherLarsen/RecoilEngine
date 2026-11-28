/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: see Fakes.h.

#include "Fakes.h"

#include <cstdint>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"

namespace KT1 {
namespace {

std::vector<uint8_t> defFlag;   // by unit def id; empty until the first question of a game (the defs are loaded by then)

void ReadDefs() {
	const std::vector<UnitDef>& defs = unitDefHandler->GetUnitDefsVec();
	defFlag.assign(defs.size(), 0);
	for (size_t id = 0; id < defs.size(); ++id) {
		const auto it = defs[id].customParams.find("spc_blip");
		defFlag[id] = (it != defs[id].customParams.end() && it->second == "1") ? 1 : 0;
	}
}

} // namespace

// KT1 hot
bool Fakes::IsDef(const UnitDef* def) {
	if (defFlag.empty()) ReadDefs();
	return defFlag[def->id] != 0;
}

bool Fakes::Is(const CUnit* unit) { return IsDef(unit->unitDef); }

bool Fakes::Sees(const char* system) {
	const Rule* r = Registry::Get().Find(system);
	return r != nullptr && r->Info().seesFakes;
}

void Fakes::Reset() { defFlag.clear(); }

} // namespace KT1
