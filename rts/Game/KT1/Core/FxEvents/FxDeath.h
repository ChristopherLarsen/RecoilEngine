/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: unit death and damage-state sets (replaces code/LuaRules/Gadgets/fx_death_sets.lua and
// gamedata/fx_death_logic.lua). Sizes in map units, times in ticks. No engine include.
//
// The roster (the lists and the sequence numbers) is data: its defaults are empty and the data file gives them, from the
// values the Lua reads in gamedata/fx_dth_roster.lua (parity pack G0-WVFXDTH).

#ifndef KT1_FX_DEATH_H
#define KT1_FX_DEATH_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::Core::Fx {

struct DeathSelectRow {
	std::string layer;                // DATA: land, air, water, sub
	float bound_lo = 0.0f;            // DATA: bound_xz at least this
	float bound_hi = -1.0f;           // DATA: and under this; negative: no upper end
	std::string base;                 // DATA: the set
	std::string extra;                // DATA: the extra set; empty: none
	float extra_below = 0.0f;         // DATA: the extra set only under this bound_xz
	template<class S> void Describe(S& s) {
		s.Field("layer", layer);
		s.Field("bound_lo", bound_lo);
		s.Field("bound_hi", bound_hi);
		s.Field("base", base);
		s.Field("extra", extra);
		s.Field("extra_below", extra_below);
	}
};

struct DeathSequenceRow {
	std::string kind;                 // DATA: mobile, air, structure, command, sea, sub
	int wait_min_ticks = 0;           // DATA
	int wait_max_ticks = 0;           // DATA
	template<class S> void Describe(S& s) {
		s.Field("kind", kind);
		s.Field("wait_min_ticks", wait_min_ticks);
		s.Field("wait_max_ticks", wait_max_ticks);
	}
};

struct DamageCountRow {
	float volume_above = 0.0f;        // DATA: map units cubed; 0: every unit
	int count = 1;                    // DATA: emitters
	float scale = 1.0f;               // DATA
	template<class S> void Describe(S& s) {
		s.Field("volume_above", volume_above);
		s.Field("count", count);
		s.Field("scale", scale);
	}
};

struct DamageStateRow {
	std::string fx_class;             // DATA: mobile, structure, sub
	std::string state;                // DATA
	float fraction = 0.0f;            // DATA: active at or below this health share
	std::string alternatives;         // DATA: "set+set|set": one alternative is drawn at random per emitter
	template<class S> void Describe(S& s) {
		s.Field("fx_class", fx_class);
		s.Field("state", state);
		s.Field("fraction", fraction);
		s.Field("alternatives", alternatives);
	}
};

struct DeathConfig {
	bool enabled = true;              // DESIGN: the Lua's mod option fx_dth_sets
	float tick_s = 0.1f;              // PARITY: G0-WVFXDTH one tick
	float scan_s = 0.5f;              // DESIGN: damaged units' health is read twice a second
	float state_repeat_s = 2.0f;      // DESIGN: a damage-state emitter is made again after this
	float map_unit = 8.0f;            // REQ OUT-0001: elmos per map unit
	float air_above_units = 2.0f;     // DESIGN: higher than this above ground or water is the air layer, map units
	float sub_below_units = 1.0f;     // DESIGN: deeper than this over a sea floor is the sub layer, map units
	std::string effect_prefix = "fxs_dth_";   // DESIGN: effect name = prefix + set
	float bound_xz_factor = 0.0f;     // DATA: bound_xz = factor x (sx + sz)
	std::vector<DeathSelectRow> selection;    // DATA
	std::vector<DeathSequenceRow> sequence;   // DATA
	float timed_threshold = 0.0f;     // DATA: structures at least this wide make the timed run first
	float timed_wait_ticks[2] = {0.0f, 0.0f};         // DATA: between two timed hits
	float timed_count_per_legacy[2] = {0.0f, 0.0f};   // DATA: hits per map unit of width (the second is used)
	float timed_offset_box_fraction = 0.0f;           // DATA
	int timed_pause_ticks = 0;        // DATA: before the final sets
	std::string timed_set = "H1";     // PARITY: G0-WVFXDTH the timed run's hit
	std::string air_kill_set = "H2";  // PARITY: G0-WVFXDTH one hit at a kill in flight
	std::vector<DamageCountRow> damage_counts;        // DATA
	std::vector<DamageStateRow> damage_states;        // DATA
	template<class S> void Describe(S& s) {
		s.Field("enabled", enabled);
		s.Field("tick_s", tick_s);
		s.Field("scan_s", scan_s);
		s.Field("state_repeat_s", state_repeat_s);
		s.Field("map_unit", map_unit);
		s.Field("air_above_units", air_above_units);
		s.Field("sub_below_units", sub_below_units);
		s.Field("effect_prefix", effect_prefix);
		s.Field("bound_xz_factor", bound_xz_factor);
		s.List("selection", selection, [](auto& o, DeathSelectRow& r) { r.Describe(o); });
		s.List("sequence", sequence, [](auto& o, DeathSequenceRow& r) { r.Describe(o); });
		s.Field("timed_threshold", timed_threshold);
		s.Field("timed_wait_ticks", timed_wait_ticks);
		s.Field("timed_count_per_legacy", timed_count_per_legacy);
		s.Field("timed_offset_box_fraction", timed_offset_box_fraction);
		s.Field("timed_pause_ticks", timed_pause_ticks);
		s.Field("timed_set", timed_set);
		s.Field("air_kill_set", air_kill_set);
		s.List("damage_counts", damage_counts, [](auto& o, DamageCountRow& r) { r.Describe(o); });
		s.List("damage_states", damage_states, [](auto& o, DamageStateRow& r) { r.Describe(o); });
	}
};

constexpr int32_t DEATH_STATES = 4;   // DESIGN: damage states one fx class may have

// the roster with its names resolved once (Start): indexes into the effect list, -1: none
struct DeathSelect { DeathLayer layer = DeathLayer::Land; float lo = 0.0f, hi = -1.0f, extraBelow = 0.0f; int32_t base = -1, extra = -1; };
struct DamageState { float fraction = 0.0f; std::vector<std::vector<int32_t>> alternatives; };
struct DeathPlan {
	std::vector<DeathSelect> selection;
	std::vector<DamageState> states[3];         // by fx class: mobile, structure, sub
	std::vector<std::string> sets;              // the set names the indexes above point at
	int32_t timedSet = -1, airKillSet = -1;
	float topFraction = 0.0f;                   // the highest threshold of any class
};

struct DeathParts {
	int32_t waitTicks = 0;
	bool timed = false;
	int32_t base = -1, extra = -1;              // indexes into DeathPlan::sets
};

// false: a roster row names an unknown layer or class (the row is left out)
bool ResolveDeathPlan(const DeathConfig& c, DeathPlan& out);
DeathLayer LayerOf(const DeathConfig& c, float y, float groundHeight);
int32_t DamageClassOf(DeathKind kind);          // 0 mobile, 1 structure, 2 sub
// sizes in map units; legacy: the larger horizontal size. One random draw (the wait).
DeathParts PartsOfDeath(const DeathConfig& c, const DeathPlan& plan, DeathKind kind, DeathLayer layer, float sx, float sz,
	float legacy, int32_t (*range)(void* ctx, int32_t lo, int32_t hi), void* ctx);
uint32_t ActiveStates(const DeathPlan& plan, int32_t damageClass, float healthShare);   // bit per state
int32_t DamageEmitters(const DeathConfig& c, float volume);

} // namespace KT1::Core::Fx

#endif
