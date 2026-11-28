/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: a config row turned into a row ready to ask (OrderRuleRow.h). No engine include.

#include "OrderRuleRow.h"

#include <array>

namespace KT1::Core::OrderRules {

using Orders::DefFacts;
using Orders::DefFlag;

namespace {

// config names, in the order of their enums
constexpr std::array CALL_INS = {"command", "build_step", "feature_step", "transport_load", "transport_unload"};
constexpr std::array ACTIONS = {"refuse", "refuse_and_reissue", "clamp", "native"};
constexpr std::array UNIT_CLASSES = {"any", "not_air", "mobile", "ground", "ground_not_command", "mobile_non_factory",
	"submarine", "transport", "can_reclaim", "indirect_ground", "gap_transport", "idle_lander"};
constexpr std::array CONDITIONS = {"always", "beyond_map", "enemy_unseen", "enemy_target", "unreclaimable_target",
	"attack_move_point", "record_given", "not_accepted", "engine_fire_state", "move_state_from_player", "game_fire_state",
	"self_destruct", "cancels_self_destruct", "dive_not_submarine", "dive_toggle", "carried", "ferry_not_transport",
	"ferry_route", "ends_ferry", "living_reclaim", "transport_gate", "area_reclaim", "not_in_def_accepts",
	"indirect_known_target", "short_move", "near_queued_move", "goal_cell", "occupied_unload_spot", "held_idle_mode",
	"reclaim_while_moving"};

static_assert(CALL_INS.size() == static_cast<size_t>(CallIn::Count));
static_assert(ACTIONS.size() == static_cast<size_t>(Action::Count));
static_assert(UNIT_CLASSES.size() == static_cast<size_t>(UnitClass::Count));
static_assert(CONDITIONS.size() == static_cast<size_t>(Condition::Count));

template<class E, size_t N> bool ParseName(const std::array<const char*, N>& names, std::string_view name, E& out) {
	for (size_t i = 0; i < N; ++i) {
		if (name != names[i]) continue;
		out = static_cast<E>(i);
		return true;
	}
	return false;
}

bool GroundMobile(const DefFacts& def) {
	return def.GroundMover() && !def.Has(DefFlag::Building) && def.speed > 0;
}

} // namespace

bool InClass(const DefFacts& def, UnitClass unitClass) {
	switch (unitClass) {
		case UnitClass::Any: return true;
		case UnitClass::NotAir: return !def.Air();
		case UnitClass::Mobile: return def.Has(DefFlag::CanMove);
		case UnitClass::Ground: return GroundMobile(def);
		case UnitClass::GroundNotCommand: return GroundMobile(def) && !def.Has(DefFlag::CommandUnit);
		case UnitClass::MobileNonFactory: return def.Has(DefFlag::CanMove) && !def.Has(DefFlag::Factory);
		case UnitClass::Submarine: return def.Has(DefFlag::Submarine);
		case UnitClass::Transport: return def.Has(DefFlag::Transport);
		case UnitClass::CanReclaim: return def.Has(DefFlag::CanReclaim);
		case UnitClass::IndirectGround: return !def.Air() && def.Has(DefFlag::IndirectFire);
		case UnitClass::GapTransport: return def.landShortGap > 0 && def.cargoShortGap > 0;
		case UnitClass::IdleLander: return def.Has(DefFlag::IdleLands);
		case UnitClass::Count: break;
	}
	return false;
}

bool ParseCallIn(std::string_view name, CallIn& out) { return ParseName(CALL_INS, name, out); }
bool ParseAction(std::string_view name, Action& out) { return ParseName(ACTIONS, name, out); }
bool ParseCondition(std::string_view name, Condition& out) { return ParseName(CONDITIONS, name, out); }
bool ParseUnitClass(std::string_view name, UnitClass& out) { return ParseName(UNIT_CLASSES, name, out); }

bool Compile(const OrderRuleRow& row, int32_t source, CompiledRow& out) {
	CompiledRow c;
	if (!ParseCallIn(row.call_in, c.callIn) || !ParseAction(row.action, c.action)) return false;
	if (!ParseCondition(row.condition, c.condition) || !ParseUnitClass(row.unit_class, c.unitClass)) return false;
	if (!row.command.empty() && !Orders::ParseCommandList(row.command, c.commands)) return false;
	c.internalToo = row.internal_too;
	c.source = source;
	out = c;
	return true;
}

bool Matches(const CompiledRow& row, Orders::CommandKind kind, const DefFacts& def, bool internal) {
	if (internal && !row.internalToo) return false;
	if (row.commands != 0 && (row.commands & Orders::MaskOf(kind)) == 0) return false;
	return InClass(def, row.unitClass);
}

} // namespace KT1::Core::OrderRules
