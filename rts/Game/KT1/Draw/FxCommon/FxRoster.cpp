/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 EFFECTS draw: roster lookups. Stub (LUA-FREE H01): step D03 writes the bodies.

#include "Game/KT1/Draw/FxCommon/FxRoster.h"

namespace KT1::FxDraw {

void BuildKeys(KeyTable&, const std::vector<std::string>&) {}
RosterKey KeyOf(const KeyTable&, std::string_view) { return Core::Fx::NO_KEY; }
const FactionLook& LookOfFaction(const RosterConfig&, Faction) {
	static const FactionLook none;
	return none;
}
Rgb ColourOf(const FactionLook&, PaletteRole) { return {}; }
PaletteRole RoleOf(std::string_view) { return PaletteRole::Main; }

} // namespace KT1::FxDraw
