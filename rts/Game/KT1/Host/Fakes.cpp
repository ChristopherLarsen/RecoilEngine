/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: see Fakes.h.

#include "Fakes.h"

#include <cstdint>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"

namespace KT1 {
namespace {

std::vector<int8_t> defFlag;   // by unit def id: -1 not read yet, else 0 or 1

} // namespace

// KT1 hot
bool Fakes::IsDef(const UnitDef* def) {
	const size_t id = static_cast<size_t>(def->id);
	if (id >= defFlag.size()) defFlag.resize(id + 1, -1);
	if (defFlag[id] < 0) {
		const auto it = def->customParams.find("spc_blip");
		defFlag[id] = (it != def->customParams.end() && it->second == "1") ? 1 : 0;
	}
	return defFlag[id] != 0;
}

bool Fakes::Is(const CUnit* unit) { return IsDef(unit->unitDef); }

bool Fakes::Sees(const char* system) {
	const Rule* r = Registry::Get().Find(system);
	return r != nullptr && r->Info().seesFakes;
}

void Fakes::Reset() { defFlag.clear(); }

} // namespace KT1
