/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS draw: the looks the effects read by roster key (replaces gamedata/fx_factions.lua and the name lookups of
// the look tables). The data layer fills it; defaults are empty. A look table (the shot recipes of
// Draw/ShotFollower/ShotRecipe.h) is a list of named rows: its RosterKey is the row's index. Engine-free.

#ifndef KT1_FX_ROSTER_H
#define KT1_FX_ROSTER_H

#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Draw/FxCommon/FxParticles.h"

namespace KT1::FxDraw {

using Core::Fx::Faction;
using Core::Fx::RosterKey;

enum class PaletteRole : uint8_t { Main, Core, Spark, Smoke, Shock, Ichor, Alt, Count };

struct FactionLook {
	std::string code;                       // DATA: hum, ara, gry, had
	float main[3] = {1.0f, 1.0f, 1.0f};     // DATA
	float core[3] = {1.0f, 1.0f, 1.0f};     // DATA
	float spark[3] = {1.0f, 1.0f, 1.0f};    // DATA
	float smoke[3] = {0.5f, 0.5f, 0.5f};    // DATA
	float shock[3] = {1.0f, 1.0f, 1.0f};    // DATA
	float ichor[3] = {1.0f, 1.0f, 1.0f};    // DATA
	float alt[3] = {1.0f, 1.0f, 1.0f};      // DATA
	std::string energy_role = "main";       // DATA: the palette role of its energy looks
	bool trail_smoke = true;                // DATA
	float hit_smoke = 1.0f;                 // DATA: the weight of smoke in its hits
	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("main", main);
		s.Field("core", core);
		s.Field("spark", spark);
		s.Field("smoke", smoke);
		s.Field("shock", shock);
		s.Field("ichor", ichor);
		s.Field("alt", alt);
		s.Field("energy_role", energy_role);
		s.Field("trail_smoke", trail_smoke);
		s.Field("hit_smoke", hit_smoke);
	}
};

struct RosterConfig {                       // game/data/kt1/fxroster.json
	std::vector<FactionLook> factions;      // DATA: in the order of Core::Fx::Faction
	template<class S> void Describe(S& s) {
		s.List("factions", factions, [](auto& o, FactionLook& f) { f.Describe(o); });
	}
};

struct KeyTable {                           // names sorted for the lookup, each with its row
	struct Entry { std::string name; RosterKey key = Core::Fx::NO_KEY; };
	std::vector<Entry> entries;
};

void BuildKeys(KeyTable& table, const std::vector<std::string>& names);       // key = index in `names`
RosterKey KeyOf(const KeyTable& table, std::string_view name);                // NO_KEY: unknown
// a faction the data has no row for: a white look
const FactionLook& LookOfFaction(const RosterConfig& roster, Faction faction);
Rgb ColourOf(const FactionLook& look, PaletteRole role);
PaletteRole RoleOf(std::string_view name);                                   // unknown: Main

} // namespace KT1::FxDraw

#endif
