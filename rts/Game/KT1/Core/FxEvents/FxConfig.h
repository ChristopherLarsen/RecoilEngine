/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect system's tunables (game/data/kt1/fxevents.json). It holds the values of fx_core,
// fx_build_sets, fx_death_sets, fx_wreck_physics, fx_script_relay, fx_events, gamedata/fx_budget.lua and the synced halves
// of gfx_scorch, fx_blasts, gfx_criteria_fx, gfx_fog_death_puff, fx_depth and fx_strategic. No engine include.

#ifndef KT1_FX_CONFIG_H
#define KT1_FX_CONFIG_H

#include <string>
#include <vector>

#include "Game/KT1/Core/Common/EventMapRow.h"
#include "Game/KT1/Core/FxEvents/FxBudget.h"
#include "Game/KT1/Core/FxEvents/FxBuildWork.h"
#include "Game/KT1/Core/FxEvents/FxCriteria.h"
#include "Game/KT1/Core/FxEvents/FxDeath.h"
#include "Game/KT1/Core/FxEvents/FxStrategic.h"
#include "Game/KT1/Core/FxEvents/FxWreck.h"

namespace KT1::Core::Fx {

// One row of the event map: a sim event raises a display event (S30). A row turns the pair on; which units or weapons take
// part is also the event's own rule (a depth hit needs a depth weapon). Triggers that are not sim events are functions.
struct FxEventRow : EventMapRow {
	std::string display_event;        // the struct's name in FxEvent.h
	template<class S> void Describe(S& s) {
		EventMapRow::Describe(s);
		s.Field("display_event", display_event);
	}
};

struct FxCoreConfig {
	float scale = 1.0f;               // DESIGN: the effect option; the mod option fx_scale overrides it
	float scale_send_s = 1.0f;        // DESIGN: the Scale event's period
	float confirm_gap_s = 0.2f;       // PLACEHOLDER: at most one unit-hit confirmation per unit in this time
	float unit_hit_sphere = 12.0f;    // DESIGN: a unit this near the explosion takes the unit set, elmos
	float confirm_radius[3] = {16.0f, 32.0f, 56.0f};   // PLACEHOLDER: unit radius up to these is tier S, M, L; else XL
	float confirm_surface = 0.8f;     // DESIGN: the confirmation sits this share of the radius towards the attacker
	float confirm_dir_y = 0.3f;       // DESIGN: its direction's upward part
	float gate_radius_unit = 8.0f;    // DESIGN: a gated scorch row's radius is in map units, elmos each
	float gate_land_slack = 16.0f;    // DESIGN: an explosion higher than radius + this above the ground is an air burst
	float gate_size_factor = 2.0f;    // DESIGN: a gated mark's size is the row's value x the radius x this
	bool criteria_owns_hits = true;   // DESIGN: the criteria flash owns every hit's flash (mod option crit_fx_off "1": false)
	template<class S> void Describe(S& s) {
		s.Field("scale", scale);
		s.Field("scale_send_s", scale_send_s);
		s.Field("confirm_gap_s", confirm_gap_s);
		s.Field("unit_hit_sphere", unit_hit_sphere);
		s.Field("confirm_radius", confirm_radius);
		s.Field("confirm_surface", confirm_surface);
		s.Field("confirm_dir_y", confirm_dir_y);
		s.Field("gate_radius_unit", gate_radius_unit);
		s.Field("gate_land_slack", gate_land_slack);
		s.Field("gate_size_factor", gate_size_factor);
		s.Field("criteria_owns_hits", criteria_owns_hits);
	}
};

struct FxRelayConfig {
	int ability_events_a_step = 32;   // DESIGN: teleport, crash, remnant and sacrifice relays; the rest are dropped
	int script_blasts_a_step = 16;    // DESIGN
	int scorch_a_step = 24;           // DESIGN: explosion scorch requests
	float scorch_min_area = 8.0f;     // DESIGN: explosion diameter below which no mark is asked for, elmos (beams: never)
	int blasts_a_step = 64;           // DESIGN: classified blasts
	template<class S> void Describe(S& s) {
		s.Field("ability_events_a_step", ability_events_a_step);
		s.Field("script_blasts_a_step", script_blasts_a_step);
		s.Field("scorch_a_step", scorch_a_step);
		s.Field("scorch_min_area", scorch_min_area);
		s.Field("blasts_a_step", blasts_a_step);
	}
};

struct BlastCoreRow {
	std::string blast_class;          // DATA: small_arms, tank_shell, artillery, building_death, experimental_death
	std::string effect;               // DATA: the core effect (gamedata/fx_blast.lua CORE)
	template<class S> void Describe(S& s) {
		s.Field("blast_class", blast_class);
		s.Field("effect", effect);
	}
};

struct BlastSyncConfig {
	std::vector<BlastCoreRow> cores;  // DATA
	std::string noflash_suffix = "_noflash";   // DESIGN: the core without its flash, while the criteria flash owns hits
	float beam_gap_s = 0.333f;        // DESIGN: a beam's blast at most once per weapon and cell in this time
	float beam_cell = 64.0f;          // DESIGN: elmos
	float giant_ring = 70.0f;         // DESIGN: the scorch marks this far round an experimental's death, elmos
	float giant_lift = 10.0f;         // DESIGN: its blast sits this far above the unit's position
	std::string giant_tier = "4";     // REQ ECO-009: the eco_tier of a giant unit
	std::string giant_core = "sp_blast_experimental_death";   // DESIGN
	std::string giant_scorch_weapon = "sp_death_structure";   // DESIGN: whose mark the ring takes
	template<class S> void Describe(S& s) {
		s.List("cores", cores, [](auto& o, BlastCoreRow& r) { r.Describe(o); });
		s.Field("noflash_suffix", noflash_suffix);
		s.Field("beam_gap_s", beam_gap_s);
		s.Field("beam_cell", beam_cell);
		s.Field("giant_ring", giant_ring);
		s.Field("giant_lift", giant_lift);
		s.Field("giant_tier", giant_tier);
		s.Field("giant_core", giant_core);
		s.Field("giant_scorch_weapon", giant_scorch_weapon);
	}
};

constexpr int32_t BUILD_SLOTS = 4;    // PARITY: G0-WVFXBLD faction slots

struct BuildSlotRow {
	int slot = 0;                     // DATA: 1 .. BUILD_SLOTS
	std::string effects;              // DATA: the on-target set effects, "name+name"
	float period_s = 0.2f;            // DATA: the slot's update period (default: a slot the roster gives none)
	bool pool = false;                // DATA: made only until the pool starts to shrink (slot 2)
	template<class S> void Describe(S& s) {
		s.Field("slot", slot);
		s.Field("effects", effects);
		s.Field("period_s", period_s);
		s.Field("pool", pool);
	}
};

struct BuildSetConfig {
	bool enabled = true;              // DESIGN: the Lua's mod option fx_bld_sets
	std::vector<BuildSlotRow> slots;  // DATA
	float pool_shrink_progress = 0.0f;   // DATA: build progress at which the pool starts to shrink
	template<class S> void Describe(S& s) {
		s.Field("enabled", enabled);
		s.List("slots", slots, [](auto& o, BuildSlotRow& r) { r.Describe(o); });
		s.Field("pool_shrink_progress", pool_shrink_progress);
	}
};

struct FxEventsConfig {
	std::vector<FxEventRow> events;   // DATA: the event map
	BudgetConfig budget;
	FxCoreConfig core;
	FxRelayConfig relay;
	BlastSyncConfig blast;
	BuildSetConfig build;
	DeathConfig death;
	WreckConfig wreck;
	CriteriaTimingConfig criteria;
	StrategicConfig strategic;
	BuildWorkConfig work;
	template<class S> void Describe(S& s) {
		s.List("events", events, [](auto& o, FxEventRow& r) { r.Describe(o); });
		s.Object("budget", [&](auto& o) { budget.Describe(o); });
		s.Object("core", [&](auto& o) { core.Describe(o); });
		s.Object("relay", [&](auto& o) { relay.Describe(o); });
		s.Object("blast", [&](auto& o) { blast.Describe(o); });
		s.Object("build", [&](auto& o) { build.Describe(o); });
		s.Object("death", [&](auto& o) { death.Describe(o); });
		s.Object("wreck", [&](auto& o) { wreck.Describe(o); });
		s.Object("criteria", [&](auto& o) { criteria.Describe(o); });
		s.Object("strategic", [&](auto& o) { strategic.Describe(o); });
		s.Object("work", [&](auto& o) { work.Describe(o); });
	}
};

} // namespace KT1::Core::Fx

#endif
