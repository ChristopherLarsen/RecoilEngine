/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the family's one config (game archive data/kt1/orders.json), read by system Orders and handed to AMove
// and OrderRules (Rules/Orders/OrdersInternal.h). No engine include: Describe is a template over the config schema.
// Values: game-build game/data/kt1/orders.json (full labels there). The members hold the defaults, each with its source
// label; where the game has parity values, the default here is the design layer's and the parity value goes in the file.
// Seconds are seconds (steps come from Core::Rate); "_mu" is map units; "_elmo" engine units; speeds are per second.

#ifndef KT1_CORE_ORDERS_CONFIG_H
#define KT1_CORE_ORDERS_CONFIG_H

#include <string>
#include <string_view>
#include <vector>


namespace KT1::Core::Orders {

struct PatrolConfig {
	float patrol_check_s = 0.6f;         // PLACEHOLDER: design layer (REQ ORD-004 v1 R1)
	float fight_check_s = 0.1f;          // DESIGN: factory rally and plain fight check (no interval released)
	float repair_threshold = 0.75f;      // PLACEHOLDER: design layer, share of max health (REQ ORD-004 v1 R3)
	float repair_epsilon = 0.000001f;    // DESIGN: float slack on the threshold test

	template<class S> void Describe(S& s) {
		s.Field("patrol_check_s", patrol_check_s);
		s.Field("fight_check_s", fight_check_s);
		s.Field("repair_threshold", repair_threshold);
		s.Field("repair_epsilon", repair_epsilon);
	}
};

struct GuardRulesConfig {
	float check_s = 0.5f;                // PLACEHOLDER: REQ ORD-005 v2 R1 (value withheld)
	float half_radius_mu = 15.0f;        // DESIGN: half the guard radius, the part in use (REQ ORD-005 v2 R2, R5; withheld)
	float follow_lead_s = 1.0f;          // DESIGN: follow where the guarded unit will be
	float follow_behind_mu = 4.0f;       // DESIGN: stop this far short of that point
	float moving_speed_elmo_s = 0.3f;    // DESIGN: faster than this the guarded unit is moving
	float ring_settle_mu = 1.5f;         // DESIGN: settled within this of the ring place (FRM-005 ring)
	float ring_push_mu = 3.0f;           // DESIGN: a settled unit walks back when pushed this far
	float ring_on_way_mu = 0.5f;         // DESIGN: a move this near the place is already the way there

	template<class S> void Describe(S& s) {
		s.Field("check_s", check_s);
		s.Field("half_radius_mu", half_radius_mu);
		s.Field("follow_lead_s", follow_lead_s);
		s.Field("follow_behind_mu", follow_behind_mu);
		s.Field("moving_speed_elmo_s", moving_speed_elmo_s);
		s.Field("ring_settle_mu", ring_settle_mu);
		s.Field("ring_push_mu", ring_push_mu);
		s.Field("ring_on_way_mu", ring_on_way_mu);
	}
};

// a builder held out of reach walks to a free spot (engineering flow)
struct ReachConfig {
	float every_s = 0.5f;                // DESIGN: between checks per builder
	float walking_every_s = 0.1f;        // DESIGN: between checks while it walks to a spot
	float stall_s = 1.0f;                // DESIGN: without getting closer before it looks
	float progress_elmo = 2.0f;          // DESIGN: closer by this counts
	float inside_elmo = 6.0f;            // DESIGN: the spot lies this far inside the reach
	float min_out_elmo = 4.0f;           // DESIGN: and at least this far outside the target's radius
	float clear_elmo = 14.0f;            // DESIGN: free of other units around a spot
	int spots = 24;                      // DESIGN: candidates round the target
	int tries = 4;                       // DESIGN: spots per target, at most Core::Orders::REACH_TRIED
	float tried_elmo = 20.0f;            // DESIGN: a candidate this near a tried spot is skipped
	float walk_time_factor = 1.5f;       // DESIGN: of the walk's own time ...
	float walk_slack_s = 3.0f;           // DESIGN: ... plus this, then the walk is dropped
	float out_elmo = 24.0f;              // DESIGN: the way round keeps this far beyond the reach
	float arc_step_rad = 0.7853982f;     // DESIGN: largest angle between two points of the way round (45 degrees)
	float done_inside_elmo = 2.0f;       // DESIGN: a walk ends this far inside the reach
	float feature_radius_elmo = 16.0f;   // DESIGN: radius of a wreck whose def gives none
	float unit_radius_elmo = 16.0f;      // DESIGN: radius of a unit that reports none

	template<class S> void Describe(S& s) {
		s.Field("every_s", every_s);
		s.Field("walking_every_s", walking_every_s);
		s.Field("stall_s", stall_s);
		s.Field("progress_elmo", progress_elmo);
		s.Field("inside_elmo", inside_elmo);
		s.Field("min_out_elmo", min_out_elmo);
		s.Field("clear_elmo", clear_elmo);
		s.Field("spots", spots);
		s.Field("tries", tries);
		s.Field("tried_elmo", tried_elmo);
		s.Field("walk_time_factor", walk_time_factor);
		s.Field("walk_slack_s", walk_slack_s);
		s.Field("out_elmo", out_elmo);
		s.Field("arc_step_rad", arc_step_rad);
		s.Field("done_inside_elmo", done_inside_elmo);
		s.Field("feature_radius_elmo", feature_radius_elmo);
		s.Field("unit_radius_elmo", unit_radius_elmo);
	}
};

// move tracking: stale, give-up, redirect
struct TrackConfig {
	float period_s = 0.1f;               // DESIGN: between tracking passes (plus the step after a ground move is given)
	float give_up_s = 5.0f;              // PLACEHOLDER: REQ PTH-003 v2 plan.give_up_time (fixture value)
	float progress_min_elmo = 8.0f;      // REQ PTH-004 v3 stuck.progress_min, 0.5 cells
	float ahead_recent_s = 4.0f;         // REQ PTH-004 v3 stuck.time
	float ahead_elmo = 24.0f;            // DESIGN: the look ahead for a friend in the way ...
	float ahead_radius_elmo = 32.0f;     // DESIGN: ... and its radius
	float moving_speed_elmo_s = 3.0f;    // DESIGN: faster than this a unit moves
	float gain_mu = 1.0f;                // DESIGN: closer to the goal by this counts as gaining
	float stuck_still_s = 2.0f;          // DESIGN: standing this long ...
	float stuck_no_gain_s = 2.5f;        // DESIGN: ... without gaining this long is stuck
	float end_slack_elmo = 8.0f;         // DESIGN: arrival radius plus this is "at the goal"
	float stop_still_s = 1.0f;           // DESIGN: a redirected move stands this long at its point, then ends
	float redirect_near_elmo = 128.0f;   // DESIGN: a land verdict this near the goal is acted on at once
	int redirect_small_fill = 16;        // DESIGN: squares; a fill this small in a boxed unit is its own box
	float trust_still_s = 1.0f;          // DESIGN: standing this long, a far verdict is acted on
	float retry_window_s = 0.5f;         // DESIGN: a move gone this soon was dropped by the engine
	int retry_max = 3;                   // DESIGN: times a dropped redirect is given again
	float tie_tolerance_elmo = 1.0f;     // DESIGN: float slack on "as near the goal"
	int tie_points = 16;                 // DESIGN: points tried round the goal for the tie
	float refill_s = 1.0f;               // DESIGN: between reachability fills of a stalled move
	int fill_squares = 65536;            // DESIGN: the engine's default flood size (LuaPathFinder)
	float square_elmo = 8.0f;            // DESIGN: the engine's SQUARE_SIZE
	float beyond_margin_squares = 3.0f;  // DESIGN: added to the flood's side before halving
	float beyond_extra_squares = 4.0f;   // DESIGN: further margin of the "beyond the flood" reach
	int fill_budget_squares = 262144;    // DESIGN: order fills per pass (4 floods); 0: no budget
	bool skip_far_order_fill = true;     // DESIGN: no order fill for a goal beyond the flood (mod option ord_fill_far "1": off)
	bool skip_far_refill = true;         // DESIGN: nor an undecided refill there (mod option ord_refill_far "1": off)

	template<class S> void Describe(S& s) {
		s.Field("period_s", period_s);
		s.Field("give_up_s", give_up_s);
		s.Field("progress_min_elmo", progress_min_elmo);
		s.Field("ahead_recent_s", ahead_recent_s);
		s.Field("ahead_elmo", ahead_elmo);
		s.Field("ahead_radius_elmo", ahead_radius_elmo);
		s.Field("moving_speed_elmo_s", moving_speed_elmo_s);
		s.Field("gain_mu", gain_mu);
		s.Field("stuck_still_s", stuck_still_s);
		s.Field("stuck_no_gain_s", stuck_no_gain_s);
		s.Field("end_slack_elmo", end_slack_elmo);
		s.Field("stop_still_s", stop_still_s);
		s.Field("redirect_near_elmo", redirect_near_elmo);
		s.Field("redirect_small_fill", redirect_small_fill);
		s.Field("trust_still_s", trust_still_s);
		s.Field("retry_window_s", retry_window_s);
		s.Field("retry_max", retry_max);
		s.Field("tie_tolerance_elmo", tie_tolerance_elmo);
		s.Field("tie_points", tie_points);
		s.Field("refill_s", refill_s);
		s.Field("fill_squares", fill_squares);
		s.Field("square_elmo", square_elmo);
		s.Field("beyond_margin_squares", beyond_margin_squares);
		s.Field("beyond_extra_squares", beyond_extra_squares);
		s.Field("fill_budget_squares", fill_budget_squares);
		s.Field("skip_far_order_fill", skip_far_order_fill);
		s.Field("skip_far_refill", skip_far_refill);
	}
};

// when a stalled move is filled again
struct RefillConfig {
	bool schedule = true;                // DESIGN: false (mod option ord_reach_sched "0"): every refill_s
	float cell_elmo = 512.0f;            // DESIGN: the blocking-change grid
	float near_elmo = 512.0f;            // DESIGN: a change this near the goal or the unit counts
	float moved_elmo = 512.0f;           // DESIGN: an undecided fill this far behind
	float max_s = 10.0f;                 // DESIGN: longest without a fill

	template<class S> void Describe(S& s) {
		s.Field("schedule", schedule);
		s.Field("cell_elmo", cell_elmo);
		s.Field("near_elmo", near_elmo);
		s.Field("moved_elmo", moved_elmo);
		s.Field("max_s", max_s);
	}
};

// enclosed units and structure footprints (REQ PTH-004 v3 R5)
struct BoxedConfig {
	int directions = 8;                  // DESIGN: probes round the unit
	float beyond_elmo = 8.0f;            // DESIGN: one square beyond the footprint
	float structure_radius_elmo = 96.0f; // DESIGN: structures looked at per probe
	float factory_radius_elmo = 160.0f;  // DESIGN: factories whose yard the unit may stand in
	float inside_radius_elmo = 64.0f;    // DESIGN: structures the unit may stand inside
	float mid_slack_elmo = 128.0f;       // DESIGN: a mid point may lie this far off its grid cell
	float cell_elmo = 256.0f;            // DESIGN: the grid of immobile units

	template<class S> void Describe(S& s) {
		s.Field("directions", directions);
		s.Field("beyond_elmo", beyond_elmo);
		s.Field("structure_radius_elmo", structure_radius_elmo);
		s.Field("factory_radius_elmo", factory_radius_elmo);
		s.Field("inside_radius_elmo", inside_radius_elmo);
		s.Field("mid_slack_elmo", mid_slack_elmo);
		s.Field("cell_elmo", cell_elmo);
	}
};

struct WaypointUnit {
	std::string unit;                    // unit def name
	float advance_mu = 0.0f;             // DESIGN: 0 = the arrival radius; REQ MOV-020 v2 values in orders.json

	template<class S> void Describe(S& s) {
		s.Field("unit", unit);
		s.Field("advance_mu", advance_mu);
	}
};

// the waypoint step (ORD-001 R2, MOV-020)
struct WaypointConfig {
	float no_progress_s = 3.0f;          // DESIGN: without coming nearer before the step ends
	float progress_elmo = 0.5f;          // DESIGN: nearer by this counts
	float near_radii = 4.0f;             // DESIGN: within this many radii the first time limit applies ...
	float anywhere_factor = 2.0f;        // DESIGN: ... and this factor of it anywhere
	float rest_speed_elmo_s = 0.3f;      // DESIGN: slower than this is at rest
	float rest_s = 1.0f;                 // DESIGN: at rest this long ends the step
	float goal_floor_share = 0.4f;       // DESIGN: the move goal radius is at least this share of the radius ...
	float goal_share = 0.8f;             // DESIGN: ... times this
	bool step_all = false;               // DESIGN: mod option ord_step_all (diagnosis): every unit steps to the ordered point
	std::vector<WaypointUnit> units;     // per-unit advance radius

	template<class S> void Describe(S& s) {
		s.Field("no_progress_s", no_progress_s);
		s.Field("progress_elmo", progress_elmo);
		s.Field("near_radii", near_radii);
		s.Field("anywhere_factor", anywhere_factor);
		s.Field("rest_speed_elmo_s", rest_speed_elmo_s);
		s.Field("rest_s", rest_s);
		s.Field("goal_floor_share", goal_floor_share);
		s.Field("goal_share", goal_share);
		s.Field("step_all", step_all);
		s.List("units", units, [](auto& o, WaypointUnit& unit) { unit.Describe(o); });
	}
};

// one transport code's own times; a value under 0 takes TransportConfig's
struct TransportCode {
	std::string code;                    // customParams eco_code
	float load_s = -1.0f;                // DESIGN: -1 = the common value
	float unload_s = -1.0f;              // DESIGN: -1 = the common value
	float drop_accuracy_mu = -1.0f;      // DESIGN: -1 = the common value
	float unload_interval_s = -1.0f;     // DESIGN: -1 = none: each unit takes the unload time

	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("load_s", load_s);
		s.Field("unload_s", unload_s);
		s.Field("drop_accuracy_mu", drop_accuracy_mu);
		s.Field("unload_interval_s", unload_interval_s);
	}
};

struct TransportConfig {
	float load_s = 2.5f;                 // PLACEHOLDER: REQ ORD-007 v2 R1 (per transport, not released)
	float unload_s = 2.8f;               // PLACEHOLDER: REQ ORD-007 v2 R2
	float drop_accuracy_mu = 1.2f;       // PLACEHOLDER: REQ ORD-007 v2 R2
	float drop_hold_s = 3.0f;            // DESIGN: longest wait for the set-down point
	bool reverse_order = false;          // DESIGN: set down in reverse loading order (off: all at once)
	float reverse_hold_s = 1.0f;         // DESIGN: longest refusal under reverse_order
	float ask_gap_s = 1.0f;              // DESIGN: a gate not asked for longer than this restarts
	float slow_update_s = 0.5f;          // DESIGN: the engine's command AI asks once per this; a gate from the order opens half early
	std::vector<TransportCode> codes;

	template<class S> void Describe(S& s) {
		s.Field("load_s", load_s);
		s.Field("unload_s", unload_s);
		s.Field("drop_accuracy_mu", drop_accuracy_mu);
		s.Field("drop_hold_s", drop_hold_s);
		s.Field("reverse_order", reverse_order);
		s.Field("reverse_hold_s", reverse_hold_s);
		s.Field("ask_gap_s", ask_gap_s);
		s.Field("slow_update_s", slow_update_s);
		s.List("codes", codes, [](auto& o, TransportCode& code) { code.Describe(o); });
	}
};

struct FerryConfig {
	float period_s = 0.5f;               // DESIGN: between looks at a route
	float leave_delay_s = 3.0f;          // DESIGN: after the last arrival, with a unit aboard
	float gather_mu = 60.0f;             // DESIGN: guards of the marker within this are waiting
	float drop_radius_mu = 6.0f;         // PLACEHOLDER: REQ ORD-007 v2 R5, set down round the end
	float return_mu = 4.0f;              // DESIGN: farther than this from the marker it flies back

	template<class S> void Describe(S& s) {
		s.Field("period_s", period_s);
		s.Field("leave_delay_s", leave_delay_s);
		s.Field("gather_mu", gather_mu);
		s.Field("drop_radius_mu", drop_radius_mu);
		s.Field("return_mu", return_mu);
	}
};

struct FireConfig {
	std::string structure = "ground";    // REQ ORD-010 v6 R1: cannot move
	std::string experimental = "ground"; // REQ ORD-010 v6 R1
	std::string command_unit = "ground"; // REQ ORD-010 v6 R1
	std::string other = "free";          // REQ ORD-010 v6 R1
	int experimental_tier = 4;           // REQ ORD-010 v6 R1: tier 4 and up
	int engine_hold = 0;                 // DESIGN: the engine fire state each game state runs as
	int engine_free = 2;                 // DESIGN
	int engine_ground = 2;               // DESIGN

	template<class S> void Describe(S& s) {
		s.Field("structure", structure);
		s.Field("experimental", experimental);
		s.Field("command_unit", command_unit);
		s.Field("other", other);
		s.Field("experimental_tier", experimental_tier);
		s.Field("engine_hold", engine_hold);
		s.Field("engine_free", engine_free);
		s.Field("engine_ground", engine_ground);
	}
};

// the attack-move task (system AMove): its own file, data/kt1/amove.json
struct AMoveConfig {
	float resume_tick_s = 0.2f;          // DESIGN: upkeep tick; the pick of a unit with no scanned weapon
	float guard_factor = 1.5f;           // DESIGN: strip half width = this x the primary weapon's range
	float stop_share = 0.9f;             // DESIGN: close to this share of the weapon's reach
	float hold_goal_elmo = 1.0f;         // DESIGN: goal radius that stops a unit where it is
	float back_goal_share = 0.8f;        // DESIGN: share of the arrival radius for the way back
	float back_stall_s = 2.0f;           // DESIGN: without headway on the way back, or near the goal
	float headway_elmo2 = 1.0f;          // DESIGN: squared distance gained that counts as headway
	float group_near_radii = 4.0f;       // DESIGN: arrival radii within which no headway counts as arrived
	float air_arrive_factor = 2.0f;      // DESIGN: aircraft arrive within this many arrival radii
	float seen_reuse_s = 0.13333334f;    // DESIGN: the seen-enemy list is rebuilt this often (Lua: 4 frames)
	float seen_cell_elmo = 256.0f;       // DESIGN: groups the search only
	float circle_spacing = 0.8f;         // DESIGN: search circles along the route, share of the strip half width
	float circle_radius = 1.42f;         // DESIGN: their radius, share of the strip half width
	int direct_per_circle = 4;           // DESIGN: with at most this many enemies per circle, test each directly

	template<class S> void Describe(S& s) {
		s.Field("resume_tick_s", resume_tick_s);
		s.Field("guard_factor", guard_factor);
		s.Field("stop_share", stop_share);
		s.Field("hold_goal_elmo", hold_goal_elmo);
		s.Field("back_goal_share", back_goal_share);
		s.Field("back_stall_s", back_stall_s);
		s.Field("headway_elmo2", headway_elmo2);
		s.Field("group_near_radii", group_near_radii);
		s.Field("air_arrive_factor", air_arrive_factor);
		s.Field("seen_reuse_s", seen_reuse_s);
		s.Field("seen_cell_elmo", seen_cell_elmo);
		s.Field("circle_spacing", circle_spacing);
		s.Field("circle_radius", circle_radius);
		s.Field("direct_per_circle", direct_per_circle);
	}
};

// the accepted-order set of one eco code (ORD-010 R6), for defs without customParams ord_accepts
struct AcceptRow {
	std::string code;                    // customParams eco_code
	std::string words;                   // comma list: "move,attack,..." (DefFacts.h AcceptWord)

	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("words", words);
	}
};

// one value a fixture overlays (SetValue's keys)
struct FixtureValue {
	std::string key;
	float value = 0.0f;                  // DESIGN: no value

	template<class S> void Describe(S& s) {
		s.Field("key", key);
		s.Field("value", value);
	}
};

// a requirement's synthetic fixture (GG.Ord.UseFixture; engine cases)
struct Fixture {
	std::string name;                    // "SF-ORD-1"
	std::vector<FixtureValue> values;

	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.List("values", values, [](auto& o, FixtureValue& v) { v.Describe(o); });
	}
};

struct OrdersConfig {
	float arrival_radius_mu = 1.2f;      // DESIGN: design layer move.arrival_radius (REQ MOV-003 v4 R3)
	float elmos_per_mu = 8.0f;           // DESIGN: 1 map unit = 1 map square = 8 elmos
	int queue_min = 50;                  // PLACEHOLDER: design layer (REQ ORD-001 v1 R3); no code reads it, cases do
	float selfdestruct_delay_s = 5.0f;   // PLACEHOLDER: REQ ORD-010 v6 R4
	float safety_net_s = 0.27f;          // DESIGN: between walks that add queues no command call-in reported
	PatrolConfig patrol;
	GuardRulesConfig guard;
	ReachConfig reach;
	TrackConfig track;
	RefillConfig refill;
	BoxedConfig boxed;
	WaypointConfig waypoint;
	TransportConfig transport;
	FerryConfig ferry;
	FireConfig fire;
	std::vector<AcceptRow> accepts;
	std::vector<Fixture> fixtures;

	template<class S> void Describe(S& s) {
		s.Field("arrival_radius_mu", arrival_radius_mu);
		s.Field("elmos_per_mu", elmos_per_mu);
		s.Field("queue_min", queue_min);
		s.Field("selfdestruct_delay_s", selfdestruct_delay_s);
		s.Field("safety_net_s", safety_net_s);
		s.Object("patrol", [this](auto& o) { patrol.Describe(o); });
		s.Object("guard", [this](auto& o) { guard.Describe(o); });
		s.Object("reach", [this](auto& o) { reach.Describe(o); });
		s.Object("track", [this](auto& o) { track.Describe(o); });
		s.Object("refill", [this](auto& o) { refill.Describe(o); });
		s.Object("boxed", [this](auto& o) { boxed.Describe(o); });
		s.Object("waypoint", [this](auto& o) { waypoint.Describe(o); });
		s.Object("transport", [this](auto& o) { transport.Describe(o); });
		s.Object("ferry", [this](auto& o) { ferry.Describe(o); });
		s.Object("fire", [this](auto& o) { fire.Describe(o); });
		s.List("accepts", accepts, [](auto& o, AcceptRow& row) { row.Describe(o); });
		s.List("fixtures", fixtures, [](auto& o, Fixture& f) { f.Describe(o); });
	}

	float ArrivalElmo() const { return arrival_radius_mu * elmos_per_mu; }
};

// Sets one value by the Lua's key (ord_values.lua V.ord: "queue_min", "patrol_check_interval", "patrol_repair_threshold",
// "guard_check_interval", "guard_radius", "load_time", "unload_time", "drop_accuracy", "selfdestruct_delay",
// "give_up_time", "fight_check_interval", "fight_resume_tick", "fight_guard_factor", "fight_stop_share").
// "guard_radius" is the whole radius: half of it goes into guard.half_radius_mu.
// false: unknown key.
bool SetValue(OrdersConfig& config, std::string_view key, float value);
// base with the named fixture's values over it; an unknown or empty name: base
OrdersConfig WithFixture(const OrdersConfig& base, std::string_view name);
// the attack-move values a fixture of orders.json overlays (keys fight_resume_tick, fight_guard_factor, fight_stop_share)
bool SetValue(AMoveConfig& config, std::string_view key, float value);
AMoveConfig WithFixture(const AMoveConfig& base, const OrdersConfig& orders, std::string_view name);

} // namespace KT1::Core::Orders

#endif
