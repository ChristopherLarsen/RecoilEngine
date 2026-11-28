/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the native Allow* chains. See AllowChain.h.

#include "AllowChain.h"

#include "Check.h"
#include "Fakes.h"
#include "Game/KT1/Core/SimHost/Validators.h"
#include "Lua/LuaConfig.h"
#include "Sim/Features/Feature.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

using Core::SimHost::AskVeto;
using Core::SimHost::Chain;

struct Chains {
	Chain<AllowChain::CommandFn> command;
	Chain<AllowChain::CreationFn> creation;
	Chain<AllowChain::TransferFn> transfer;
	Chain<AllowChain::UnitStepFn> build;
	Chain<AllowChain::UnitStepFn> capture;
	Chain<AllowChain::FeatureStepFn> featureBuild;
	Chain<AllowChain::TargetFn> target;
	Chain<AllowChain::DamageFn> damage;
};

Chains chains;

template<class Fn> void Add(Chain<Fn>& chain, const char* system, int order, Fn fn) {
	chain.Add(order, system, {system, Check::InCheck(system), std::move(fn), Fakes::Sees(system)});
}

Check::Value Answer(float first) {
	Check::Value v;
	v.v[0] = first;
	return v;
}

// no unit: not a fake
bool IsFake(const CUnit* u) { return u != nullptr && Fakes::Is(u); }
bool IsFake(unsigned unitID) { return IsFake(unitHandler.GetUnit(unitID)); }

// A veto chain whose check-mode answers go to the check helper under the call-in's keys.
// KT1 hot
template<class Fn, class... Args> bool Veto(const Chain<Fn>& chain, const char* callIn, int id1, int id2, Args&&... args) {
	return AskVeto(chain, Check::Gate(), [&](const auto& system, bool yes) {
		Check::Native(system.c_str(), callIn, id1, id2, 0, Answer(yes ? 1.0f : 0.0f));
	}, args...);
}

} // namespace

void AllowChain::AddCommand(const char* system, int order, CommandFn fn) { Add(chains.command, system, order, std::move(fn)); }
void AllowChain::AddUnitCreation(const char* system, int order, CreationFn fn) { Add(chains.creation, system, order, std::move(fn)); }
void AllowChain::AddUnitTransfer(const char* system, int order, TransferFn fn) { Add(chains.transfer, system, order, std::move(fn)); }
void AllowChain::AddBuildStep(const char* system, int order, UnitStepFn fn) { Add(chains.build, system, order, std::move(fn)); }
void AllowChain::AddCaptureStep(const char* system, int order, UnitStepFn fn) { Add(chains.capture, system, order, std::move(fn)); }
void AllowChain::AddFeatureBuildStep(const char* system, int order, FeatureStepFn fn) { Add(chains.featureBuild, system, order, std::move(fn)); }
void AllowChain::AddWeaponTarget(const char* system, int order, TargetFn fn) { Add(chains.target, system, order, std::move(fn)); }
void AllowChain::AddPreDamage(const char* system, int order, DamageFn fn) { Add(chains.damage, system, order, std::move(fn)); }
void AllowChain::Reset() { chains = Chains(); }

AllowChain::Call::Call() : mark(Check::Mark()) {}
AllowChain::Call::~Call() { Check::CloseScope(mark); }

bool AllowChain::AskCommand(const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua) {
	if (chains.command.Empty()) return true;
	return Veto(chains.command, "AllowCommand", u->id, c.GetID(), u, c, playerNum, fromSynced, fromLua, Faked{IsFake(u)});
}
bool AllowChain::AskUnitTransfer(const CUnit* u, int newTeam, bool capture) {
	if (chains.transfer.Empty()) return true;
	return Veto(chains.transfer, "AllowUnitTransfer", u->id, newTeam, u, newTeam, capture, Faked{IsFake(u)});
}
bool AllowChain::AskBuildStep(const CUnit* builder, const CUnit* u, float part) {
	if (chains.build.Empty()) return true;
	return Veto(chains.build, "AllowUnitBuildStep", builder->id, u->id, builder, u, part, Faked{IsFake(builder), IsFake(u)});
}
bool AllowChain::AskCaptureStep(const CUnit* builder, const CUnit* u, float part) {
	if (chains.capture.Empty()) return true;
	return Veto(chains.capture, "AllowUnitCaptureStep", builder->id, u->id, builder, u, part,
		Faked{IsFake(builder), IsFake(u)});
}
bool AllowChain::AskFeatureBuildStep(const CUnit* builder, const CFeature* f, float part) {
	if (chains.featureBuild.Empty()) return true;
	return Veto(chains.featureBuild, "AllowFeatureBuildStep", builder->id, f->id, builder, f, part, Faked{IsFake(builder)});
}

bool AllowChain::AskUnitCreation(const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop) {
	if (chains.creation.Empty()) return true;
	const Faked faked{Fakes::IsDef(def), IsFake(builder)};
	for (const auto& item : chains.creation) {
		const Check::Running running(item.value.system.c_str(), item.value.check);
		bool itemDrop = true;
		const bool yes = item.value.fn(def, builder, info, itemDrop, faked);
		if (item.value.check) {
			Check::Value v = Answer(yes ? 1.0f : 0.0f);
			v.v[1] = itemDrop ? 1.0f : 0.0f;
			v.count = yes ? 1 : Check::Value::BOTH;
			Check::Native(item.value.system.c_str(), "AllowUnitCreation", def->id, (builder != nullptr) ? builder->id : -1, 0, v);
		} else if (!yes) {
			drop = itemDrop;
			return false;
		}
	}
	return true;
}

// KT1 hot
bool AllowChain::AskWeaponTarget(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority) {
	if (chains.target.Empty()) return true;
	const Faked faked{IsFake(attackerID), IsFake(targetID)};
	for (const auto& item : chains.target) {
		const Check::Running running(item.value.system.c_str(), item.value.check);
		if (!item.value.check) {
			if (!item.value.fn(attackerID, targetID, weaponNum, weaponDefID, priority, faked)) return false;
			continue;
		}
		const float incoming = (priority != nullptr) ? *priority : 0.0f;
		float own = incoming;
		const bool yes = item.value.fn(attackerID, targetID, weaponNum, weaponDefID, (priority != nullptr) ? &own : nullptr, faked);
		Check::Value v = Answer(yes ? 1.0f : 0.0f);
		v.v[1] = own;
		v.count = (yes && priority != nullptr) ? Check::Value::BOTH : 1;
		v.hasInput = (priority != nullptr);
		v.input = incoming;
		// the ids as Lua receives them (a command AI's -1, the weapon number from LUA_WEAPON_BASE_INDEX)
		const int weapon = static_cast<int>(weaponNum);
		Check::Native(item.value.system.c_str(), "AllowWeaponTarget", static_cast<int>(attackerID), static_cast<int>(targetID),
			weapon + LUA_WEAPON_BASE_INDEX * (weapon >= 0), v);
	}
	return true;
}

AllowChain::Damaged AllowChain::AskPreDamaged(const PreDamage& d, float& damage, float* newDamage) {
	Damaged native;
	if (chains.damage.Empty()) return native;
	const float incoming = damage;
	const Faked faked{IsFake(d.unit), IsFake(d.attacker)};
	for (const auto& item : chains.damage) {
		if (faked.first && !item.value.seesFakes) continue;   // Lua gadgets are not asked either
		const Check::Running running(item.value.system.c_str(), item.value.check);
		float own = item.value.check ? incoming : damage;
		float impulse = 1.0f;
		item.value.fn(d, own, impulse, faked);
		if (!item.value.check) {
			native.ran = true;
			damage = own;
			native.impulse *= impulse;
			continue;
		}
		Check::Value v = Answer(own);
		v.v[1] = impulse;
		v.count = Check::Value::BOTH;
		v.hasInput = true;
		v.input = incoming;
		Check::Native(item.value.system.c_str(), "UnitPreDamaged", d.unit->id, d.projectileID, d.weaponDefID, v);
	}
	if (native.ran) *newDamage = damage;
	return native;
}

// The engine's rule for skipping its own damage handling: no damage and no impulse.
bool AllowChain::AfterPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult) {
	if (!native.ran) return skip;
	*impulseMult *= native.impulse;
	return skip || (*newDamage == 0.0f && *impulseMult == 0.0f);
}

} // namespace KT1
