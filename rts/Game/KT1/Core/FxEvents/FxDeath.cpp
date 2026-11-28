/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: unit death and damage-state sets (replaces the rules of gamedata/fx_death_logic.lua that
// code/LuaRules/Gadgets/fx_death_sets.lua reads; the names in the comments are theirs). Every number is the config's.

#include "Game/KT1/Core/FxEvents/FxDeath.h"

#include <algorithm>
#include <array>

namespace KT1::Core::Fx {

namespace {

// in the order of DeathLayer, DeathKind and the damage classes
constexpr std::array<std::string_view, 4> LAYER_NAMES = {"land", "water", "sub", "air"};   // DESIGN: one per DeathLayer
constexpr std::array<std::string_view, 6> KIND_NAMES = {"mobile", "air", "structure", "command", "sea", "sub"};   // DESIGN: one per DeathKind
constexpr std::array<std::string_view, 3> CLASS_NAMES = {"mobile", "structure", "sub"};   // DESIGN: DeathPlan::states

template<size_t N> int32_t NameIndex(const std::array<std::string_view, N>& names, std::string_view name) {
	const auto found = std::find(names.begin(), names.end(), name);
	return (found == names.end()) ? -1 : static_cast<int32_t>(found - names.begin());
}

// the index of a set's name, added on its first use; -1: no name
int32_t SetIndex(DeathPlan& plan, std::string_view name) {
	if (name.empty())
		return -1;
	const auto found = std::find(plan.sets.begin(), plan.sets.end(), name);
	if (found != plan.sets.end())
		return static_cast<int32_t>(found - plan.sets.begin());
	plan.sets.emplace_back(name);
	return static_cast<int32_t>(plan.sets.size()) - 1;
}

// the non-empty pieces between `separator`s (the Lua's gmatch "[^x]+")
template<class F> void Split(std::string_view text, char separator, F&& piece) {
	while (!text.empty()) {
		const size_t end = text.find(separator);
		const std::string_view head = text.substr(0, end);
		if (!head.empty())
			piece(head);
		text.remove_prefix((end == std::string_view::npos) ? text.size() : end + 1);
	}
}

bool ResolveSelection(const DeathConfig& c, DeathPlan& plan) {
	bool known = true;
	for (const DeathSelectRow& row: c.selection) {
		const int32_t layer = NameIndex(LAYER_NAMES, row.layer);
		if (layer < 0) {
			known = false;
			continue;
		}
		DeathSelect s;
		s.layer = static_cast<DeathLayer>(layer);
		s.lo = row.bound_lo;
		s.hi = row.bound_hi;
		s.extraBelow = row.extra_below;
		s.base = SetIndex(plan, row.base);
		s.extra = SetIndex(plan, row.extra);
		plan.selection.push_back(s);
	}
	return known;
}

// a class's states keep the rows' order: bit i of ActiveStates is its i-th row
bool ResolveStates(const DeathConfig& c, DeathPlan& plan) {
	bool known = true;
	for (const DamageStateRow& row: c.damage_states) {
		const int32_t fxClass = NameIndex(CLASS_NAMES, row.fx_class);
		if (fxClass < 0 || static_cast<int32_t>(plan.states[fxClass].size()) >= DEATH_STATES) {
			known = false;
			continue;
		}
		DamageState state;
		state.fraction = row.fraction;
		Split(row.alternatives, '|', [&](std::string_view alternative) {
			std::vector<int32_t>& sets = state.alternatives.emplace_back();
			Split(alternative, '+', [&](std::string_view name) { sets.push_back(SetIndex(plan, name)); });
		});
		plan.topFraction = std::max(plan.topFraction, row.fraction);
		plan.states[fxClass].push_back(std::move(state));
	}
	return known;
}

// D.Sequence: the kind's row, else the mobile row (a later row of a kind replaces an earlier one)
const DeathSequenceRow* SequenceOf(const DeathConfig& c, DeathKind kind) {
	const std::string_view wanted = KIND_NAMES[static_cast<size_t>(kind)];
	const std::string_view fallback = KIND_NAMES[static_cast<size_t>(DeathKind::Mobile)];
	const DeathSequenceRow* own = nullptr;
	const DeathSequenceRow* mobile = nullptr;
	for (const DeathSequenceRow& row: c.sequence) {
		if (row.kind == wanted)
			own = &row;
		if (row.kind == fallback)
			mobile = &row;
	}
	return (own != nullptr) ? own : mobile;
}

} // namespace

bool ResolveDeathPlan(const DeathConfig& c, DeathPlan& out) {
	out = DeathPlan{};
	const bool selection = ResolveSelection(c, out);
	const bool states = ResolveStates(c, out);
	out.timedSet = SetIndex(out, c.timed_set);
	out.airKillSet = SetIndex(out, c.air_kill_set);
	return selection && states;
}

// Layer
DeathLayer LayerOf(const DeathConfig& c, float y, float groundHeight) {
	if (y > std::max(groundHeight, 0.0f) + c.air_above_units * c.map_unit)
		return DeathLayer::Air;
	if (groundHeight < 0.0f)
		return (y < -(c.sub_below_units * c.map_unit)) ? DeathLayer::Sub : DeathLayer::Water;
	return DeathLayer::Land;
}

int32_t DamageClassOf(DeathKind kind) {
	if (kind == DeathKind::Structure)
		return 1;
	return (kind == DeathKind::Sub) ? 2 : 0;   // DESIGN: CLASS_NAMES' index of sub
}

// D.DeathParts (D.Bounds, D.Select, D.Timed)
DeathParts PartsOfDeath(const DeathConfig& c, const DeathPlan& plan, DeathKind kind, DeathLayer layer, float sx, float sz,
	float legacy, int32_t (*range)(void* ctx, int32_t lo, int32_t hi), void* ctx) {
	DeathParts parts;
	const DeathSequenceRow* seq = SequenceOf(c, kind);
	const int32_t lo = (seq != nullptr) ? seq->wait_min_ticks : 0;
	const int32_t hi = (seq != nullptr) ? seq->wait_max_ticks : 0;
	parts.waitTicks = range(ctx, lo, std::max(lo, hi));   // drawn even when the two are equal
	if (layer == DeathLayer::Air) {   // a kill in flight
		parts.base = plan.airKillSet;
		return parts;
	}
	parts.timed = kind == DeathKind::Structure && legacy >= c.timed_threshold;
	if (layer == DeathLayer::Sub)
		return parts;
	const float bound = c.bound_xz_factor * (sx + sz);
	for (const DeathSelect& row: plan.selection) {   // a bound on a class edge takes the larger class
		if (row.layer == layer && bound >= row.lo && (row.hi < 0.0f || bound < row.hi)) {
			parts.base = row.base;
			if (bound < row.extraBelow)
				parts.extra = row.extra;
			break;
		}
	}
	return parts;
}

uint32_t ActiveStates(const DeathPlan& plan, int32_t damageClass, float healthShare) {
	if (damageClass < 0 || damageClass >= static_cast<int32_t>(CLASS_NAMES.size()))
		return 0;
	uint32_t on = 0;
	uint32_t bit = 1;
	for (const DamageState& state: plan.states[damageClass]) {
		if (healthShare <= state.fraction)
			on |= bit;
		bit <<= 1;
	}
	return on;
}

// D.DamageCount: the last row the volume passes
int32_t DamageEmitters(const DeathConfig& c, float volume) {
	int32_t count = 1;
	for (const DamageCountRow& row: c.damage_counts) {
		if (volume > row.volume_above || row.volume_above == 0.0f)
			count = row.count;
	}
	return count;
}

} // namespace KT1::Core::Fx
