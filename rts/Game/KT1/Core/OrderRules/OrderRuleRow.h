/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: one row of the table that answers the order Allow chain (ORDERS plan section 0: "rows of command,
// unit class, condition and action"; actions refuse, refuse and re-issue a rewritten order, clamp; a rule that fits no
// row is a named validator of its own: action "native"). It replaces the Allow handlers of ord_orders.lua, ord_refuse.lua
// and the nine guard gadgets. No engine include.
//
//   {"name": "beyond_map", "gadget": "ord_orders", "call_in": "command", "command": "move", "unit_class": "not_air",
//    "condition": "beyond_map", "action": "refuse", "reason": "beyond_map", "internal_too": false}
//
// Rows are asked in list order, which is the replaced gadgets' layer order; consecutive rows with one "gadget" are one
// validator (check mode compares it with that gadget's answer). The first row that refuses ends the call. A row is skipped
// for the order it is itself re-issuing (the Lua's per-gadget "passing" flags); every other row sees that order.

#ifndef KT1_CORE_ORDER_RULES_ORDER_RULE_ROW_H
#define KT1_CORE_ORDER_RULES_ORDER_RULE_ROW_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Orders/DefFacts.h"
#include "Game/KT1/Core/Orders/OrderTypes.h"

namespace KT1::Core::OrderRules {

enum class CallIn : uint8_t { Command, BuildStep, FeatureStep, TransportLoad, TransportUnload, Count };

// Refuse: the answer is no (with the row's reason, if it has one). Reissue: refuse and give the condition's rewritten
// order. Clamp: refuse and give the order with its values brought into range. Native: the condition's own validator does
// its work and gives the answer (it may allow).
enum class Action : uint8_t { Refuse, Reissue, Clamp, Native, Count };

// What a row tests; the adapter gathers the facts, GuardLogic.h and Core/Orders hold the pure tests.
// Config names are the lower_snake_case of these (comments: the replaced rule).
enum class Condition : uint8_t {
	Always,
	// ord_orders.lua AllowCommand
	BeyondMap,             // a move point off the map, unit cannot fly
	EnemyUnseen,           // a unit target of another side, neither in sight nor on radar
	EnemyTarget,           // guard of a unit of another side
	UnreclaimableTarget,   // reclaim of itself or of a command or support unit (ECO-019 R1, R3)
	AttackMovePoint,       // engine fight to a point on a mobile non-factory: re-issued as attack-move, unarmed as move
	RecordGiven,           // native, allows: repair and reclaim orders given (the assist step test reads them)
	NotAccepted,           // ORD-010 R6: the unit's pack accepted-order set lacks the order
	EngineFireState,       // native, refuses: an engine fire-state order sets the game state
	MoveStateFromPlayer,   // a move-state order not from synced code
	GameFireState,         // native, refuses: sets or cycles the game fire state
	SelfDestruct,          // native, refuses: toggles the countdown
	CancelsSelfDestruct,   // native, allows: a stop ends the countdown
	DiveNotSubmarine,      // dive order on a unit that cannot dive
	DiveToggle,            // native, refuses: dive or surface
	Carried,               // a carried unit's move, fight, patrol or guard (internal ones are refused silently)
	FerryNotTransport,     // ferry order without transport room or six parameters
	FerryRoute,            // native, refuses: sets the route
	EndsFerry,             // native, allows: a load, stop, move or pickup order ends the route
	// ord_orders.lua unit call-ins
	LivingReclaim,         // native, allows (build step): remember a reclaim step on a unit that may not take it
	TransportGate,         // native: the load and set-down clocks (ORD-007, ORD-011)
	// ord_edge.lua
	AreaReclaim,           // area reclaim becomes one reclaim per wreck, nearest first
	NotInDefAccepts,       // R6: the def's spc_accepts set lacks the kind
	// ord_known_target.lua
	IndirectKnownTarget,   // indirect fire on a radar or remembered target in range: attack its position
	// mov_short_order.lua
	ShortMove,             // a move point nearer than the minimum: re-issued as stop
	// mov_shift_keep.lua
	NearQueuedMove,        // a queued move near a queued one: appended with an insert
	// mov_goal_cell.lua
	GoalCell,              // the stopping point in the goal's cell
	// mov_air_unload_gap.lua
	OccupiedUnloadSpot,    // an unload onto a held spot: set down short of it
	// mov_air_idle_water.lua
	HeldIdleMode,          // native, refuses: the player's idle mode is kept for later
	// ord_reclaim_still.lua
	ReclaimWhileMoving,    // native (build and feature steps, part < 0): reclaim only standing still
	Count
};

enum class UnitClass : uint8_t {
	Any,
	NotAir,
	Mobile,           // can move
	Ground,           // can move, cannot fly, not a building, speed over 0
	GroundNotCommand, // Ground without command units (mov_goal_cell)
	MobileNonFactory,
	Submarine,
	Transport,
	CanReclaim,
	IndirectGround,   // cannot fly and has an indirect-fire weapon
	GapTransport,     // a transport with both unload gaps
	IdleLander,       // an aircraft that lands when idle
	Count
};
bool InClass(const Orders::DefFacts& def, UnitClass unitClass);

struct OrderRuleRow {
	std::string name;                   // names the row in logs and traces
	std::string gadget;                 // the replaced gadget (file name without .lua): groups rows into a validator
	std::string call_in = "command";    // "command", "build_step", "feature_step", "transport_load", "transport_unload"
	std::string command;                // comma list of command names (OrderTypes.h); empty: any
	std::string unit_class = "any";     // the UnitClass of the unit asked about
	std::string condition = "always";
	std::string action = "refuse";      // "refuse", "refuse_and_reissue", "clamp", "native"
	std::string reason;                 // rules param cmb_refused (hud.refused.<reason>); empty: silent
	bool internal_too = false;          // false: internal orders pass the row

	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("gadget", gadget);
		s.Field("call_in", call_in);
		s.Field("command", command);
		s.Field("unit_class", unit_class);
		s.Field("condition", condition);
		s.Field("action", action);
		s.Field("reason", reason);
		s.Field("internal_too", internal_too);
	}
};

// a row ready to ask: the strings resolved once at Start
struct CompiledRow {
	CallIn callIn = CallIn::Command;
	Orders::CommandMask commands = 0;   // 0: any
	UnitClass unitClass = UnitClass::Any;
	Condition condition = Condition::Always;
	Action action = Action::Refuse;
	bool internalToo = false;
	int32_t source = -1;                // index in the config's rows (name, gadget, reason; the reissue guard bit)
};
bool ParseCallIn(std::string_view name, CallIn& out);
bool ParseAction(std::string_view name, Action& out);
bool ParseCondition(std::string_view name, Condition& out);
bool ParseUnitClass(std::string_view name, UnitClass& out);
bool Compile(const OrderRuleRow& row, int32_t source, CompiledRow& out);   // false: an unknown name
// the row's command, class and internal filters (not its condition)
bool Matches(const CompiledRow& row, Orders::CommandKind kind, const Orders::DefFacts& def, bool internal);

constexpr size_t MAX_ROWS = 64;   // DESIGN: the reissue guard is one bit per row

// the numbers of the guard rules
struct GuardValues {
	float cancel_distance_elmo = 17.0f;     // DESIGN: the engine's COMMAND_CANCEL_DIST (mov_shift_keep), not a game value
	float short_order_mu = 0.0f;            // DESIGN: no rule without a value; REQ MOV-019 v3 value in orders.json
	float goal_cell_mu = 1.25f;             // DESIGN: design layer move.goal_cell (REQ MOV-003 v4 R3)
	float rest_short_mu = 0.32f;            // DESIGN: design layer move.rest_short (REQ MOV-003 v4 R3)
	float goal_same_elmo = 0.001f;          // DESIGN: a rest point this near the ordered one is the order itself
	int goal_records = 32;                  // DESIGN: ordered points kept per unit (REQ MOV-020 v2 R2)
	float reclaim_still_elmo_s = 1.5f;      // DESIGN: at or under this speed a builder counts as stopped
	float reclaim_latency_s = 0.1f;         // PARITY: stopped in reach before reclaim starts (reference fixture, 3 reference frames)
	float reclaim_fresh_gap_s = 0.0f;       // DESIGN: 0: a step not asked on the step before restarts the latency
	float idle_water_check_s = 0.16666667f; // DESIGN: seconds between the over-water looks (Lua: 5 frames)
	int transfer_trim_tries = 5;            // DESIGN: steps a transferred factory's queue trim is repeated
	int transfer_fallback_fire = 2;         // DESIGN: the engine's free fire when the def gives no fire state
	float unload_query_elmo = 128.0f;       // DESIGN: radius searched for a unit holding an unload spot
	float unload_same_elmo = 0.000001f;     // DESIGN: a transport this near the spot is on it

	template<class S> void Describe(S& s) {
		s.Field("cancel_distance_elmo", cancel_distance_elmo);
		s.Field("short_order_mu", short_order_mu);
		s.Field("goal_cell_mu", goal_cell_mu);
		s.Field("rest_short_mu", rest_short_mu);
		s.Field("goal_same_elmo", goal_same_elmo);
		s.Field("goal_records", goal_records);
		s.Field("reclaim_still_elmo_s", reclaim_still_elmo_s);
		s.Field("reclaim_latency_s", reclaim_latency_s);
		s.Field("reclaim_fresh_gap_s", reclaim_fresh_gap_s);
		s.Field("idle_water_check_s", idle_water_check_s);
		s.Field("transfer_trim_tries", transfer_trim_tries);
		s.Field("transfer_fallback_fire", transfer_fallback_fire);
		s.Field("unload_query_elmo", unload_query_elmo);
		s.Field("unload_same_elmo", unload_same_elmo);
	}
};

// the "rules" object of data/kt1/orders.json
struct RulesConfig {
	std::vector<OrderRuleRow> rows;   // in orders.json (S11, S14); empty: no rule
	GuardValues values;
	bool air_idle_water = true;       // DESIGN: mod option air_idle_water "0" turns the rule off too

	template<class S> void Describe(S& s) {
		s.List("rows", rows, [](auto& o, OrderRuleRow& row) { row.Describe(o); });
		s.Object("values", [this](auto& o) { values.Describe(o); });
		s.Field("air_idle_water", air_idle_water);
	}
};

} // namespace KT1::Core::OrderRules

#endif
