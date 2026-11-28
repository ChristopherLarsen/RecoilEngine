/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitFx: what decides a unit's emitters: its faction, its health band, whether it moves or flies and how much detail
// the view gives it (replaces U.DamageState, U.Distance and U.SizeMul of gamedata/fx_unit_fx.lua). Engine-free.

#ifndef KT1_UNIT_FX_EMITTER_KEY_H
#define KT1_UNIT_FX_EMITTER_KEY_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::UnitFx {

struct UnitFxConfig;

enum class HealthBand : uint8_t { None, Light, Heavy };   // none, smoke, fire
enum class DetailBand : uint8_t { Full, Half, None };     // near; mid (no secondary parts); icon (only the plume, small)

struct EmitterKey {
	Core::Fx::Faction faction = Core::Fx::Faction::Human;
	HealthBand health = HealthBand::None;
	DetailBand detail = DetailBand::None;
	bool moving = false;
	bool airborne = false;
};

// a unit being built or without health shows no damage
HealthBand HealthBandOf(const UnitFxConfig& c, float health, float maxHealth, float build);
// dist: camera to unit, elmos; icon: the engine draws its icon; the zoom gates may lower the band (facts answered by the
// caller: fullOpen false gives Half at best, anyOpen false gives None)
DetailBand DetailOf(const UnitFxConfig& c, float dist, bool icon, bool fullOpen, bool anyOpen);
float SizeFactor(const UnitFxConfig& c, float radius);    // recipe sizes scale with the root of radius over the reference

} // namespace KT1::UnitFx

#endif
