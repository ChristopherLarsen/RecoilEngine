/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: the system's config, game/data/kt1/damage.json (LUA-FREE H01, step S21). No engine include.
//
// Replaces the values of gamedata/cmb_values.lua (sections cmb and armour), spc_death.lua, the SPLASH_BY_CODE table of
// cmb_damage.lua and the pattern values of eco_values.lua. The defaults are the design layer's numbers or "unset"; the
// development roster's values and every unit code come from the data file only. Describe is a template over the config
// schema (Data/Schema.h). A test overlays another config at run time (DamageBridge.h, Debug.SetValues).

#ifndef KT1_CORE_DAMAGE_CONFIG_H
#define KT1_CORE_DAMAGE_CONFIG_H

#include <string>
#include <vector>

namespace KT1::Core::Damage {

struct ArmourMult {
	std::string type;
	float mult = 1;                        // REQ CMB-007 R3: a type not listed counts 1
	template<class S> void Describe(S& s) {
		s.Field("type", type);
		s.Field("mult", mult);
	}
};

struct ArmourClass {
	std::string name;
	std::vector<ArmourMult> mults;
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.List("mults", mults, [](auto& m, ArmourMult& item) { item.Describe(m); });
	}
};

// a unit whose first weapon gives every unit in its radius the full hit, with no fall-off
struct SplashCode {
	std::string code;                      // the unit's internal code (customParams.eco_code)
	template<class S> void Describe(S& s) { s.Field("code", code); }
};

// a death blast by unit code, for a unit whose def has no spc_death_blast parameter
struct BlastByCode {
	std::string code;
	float damage = 0;                      // DESIGN: unset; the data file gives the pack's death_blast
	float radius_mu = 0;                   // DESIGN: unset; map units
	std::string damage_type;               // empty: the default type
	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("damage", damage);
		s.Field("radius_mu", radius_mu);
		s.Field("damage_type", damage_type);
	}
};

// REQ SPC-014 v4 R6: a giant structure's two zones, by unit code
struct StructureBlast {
	std::string code;
	float inner_damage = 0;                // DESIGN: unset; the data file gives the values
	float inner_radius_mu = 0;             // DESIGN: unset
	float outer_damage = 0;                // DESIGN: unset
	float outer_radius_mu = 0;             // DESIGN: unset
	std::string damage_type;
	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("inner_damage", inner_damage);
		s.Field("inner_radius_mu", inner_radius_mu);
		s.Field("outer_damage", outer_damage);
		s.Field("outer_radius_mu", outer_radius_mu);
		s.Field("damage_type", damage_type);
	}
};

// one step of a split torpedo's decaying weave
struct WeaveStep {
	float amplitude_mu = 0;                // DESIGN: unset; map units
	template<class S> void Describe(S& s) { s.Field("amplitude_mu", amplitude_mu); }
};

struct DamageConfig {
	float elmos_per_mu = 8;                        // REQ OUT-0001: elmos in one map unit

	// cmb_damage
	float death_threshold = 2.0f;                  // PLACEHOLDER REQ CMB-008 R2: a unit dies when its health after a hit is below this
	std::string default_armour_class = "unassigned";   // REQ CMB-007 R1
	std::string default_damage_type = "kinetic";   // DESIGN: the type of a blast that states none
	std::vector<ArmourClass> armour;               // REQ CMB-007 v2; DESIGN: empty, the data file gives the table
	std::vector<SplashCode> splash_codes;          // REQ WINDOW-2026-11-12-C; DESIGN: empty, the data file names the units
	float splash_query_pad = 1;                    // DESIGN: elmos added to the area hit's cylinder query

	// the death delay (read by wrecks, cargo and death animation through DamageApi.h) and the death blast
	float death_delay_s = -1;                      // DESIGN: unset (below 0); a combat fixture sets it
	float land_death_to_wreck_s = 0.3f;            // PLACEHOLDER REQ CMB-008: 3 reference ticks
	float ship_death_to_wreck_s = 15;              // PLACEHOLDER REQ ECO-009 R6
	float death_blast_beat_s = 0;                  // DESIGN (2026-11-11): seconds from a kill to its death blast
	float blast_query_pad = 16;                    // DESIGN: elmos added to a blast's cylinder query
	std::string blast_weapon = "spc_death_blast";  // DESIGN: the weapon def a blast's damage is dealt as
	std::string scorch_weapon = "sp_death_unit";   // DESIGN: the weapon def of a crash's scorch mark
	float in_flight_height = 1;                    // DESIGN: elmos above ground or water at which an aircraft is in flight
	float crash_burst_lift = 4;                    // DESIGN: elmos the crash burst is raised (look only)
	float fall_gravity_mu_s2 = 5.0f;               // PLACEHOLDER REQ MOV-014: for a move type with no fall of its own
	float landing_bound_s = 3600;                  // DESIGN: loop bound of the worked-out fall; destroys nothing
	int giant_tier = 4;                            // REQ ECO-009 v1: the tier of a giant unit
	std::vector<BlastByCode> blasts;               // REQ CMB-008 R5, SPC-014; DESIGN: empty (def parameters win)
	std::vector<StructureBlast> structure_blasts;  // REQ SPC-014 v4 R6; DESIGN: empty

	// cmb_impact_patterns (REQ CMB-022 R4): a fraction or delay below 0 is unset
	float two_stage_first_fraction = -1;           // DESIGN: unset: the engine hit stays whole
	float two_stage_second_fraction = -1;          // DESIGN: unset: no second stage
	float two_stage_second_delay_s = -1;           // DESIGN: unset
	// REQ CMB-022 R5: the decaying weave of split torpedoes
	float weave_hold_s = 0;                        // DESIGN: unset
	float weave_start_amplitude_mu = 0;            // DESIGN: unset
	float weave_step_s = 0;                        // DESIGN: unset
	std::vector<WeaveStep> weave_steps;            // DESIGN: empty
	float weave_end_amplitude_mu = 0;              // DESIGN: unset
	float weave_end_frequency = 0;                 // DESIGN: unset; redraws a second
	float weave_frequency_per_amplitude = 0.1f;    // REQ CMB-022 R5: a step redraws this times its amplitude a second
	float weave_min_frequency = 1e-6f;             // DESIGN: floor of the divisor

	template<class S> void Describe(S& s) {
		s.Field("elmos_per_mu", elmos_per_mu);
		s.Field("death_threshold", death_threshold);
		s.Field("default_armour_class", default_armour_class);
		s.Field("default_damage_type", default_damage_type);
		s.List("armour", armour, [](auto& o, ArmourClass& item) { item.Describe(o); });
		s.List("splash_codes", splash_codes, [](auto& o, SplashCode& item) { item.Describe(o); });
		s.Field("splash_query_pad", splash_query_pad);
		s.Field("death_delay_s", death_delay_s);
		s.Field("land_death_to_wreck_s", land_death_to_wreck_s);
		s.Field("ship_death_to_wreck_s", ship_death_to_wreck_s);
		s.Field("death_blast_beat_s", death_blast_beat_s);
		s.Field("blast_query_pad", blast_query_pad);
		s.Field("blast_weapon", blast_weapon);
		s.Field("scorch_weapon", scorch_weapon);
		s.Field("in_flight_height", in_flight_height);
		s.Field("crash_burst_lift", crash_burst_lift);
		s.Field("fall_gravity_mu_s2", fall_gravity_mu_s2);
		s.Field("landing_bound_s", landing_bound_s);
		s.Field("giant_tier", giant_tier);
		s.List("blasts", blasts, [](auto& o, BlastByCode& item) { item.Describe(o); });
		s.List("structure_blasts", structure_blasts, [](auto& o, StructureBlast& item) { item.Describe(o); });
		s.Field("two_stage_first_fraction", two_stage_first_fraction);
		s.Field("two_stage_second_fraction", two_stage_second_fraction);
		s.Field("two_stage_second_delay_s", two_stage_second_delay_s);
		s.Field("weave_hold_s", weave_hold_s);
		s.Field("weave_start_amplitude_mu", weave_start_amplitude_mu);
		s.Field("weave_step_s", weave_step_s);
		s.List("weave_steps", weave_steps, [](auto& o, WeaveStep& item) { item.Describe(o); });
		s.Field("weave_end_amplitude_mu", weave_end_amplitude_mu);
		s.Field("weave_end_frequency", weave_end_frequency);
		s.Field("weave_frequency_per_amplitude", weave_frequency_per_amplitude);
		s.Field("weave_min_frequency", weave_min_frequency);
	}
};

} // namespace KT1::Core::Damage

#endif
