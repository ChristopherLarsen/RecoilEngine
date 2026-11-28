/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting core: the system's config, game/data/kt1/targeting.json (LUA-FREE H01, step S19). No engine include.
//
// Replaces the values cmb_targeting.lua reads (gamedata/cmb_values.lua section cmb, design/rules.lua) and the gun values
// of unit_cmd_gun.lua (gamedata/cmd_values.lua gun). The defaults are the design layer's; the development roster's values
// come from the data file only. The hull turn has its own config (Rules/HullTurn). Describe is a template over the config
// schema (Data/Schema.h). A test overlays another config at run time (TargetingBridge.h, Debug.SetRules).

#ifndef KT1_CORE_TARGETING_CONFIG_H
#define KT1_CORE_TARGETING_CONFIG_H

#include <string>
#include <vector>

namespace KT1::Core::Targeting {

struct ClassName {
	std::string name;
	template<class S> void Describe(S& s) { s.Field("name", name); }
};

// the command unit's gun (REQ CMB-004 v2, CMD-004), a chooser of its own
struct CommandGunConfig {
	std::string weapon = "hum_cmd_gun";    // DESIGN: the weapon def whose target this chooser picks
	float scan_interval_s = 0.6f;          // DESIGN (design command gun, 2026-10-29; REQ CMB-004 R1)
	float engage_inset_mu = 0.9f;          // DESIGN (2026-10-29; REQ CMB-004 R2)
	float tracking_factor = 1.1f;          // DESIGN (2026-10-29; REQ CMB-004 R3)
	float range_min_mu = 1.5f;             // DESIGN (2026-10-29)
	float range_max_mu = 26;               // DESIGN (2026-10-29): used when the weapon reports no range
	std::vector<ClassName> priorities = {  // PLACEHOLDER (REQ CMB-004 R4): the first class that has a target
		{"mobile"}, {"command"}, {"defence"}, {"any"}};
	template<class S> void Describe(S& s) {
		s.Field("weapon", weapon);
		s.Field("scan_interval_s", scan_interval_s);
		s.Field("engage_inset_mu", engage_inset_mu);
		s.Field("tracking_factor", tracking_factor);
		s.Field("range_min_mu", range_min_mu);
		s.Field("range_max_mu", range_max_mu);
		s.List("priorities", priorities, [](auto& o, ClassName& item) { item.Describe(o); });
	}
};

struct TargetingConfig {
	float elmos_per_mu = 8;                // REQ OUT-0001: elmos in one map unit
	float ref_tick_s = 0.1f;               // REQ SIM-RATE-001: one reference tick
	float engage_inset_mu = 0.5f;          // PLACEHOLDER REQ CMB-004 R2
	float tracking_factor_default = 1.1f;  // PLACEHOLDER REQ CMB-004 R3
	float scan_interval_default_s = 0.4f;  // PLACEHOLDER REQ CMB-004 R1
	int scan_extra_ticks = 2;              // PLACEHOLDER REQ SIM-RATE-001 R2: reference ticks added to every scan delay
	float retarget_skip_s = 5.0f;          // DESIGN (2026-11-05): a target dropped on a failed check is passed over this long
	float overkill_inflight_factor = 0;    // REQ CMB-024 v3: share of in-flight damage counted toward a kill; 0: off

	// periods of the per-step choice; the engine step (Lua/SupComTargeting.cpp) holds them as step constants today
	float hold_check_s = 0.1f;             // DESIGN: a held target is checked this often
	float hold_rest_s = 1.0f;              // DESIGN: a gun holding fire looks this often
	float line_check_s = 0.5f;             // DESIGN: a held target's line of fire is read this often
	float focus_check_s = 0.1f;            // DESIGN: the attack-move focus is tried this often

	// line of fire (REQ CMB-016 R2) and friends on the line (REQ CMB-003 R6)
	float line_sample_step = 8;            // DESIGN: elmos between ground samples
	float line_clearance = 1;              // DESIGN: elmos the ground may stand above the path
	bool line_friends = false;             // REQ CMB-003 R6: shots go through friendly units
	float line_friend_pad = 32;            // DESIGN: elmos around the line searched for friends

	float inflight_epsilon = 1e-6f;        // DESIGN: in-flight damage this small is none
	int scan_phase_factor = 7;             // DESIGN: spreads first scans: (unit * factor + weapon) mod the scan delay
	int fire_state_cmd = 39405;            // DESIGN: the game's fire-state command id (ord_orders.lua)
	std::string barrier_code_part = "WALL";    // DESIGN: a unit code holding this is of class "barrier"

	CommandGunConfig command_gun;

	template<class S> void Describe(S& s) {
		s.Field("elmos_per_mu", elmos_per_mu);
		s.Field("ref_tick_s", ref_tick_s);
		s.Field("engage_inset_mu", engage_inset_mu);
		s.Field("tracking_factor_default", tracking_factor_default);
		s.Field("scan_interval_default_s", scan_interval_default_s);
		s.Field("scan_extra_ticks", scan_extra_ticks);
		s.Field("retarget_skip_s", retarget_skip_s);
		s.Field("overkill_inflight_factor", overkill_inflight_factor);
		s.Field("hold_check_s", hold_check_s);
		s.Field("hold_rest_s", hold_rest_s);
		s.Field("line_check_s", line_check_s);
		s.Field("focus_check_s", focus_check_s);
		s.Field("line_sample_step", line_sample_step);
		s.Field("line_clearance", line_clearance);
		s.Field("line_friends", line_friends);
		s.Field("line_friend_pad", line_friend_pad);
		s.Field("inflight_epsilon", inflight_epsilon);
		s.Field("scan_phase_factor", scan_phase_factor);
		s.Field("fire_state_cmd", fire_state_cmd);
		s.Field("barrier_code_part", barrier_code_part);
		s.Object("command_gun", [this](auto& o) { command_gun.Describe(o); });
	}
};

} // namespace KT1::Core::Targeting

#endif
