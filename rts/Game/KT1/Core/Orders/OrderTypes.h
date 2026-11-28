/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: what the family's three systems (Orders, AMove, OrderRules) share about commands: the command kinds,
// the game's own command ids, one queued order, one order the family gives, and the small enums. No engine include.
//
// Engine command ids stay in the engine: the adapter maps an engine id to a CommandKind (a negative id is Build, an id
// nobody names is Other). The game's own ids are here (CustomKind). No name here may be an engine CMD_* macro.

#ifndef KT1_CORE_ORDERS_ORDER_TYPES_H
#define KT1_CORE_ORDERS_ORDER_TYPES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace KT1::Core::Orders {

using UnitId = int32_t;
using Step = int32_t;              // a sim step number
constexpr UnitId NO_UNIT = -1;
constexpr Step NO_STEP = -1;       // never, or not set (the Lua's nil)

// The game's own command ids: the interface and the Lua of other families use the same numbers.
constexpr int32_t CUSTOM_WAYPOINT = 39401;     // DESIGN: the waypoint step that finishes a cut move (ORD-001 R2)
constexpr int32_t CUSTOM_FERRY = 39402;        // DESIGN: ferry route (ORD-007 R4)
constexpr int32_t CUSTOM_DIVE = 39403;         // DESIGN: dive toggle (ORD-008 R1)
constexpr int32_t CUSTOM_AMOVE = 39404;        // DESIGN: the game's attack-move order
constexpr int32_t CUSTOM_FIRE_STATE = 39405;   // DESIGN: the game's fire-state order (ORD-010 R1)
constexpr int32_t CUSTOM_PICKUP = 39406;       // DESIGN: the transport pickup order (ord_pickup.lua, family GLUE)

enum class CommandKind : uint8_t {
	Other, Stop, Wait, Move, Patrol, Fight, Attack, AreaAttack, ManualFire, Guard, Repair, Reclaim, Capture, Resurrect,
	LoadUnits, LoadOnto, UnloadUnit, UnloadUnits, FireState, MoveState, IdleMode, SelfDestruct, Insert, Remove, Build,
	Waypoint, Ferry, Dive, AMove, GameFireState, Pickup,
	Count
};

// a set of kinds as bits
using CommandMask = uint64_t;
constexpr CommandMask MaskOf(CommandKind k) { return CommandMask(1) << static_cast<unsigned>(k); }

// Config names: "move", "patrol", "fight", "attack", "area_attack", "manual_fire", "guard", "repair", "reclaim",
// "capture", "resurrect", "load_units", "load_onto", "unload_unit", "unload_units", "fire_state", "move_state",
// "idle_mode", "self_destruct", "insert", "remove", "build", "stop", "wait", "waypoint", "ferry", "dive",
// "attack_move", "game_fire_state", "pickup". An unknown name: Other.
CommandKind ParseCommandKind(std::string_view name);
// "move, patrol": the mask of the names in a comma list; false when a name is unknown (the mask holds the known ones)
bool ParseCommandList(std::string_view list, CommandMask& mask);
const char* CommandKindName(CommandKind kind);
CommandKind CustomKind(int32_t commandId);          // one of the CUSTOM_ ids; Other for any other id
int32_t CustomId(CommandKind kind);                 // 0 for a kind that is not the game's own

constexpr size_t ORDER_PARAMS = 8;
constexpr size_t ISSUE_PARAMS = 12;   // an insert carries position, id, options and the inner order's parameters

// One entry of a unit's queue as the adapter read it.
struct OrderView {
	int32_t id = 0;                    // the engine command id
	CommandKind kind = CommandKind::Other;
	uint32_t tag = 0;
	uint8_t options = 0;               // the engine's option byte
	bool internal = false;             // OPT_INTERNAL is set
	bool shift = false;
	uint8_t numParams = 0;             // the order's own count, which may exceed ORDER_PARAMS
	std::array<float, ORDER_PARAMS> params{};
};

// One order the family gives: the trace column orders_given (and OrderRules' reissued), in the order given.
struct OrderIssue {
	UnitId unit = NO_UNIT;
	int32_t id = 0;
	uint8_t options = 0;
	uint8_t numParams = 0;
	std::array<float, ISSUE_PARAMS> params{};
};

// the game's fire state (rules param fire_state: "hold", "free", "ground"), in the order a click cycles
enum class FireState : uint8_t { Hold, Free, Ground, Count };
const char* FireStateName(FireState state);
bool ParseFireState(std::string_view name, FireState& state);   // false: not a state name

// the trace column front_class: what is at the front of a unit's queue at its turn
enum class FrontClass : uint8_t { None, Plain, OwnFightOrPatrol, Guard, Assist, EngineWork };

// the internal step the family put ahead of a unit's order (the Lua's ours[u].kind)
enum class OwnKind : uint8_t { None, Attack, Repair, Reclaim, Assist, Follow };

// what a guard's chain reaches (GG.Ord.GuardTask)
enum class WorkKind : uint8_t { None, Build, Repair, Reclaim, Factory, Guard };
const char* WorkKindName(WorkKind kind);            // "build", "repair", "reclaim", "factory", "guard"; "" for None

struct Task {
	WorkKind kind = WorkKind::None;
	int32_t target = NO_UNIT;          // a unit id, or a feature id when isFeature
	bool isFeature = false;
	UnitId lead = NO_UNIT;             // the last builder of the chain (set with kind None too, when there is one)
};

// why a tracked move ended as completed-with-failure
enum class FailReason : uint8_t { BoxedIn, Unreachable, NoProgress };
const char* FailReasonName(FailReason reason);      // "boxed in", "unreachable", "no progress"

} // namespace KT1::Core::Orders

#endif
