/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitFx: what the other effect systems ask of the unit effects (replaces GG.UnitFx and GG.UnitFxUnsynced; EFFECTS 4,
// step D03). Draw side. While the system is not on every call does nothing.
//
// Lua (dev scenes only, unsynced), Spring.KT1.UnitFx:
//   Stats()               -> { requested, spawned, craters, skipped = { [reason] = n }, craterNo = { [reason] = n } }
//   Active(unitID)        -> 0 | 1 | 2            the damage state drawn
//   State()               -> { damaged = n, walkers = n }
//   SetWreckPass(bool)                            true: wreck darkening by the old pass (GG.UfxWreckPass, an A/B shot)

#ifndef KT1_UNIT_FX_API_H
#define KT1_UNIT_FX_API_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

struct lua_State;

namespace KT1::UnitFx {

bool Active();
// one foot contact (Draw/Decals' track pass raises it): dust, spray or silt by what is under the foot
void Foot(int32_t unit, float x, float y, float z, Core::Fx::SizeBucket size, int32_t legs, int32_t step);
// a drawn blast (Draw/FxFields' blast pass): craters, burning plants, charged and XL hit marks
void Blast(Core::Fx::BlastClass cls, float x, float y, float z, int32_t weaponDef);
int32_t HealthBandOfUnit(int32_t unit);          // 0 none, 1 light, 2 heavy
uint32_t Particles();                            // added since the game began

void PushUnitFxBridge(lua_State* L);             // fills Spring.KT1.UnitFx (Rule::PushLua)

} // namespace KT1::UnitFx

#endif
