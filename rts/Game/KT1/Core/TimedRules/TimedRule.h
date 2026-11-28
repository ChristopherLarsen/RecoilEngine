/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules core: one row type for "when this happens to a unit, do that, and undo it later", and the system's
// config, game/data/kt1/timedrules.json (LUA-FREE H01, COMBAT §4.3; step S21). No engine include.
//
// Replaces the per-unit timer walks of cmb_stun.lua (with gamedata/cmb_stun.lua), cmb_charge.lua and cmb_crush.lua
// (with gamedata/cmb_crush.lua). A row names a trigger, a duration in seconds (made steps with Core::Rate) and the
// effect names at the start and at the end; per-unit state of every row waits on one DueWheel (TimedState.h). Later
// families add rows to the file and register their effects (TimedRulesApi.h): no change here.
// Describe is a template over the config schema (Data/Schema.h).

#ifndef KT1_CORE_TIMED_RULE_H
#define KT1_CORE_TIMED_RULE_H

#include <cstdint>
#include <string>
#include <vector>

namespace KT1::Core::TimedRules {

// what starts a row, as the file names it: "Explosion", "WeaponFire", "Step" (walked every step), "Armed" (a system)
enum class Trigger : uint8_t { Explosion, WeaponFire, Step, Armed };

struct TimedRow {
	std::string id;
	std::string on = "Armed";              // a Trigger name
	float duration_s = 0;                  // DESIGN: 0: the trigger gives the time (a weapon's stun entry)
	std::string effect_start;
	std::string effect_end;                // empty: nothing
	template<class S> void Describe(S& s) {
		s.Field("id", id);
		s.Field("on", on);
		s.Field("duration_s", duration_s);
		s.Field("effect_start", effect_start);
		s.Field("effect_end", effect_end);
	}
};

// REQ CMB-023 R1 to R4: one stun entry of a weapon
struct StunEntryConfig {
	float seconds = 0;                     // DESIGN: unset; the data file gives the values
	float radius_mu = 0;                   // map units; 0: only the unit the shot hit
	std::string allowed;                   // comma-separated classes; empty: any
	std::string excluded;                  // comma-separated classes
	template<class S> void Describe(S& s) {
		s.Field("seconds", seconds);
		s.Field("radius_mu", radius_mu);
		s.Field("allowed", allowed);
		s.Field("excluded", excluded);
	}
};

// the stuns of one weapon, named by unit code and weapon number (the roster) or by weapon def name (fixtures)
struct StunConfig {
	std::string unit_code;                 // empty: matched by weapon_name
	int weapon = 1;                        // the unit's weapon number, from 1
	std::string weapon_name;
	bool needs_upgrade = false;            // REQ CMB-023 R6
	std::vector<StunEntryConfig> entries;
	template<class S> void Describe(S& s) {
		s.Field("unit_code", unit_code);
		s.Field("weapon", weapon);
		s.Field("weapon_name", weapon_name);
		s.Field("needs_upgrade", needs_upgrade);
		s.List("entries", entries, [](auto& o, StunEntryConfig& item) { item.Describe(o); });
	}
};

// an extra target class for units whose code holds a part
struct ClassByCode {
	std::string code_part;
	std::string class_name;
	template<class S> void Describe(S& s) {
		s.Field("code_part", code_part);
		s.Field("class_name", class_name);
	}
};

struct TimedRulesConfig {
	float elmos_per_mu = 8;                    // REQ OUT-0001: elmos in one map unit
	std::vector<TimedRow> rows = {             // DESIGN: the three rules this system replaces, in the order they run
		{"charge", "WeaponFire", 0, "draw_energy", ""},
		{"crush", "Step", 0, "crush", ""},
		{"stun", "Explosion", 0, "paralyze", "clear_paralyze"}};

	// stun (REQ CMB-023)
	float paralyze_over = 10;                  // DESIGN: paralysis is this many times the unit's maximum health
	float stun_hit_search = 256;               // DESIGN: elmos searched for the unit a point stun hit
	float stun_area_pad = 64;                  // DESIGN: elmos added to an area stun's sphere query
	float stun_hit_pad = 1;                    // DESIGN: elmos added to a unit's radius for a hit
	float stun_area_epsilon = 1e-6f;           // DESIGN: on the squared distance (the edge is inside)
	int stun_log_cap = 200;                    // DESIGN: stuns the test log keeps
	std::vector<StunConfig> stuns;             // REQ CMB-023; DESIGN: empty, the data file gives roster and fixtures
	std::vector<ClassByCode> extra_classes;    // REQ CMB-023 R1; DESIGN: empty

	// weapon charge (REQ ECO-018); per weapon: customParams shot_cost_energy and drain_energy
	float charge_look_period_s = 0.13333334f;  // DESIGN: seconds between progress events of a charging weapon
	int charge_look_cap = 64;                  // DESIGN: look events a step
	float charge_ready_epsilon = 1e-6f;        // DESIGN: energy short of the cost that still counts as complete
	float charge_min_request = 1e-12f;         // DESIGN: smaller requests are not made

	// crushing (REQ CMB-017); per def: customParams crushes and crushable
	float crush_contact_pad = 24;              // PLACEHOLDER: elmos around the footprint that count as contact
	float crush_query_pad = 24;                // DESIGN: elmos added to the cylinder query
	float crush_move_threshold = 1e-6f;        // DESIGN: squared horizontal speed, elmos per step, above which it walks

	template<class S> void Describe(S& s) {
		s.Field("elmos_per_mu", elmos_per_mu);
		s.List("rows", rows, [](auto& o, TimedRow& item) { item.Describe(o); });
		s.Field("paralyze_over", paralyze_over);
		s.Field("stun_hit_search", stun_hit_search);
		s.Field("stun_area_pad", stun_area_pad);
		s.Field("stun_hit_pad", stun_hit_pad);
		s.Field("stun_area_epsilon", stun_area_epsilon);
		s.Field("stun_log_cap", stun_log_cap);
		s.List("stuns", stuns, [](auto& o, StunConfig& item) { item.Describe(o); });
		s.List("extra_classes", extra_classes, [](auto& o, ClassByCode& item) { item.Describe(o); });
		s.Field("charge_look_period_s", charge_look_period_s);
		s.Field("charge_look_cap", charge_look_cap);
		s.Field("charge_ready_epsilon", charge_ready_epsilon);
		s.Field("charge_min_request", charge_min_request);
		s.Field("crush_contact_pad", crush_contact_pad);
		s.Field("crush_query_pad", crush_query_pad);
		s.Field("crush_move_threshold", crush_move_threshold);
	}
};

} // namespace KT1::Core::TimedRules

#endif
