/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting: TargetingApi.h over the live rule (LUA-FREE S19). While the rule is not on, every call answers its
// neutral value and Active() is false: the caller asks the Lua.

#include "TargetingApi.h"

#include "KT1TargetingRule.h"
#include "Game/KT1/Host/Check.h"
#include "Lua/SupComTargeting.h"

namespace KT1::Targeting {

namespace TG = Core::Targeting;

namespace {
TargetingRule* On() {
	TargetingRule* r = TargetingRule::Live();
	return (r != nullptr && r->On()) ? r : nullptr;
}
}

bool Active() { return On() != nullptr; }

int32_t Target(int32_t unit, int32_t weapon) { return On() ? SupComTargeting::TargetOf(unit, weapon - 1) : TG::NONE; }

TG::Engagement Engage(int32_t unit, int32_t target, bool noLine, int32_t only) {
	TargetingRule* r = On();
	return r ? TG::Engage(r->Context(), unit, target, noLine, only, r->world) : TG::Engagement{};
}

TG::Engagement2 Engage2(int32_t unit, int32_t target, bool noLine) {
	TargetingRule* r = On();
	return r ? TG::Engage2(r->Context(), unit, target, noLine, r->world) : TG::Engagement2{};
}

bool BestKey(int32_t unit, int32_t targetDef, int32_t& kind, int32_t& rank) {
	TargetingRule* r = On();
	return r != nullptr && TG::BestKey(r->Context(), r->world.UnitDef(unit), targetDef, kind, rank);
}

int32_t Focus(int32_t unit) {
	TargetingRule* r = On();
	if (r == nullptr) return TG::NONE;
	const auto slot = r->state.units.index.SlotOf(unit);
	return (slot != Core::DenseIndex::NONE) ? r->state.focus[slot] : TG::NONE;
}

// GG.Cmb.focus[unit] = target: the store, then the engine step
void SetFocus(int32_t unit, int32_t target) {
	TargetingRule* r = On();
	if (r == nullptr || !Check::MayWrite()) return;
	TG::SetFocus(r->state, unit, target);
	SupComTargeting::SetFocus(unit, target);
}

float InFlight(int32_t target) { TargetingRule* r = On(); return r ? TG::InFlight(r->state, target) : 0; }

bool Contact(int32_t target, int32_t allyTeam) { TargetingRule* r = On(); return r != nullptr && TG::Contact(r->world, target, allyTeam); }

TG::Refusal ManualFireRefusal(int32_t unit, int32_t target) {
	TargetingRule* r = On();
	return r ? TG::OrderRefusal(r->Context(), r->world.UnitDef(unit), target, true, r->world) : TG::Refusal::None;
}

int32_t ScanSteps(float intervalSeconds) { TargetingRule* r = On(); return r ? TG::ScanSteps(r->params, intervalSeconds) : 0; }
float ScanIntervalDefaultSeconds() { TargetingRule* r = On(); return r ? r->config.scan_interval_default_s : 0; }
int32_t ScanExtraSteps() { TargetingRule* r = On(); return r ? r->params.extraTicks * r->params.stepsPerTick : 0; }

int32_t CommandGunTarget(int32_t unit) {
	TargetingRule* r = On();
	if (r == nullptr) return TG::NONE;
	const auto slot = r->state.units.index.SlotOf(unit);
	return (slot != Core::DenseIndex::NONE && r->state.gunner[slot].present) ? r->state.gunner[slot].target : TG::NONE;
}

int32_t CommandGunNextScan(int32_t unit) {
	TargetingRule* r = On();
	if (r == nullptr) return TG::NONE;
	const auto slot = r->state.units.index.SlotOf(unit);
	return (slot != Core::DenseIndex::NONE && r->state.gunner[slot].present) ? r->state.gunner[slot].nextScan : TG::NONE;
}

} // namespace KT1::Targeting
