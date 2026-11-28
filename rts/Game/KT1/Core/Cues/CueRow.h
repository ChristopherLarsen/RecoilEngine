/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the sound cue rows (an EventMap, LUA-FREE H01, FRAME §4.4) and the config of system "soundcues"
// (data/kt1/sound_events.json). No engine include. Replaces the rules and tunables of LuaRules/Gadgets/snd_events.lua and
// its use of LuaUI/lib/snd_gate.lua. Which sound item a cue name plays stays the sound data's (gamedata/sound_events.lua,
// read by the sound manager).
//
// A row's `on` (Core/Common/EventMapRow.h) is a sim event name (UnitDestroyed, ProjectileCreated, Explosion, ...) or one
// of these triggers: move_start, move_stop, fuel_low, fuel_empty, refuel, repair, capture_start, capture_stall,
// capture_complete, dive, surface, shield_hit, shield_up, shield_break, shield_fall, build_loop, and any cue a caller
// raises by name (Audio/Cues/CueApi.h: cargo_load, dock, silo_ready, launch_strategic, jam_on, sacrifice, ...).
// A cue raised by name needs no row.

#ifndef KT1_CORE_CUE_ROW_H
#define KT1_CORE_CUE_ROW_H

#include <string>
#include <vector>

#include "Game/KT1/Core/Common/EventMapRow.h"

namespace KT1::Core::Cues {

struct CueRow {
	EventMapRow map;               // on, unit_class, weapon, relation
	std::string cue;               // the cue name raised
	std::string variant;
	float min_gap_s = 0.0f;        // DESIGN: seconds between two raises of this row for one weapon def; 0: no limit
	float max_per_s = 0.0f;        // DESIGN: raises of this row per second; 0: no limit
	template<class S> void Describe(S& s) {
		map.Describe(s);
		s.Field("cue", cue);
		s.Field("variant", variant);
		s.Field("min_gap_s", min_gap_s);
		s.Field("max_per_s", max_per_s);
	}
};

// a row that raises `cue` on `on`, with no filter and no limit
inline CueRow PlainRow(const char* on, const char* cue) {
	CueRow row;
	row.map.on = on;
	row.cue = cue;
	return row;
}

// a cue the audio side holds back above a camera height: the manager would drop it there anyway (PERF-REVIEW-A A6)
struct CueZoomGate {
	std::string cue;
	float zoom_cut = 0.0f;         // elmos above the ground; 0: from the sound data (SetZoomCut), none: never held back
	template<class S> void Describe(S& s) {
		s.Field("cue", cue);
		s.Field("zoom_cut", zoom_cut);
	}
};

struct CueConfig {
	float max_per_s = 480.0f;                 // PLACEHOLDER (DESIGN 2026-10-25): cues forwarded per second (16 a step at the reference rate); the rest is dropped
	float move_check_s = 0.33333334f;         // DESIGN: seconds in which every mobile unit's movement is looked at once
	float moving_speed = 9.0f;                // DESIGN: elmos per second above which a unit counts as moving
	float beam_gap_s = 0.2f;                  // DESIGN: seconds between impact cues of one beam weapon
	std::string beam_weapon_type = "BeamLaser";   // DESIGN: the weapon type the beam gap applies to
	float fuel_low_fraction = 0.19f;          // DESIGN (2026-11-10): where the unit sets none (spc_fuel_low_fraction)
	float fuel_fall_epsilon = 0.000001f;      // DESIGN: fuel seconds below which a change counts as none (low, full)
	float fuel_move_epsilon = 0.0001f;        // DESIGN: fuel seconds a rise or fall must exceed
	float health_epsilon = 0.001f;            // DESIGN: health a repair must add
	float capture_epsilon = 0.000001f;        // DESIGN: capture progress that counts as growth
	float capture_stall_s = 1.0f;             // DESIGN: seconds without growth before capture_stall (ECO-010 v2 cues)
	float build_check_s = 0.33333334f;        // DESIGN: seconds between the beam-on checks of the build loops
	float build_distance_default = 128.0f;    // DESIGN: elmos, where a def gives none
	float shield_hit_gap_s = 0.25f;           // DESIGN (2026-10-30, shields-review): seconds between hit sounds of one shield
	std::string strategic_alert = "nuclear_launch";   // REQ UI-GAP-001 R7: the other sides are told, without a position
	std::string strategic_severity = "critical";      // REQ UI-GAP-001 R7
	std::vector<CueZoomGate> zoom_gates = {
		{"fire", 0.0f},                       // DESIGN (PERF-REVIEW-A A6): the fight cues the audio side may hold back
		{"impact", 0.0f},                     // DESIGN
	};
	std::vector<CueRow> rows = {
		PlainRow("ProjectileCreated", "fire"),    // DESIGN: the automatic cues snd_events raised
		PlainRow("Explosion", "impact"),          // DESIGN
		PlainRow("move_start", "move_start"),     // DESIGN
		PlainRow("move_stop", "move_stop"),       // DESIGN
	};
	template<class S> void Describe(S& s) {
		s.Field("max_per_s", max_per_s);
		s.Field("move_check_s", move_check_s);
		s.Field("moving_speed", moving_speed);
		s.Field("beam_gap_s", beam_gap_s);
		s.Field("beam_weapon_type", beam_weapon_type);
		s.Field("fuel_low_fraction", fuel_low_fraction);
		s.Field("fuel_fall_epsilon", fuel_fall_epsilon);
		s.Field("fuel_move_epsilon", fuel_move_epsilon);
		s.Field("health_epsilon", health_epsilon);
		s.Field("capture_epsilon", capture_epsilon);
		s.Field("capture_stall_s", capture_stall_s);
		s.Field("build_check_s", build_check_s);
		s.Field("build_distance_default", build_distance_default);
		s.Field("shield_hit_gap_s", shield_hit_gap_s);
		s.Field("strategic_alert", strategic_alert);
		s.Field("strategic_severity", strategic_severity);
		s.List("zoom_gates", zoom_gates, [](auto& o, CueZoomGate& g) { g.Describe(o); });
		s.List("rows", rows, [](auto& o, CueRow& row) { row.Describe(o); });
	}
};

} // namespace KT1::Core::Cues

#endif
