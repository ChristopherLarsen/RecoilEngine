/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the names of command kinds, fire states, work kinds and failure reasons, and the game's own command
// ids (OrderTypes.h). No engine include.

#include "OrderTypes.h"

namespace KT1::Core::Orders {

namespace {

// config names, in the order of CommandKind
constexpr std::array KINDS = {"other", "stop", "wait", "move", "patrol", "fight", "attack", "area_attack", "manual_fire",
	"guard", "repair", "reclaim", "capture", "resurrect", "load_units", "load_onto", "unload_unit", "unload_units",
	"fire_state", "move_state", "idle_mode", "self_destruct", "insert", "remove", "build", "waypoint", "ferry", "dive",
	"attack_move", "game_fire_state", "pickup"};
static_assert(KINDS.size() == static_cast<size_t>(CommandKind::Count));
static_assert(static_cast<size_t>(CommandKind::Count) <= sizeof(CommandMask) * 8);   // KT1 lint: allow number-label bits in a byte

struct Custom {
	int32_t id;
	CommandKind kind;
};
constexpr std::array<Custom, 6> CUSTOMS = {{   // KT1 lint: allow number-label the count of the ids below
	{CUSTOM_WAYPOINT, CommandKind::Waypoint}, {CUSTOM_FERRY, CommandKind::Ferry}, {CUSTOM_DIVE, CommandKind::Dive},
	{CUSTOM_AMOVE, CommandKind::AMove}, {CUSTOM_FIRE_STATE, CommandKind::GameFireState},
	{CUSTOM_PICKUP, CommandKind::Pickup},
}};

constexpr std::array FIRE_STATES = {"hold", "free", "ground"};
constexpr std::array WORK_KINDS = {"", "build", "repair", "reclaim", "factory", "guard"};
constexpr std::array FAIL_REASONS = {"boxed in", "unreachable", "no progress"};
static_assert(FIRE_STATES.size() == static_cast<size_t>(FireState::Count));

std::string_view Trimmed(std::string_view s) {
	while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
	while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
	return s;
}

template<size_t N> const char* NameAt(const std::array<const char*, N>& names, size_t i) { return (i < N) ? names[i] : ""; }

} // namespace

CommandKind ParseCommandKind(std::string_view name) {
	for (size_t i = 0; i < KINDS.size(); ++i) {
		if (name == KINDS[i]) return static_cast<CommandKind>(i);
	}
	return CommandKind::Other;
}

bool ParseCommandList(std::string_view list, CommandMask& mask) {
	mask = 0;
	bool known = true;
	while (!list.empty()) {
		const size_t comma = list.find(',');
		const std::string_view name = Trimmed(list.substr(0, comma));
		list = (comma == std::string_view::npos) ? std::string_view() : list.substr(comma + 1);
		const CommandKind kind = ParseCommandKind(name);
		if (kind != CommandKind::Other) mask |= MaskOf(kind);
		else if (!name.empty()) known = false;   // "other" is no row's command either
	}
	return known;
}

const char* CommandKindName(CommandKind kind) { return NameAt(KINDS, static_cast<size_t>(kind)); }

CommandKind CustomKind(int32_t commandId) {
	for (const Custom& c : CUSTOMS) {
		if (c.id == commandId) return c.kind;
	}
	return CommandKind::Other;
}

int32_t CustomId(CommandKind kind) {
	for (const Custom& c : CUSTOMS) {
		if (c.kind == kind) return c.id;
	}
	return 0;
}

const char* FireStateName(FireState state) { return NameAt(FIRE_STATES, static_cast<size_t>(state)); }

bool ParseFireState(std::string_view name, FireState& state) {
	for (size_t i = 0; i < FIRE_STATES.size(); ++i) {
		if (name != FIRE_STATES[i]) continue;
		state = static_cast<FireState>(i);
		return true;
	}
	return false;
}

const char* WorkKindName(WorkKind kind) { return NameAt(WORK_KINDS, static_cast<size_t>(kind)); }
const char* FailReasonName(FailReason reason) { return NameAt(FAIL_REASONS, static_cast<size_t>(reason)); }

} // namespace KT1::Core::Orders
