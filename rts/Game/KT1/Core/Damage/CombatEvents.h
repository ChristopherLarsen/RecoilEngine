/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 COMBAT core: the family's typed display events (LUA-FREE H01, COMBAT §4.8). No engine include.
//
// The sim sends them with DisplayEvents::Send, the draw side registers with DisplaySink::Listen<T>. Positions are
// elmos, x y z. They are look only: nothing synced reads them. The scorch mark is EFFECTS' event
// (Core/FxEvents/FxEvent.h, KT1::Core::Fx::Scorch): the death blast raises that one, COMBAT declares none.
// Visibility (is the place seen by the viewer) is the listener's test, as in the Lua's unsynced handlers.

#ifndef KT1_CORE_COMBAT_EVENTS_H
#define KT1_CORE_COMBAT_EVENTS_H

#include <cstdint>

#include "Game/KT1/Core/Common/DisplayTypes.h"

namespace KT1::Core::Damage {

// "dot_tick" (cmb_damage, cmb_impact_patterns): a damage-over-time tick.
struct DotTick {
	static constexpr int32_t TYPE = Core::DisplayRange::COMBAT + 1;
	int32_t unit = -1;      // the single target; -1: an area tick at pos
	float pos[3] = {};      // where the pulse was scheduled (single target: the listener uses the unit's place now)
	float radius = 0;       // area ticks: the radius, elmos; single target: 0
};

} // namespace KT1::Core::Damage

namespace KT1::Core::TimedRules {

// "chg" (cmb_charge): the charge progress of a weapon, 0..1.
struct ChargeProgress {
	static constexpr int32_t TYPE = Core::DisplayRange::COMBAT + 2;
	int32_t unit = -1;
	int32_t weapon = 0;     // weapon number, from 1
	float progress = 0;
};

// "chg_fire": a charged weapon was let fire (or its wind-up completed).
struct ChargeFired {
	static constexpr int32_t TYPE = Core::DisplayRange::COMBAT + 3;
	int32_t unit = -1;
	int32_t weapon = 0;     // weapon number, from 1
};

} // namespace KT1::Core::TimedRules

namespace KT1::Core::Veterancy {

// A unit gained a level from a kill (not sent for a rebuild after a transfer). No Lua sender today: DESIGN, for the
// level-up cue and effect.
struct VetLevelUp {
	static constexpr int32_t TYPE = Core::DisplayRange::COMBAT + 4;
	int32_t unit = -1;
	int32_t level = 0;
	float pos[3] = {};
};

} // namespace KT1::Core::Veterancy

#endif
