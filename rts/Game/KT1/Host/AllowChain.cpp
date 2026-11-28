/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the native Allow* chains. See AllowChain.h.

#include "AllowChain.h"

#include "Check.h"
#include "Game/KT1/Core/SimHost/CheckTally.h"

namespace KT1 {
namespace {

using Core::SimHost::AskVeto;
using Core::SimHost::Chain;
using Core::SimHost::CheckTally;

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

template<class Fn> void Add(Chain<Fn>& chain, const char* system, bool check, int order, Fn fn) {
	chain.Add(order, system, {system, check, std::move(fn)});
}

void Compare(const std::string& system, const char* callIn, bool same) { Check::Add(system.c_str(), 1, same ? 0 : 1, callIn); }

} // namespace

void AllowChain::AddCommand(const char* system, bool check, int order, CommandFn fn) { Add(chains.command, system, check, order, std::move(fn)); }
void AllowChain::AddUnitCreation(const char* system, bool check, int order, CreationFn fn) { Add(chains.creation, system, check, order, std::move(fn)); }
void AllowChain::AddUnitTransfer(const char* system, bool check, int order, TransferFn fn) { Add(chains.transfer, system, check, order, std::move(fn)); }
void AllowChain::AddBuildStep(const char* system, bool check, int order, UnitStepFn fn) { Add(chains.build, system, check, order, std::move(fn)); }
void AllowChain::AddCaptureStep(const char* system, bool check, int order, UnitStepFn fn) { Add(chains.capture, system, check, order, std::move(fn)); }
void AllowChain::AddFeatureBuildStep(const char* system, bool check, int order, FeatureStepFn fn) { Add(chains.featureBuild, system, check, order, std::move(fn)); }
void AllowChain::AddWeaponTarget(const char* system, bool check, int order, TargetFn fn) { Add(chains.target, system, check, order, std::move(fn)); }
void AllowChain::AddPreDamage(const char* system, bool check, int order, DamageFn fn) { Add(chains.damage, system, check, order, std::move(fn)); }
void AllowChain::Reset() { chains = Chains(); }

bool AllowChain::AskCommand(Veto& s, const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua) {
	return AskVeto(chains.command, s, u, c, playerNum, fromSynced, fromLua);
}
bool AllowChain::AskUnitTransfer(Veto& s, const CUnit* u, int newTeam, bool capture) { return AskVeto(chains.transfer, s, u, newTeam, capture); }
bool AllowChain::AskBuildStep(Veto& s, const CUnit* builder, const CUnit* u, float part) { return AskVeto(chains.build, s, builder, u, part); }
bool AllowChain::AskCaptureStep(Veto& s, const CUnit* builder, const CUnit* u, float part) { return AskVeto(chains.capture, s, builder, u, part); }
bool AllowChain::AskFeatureBuildStep(Veto& s, const CUnit* builder, const CFeature* f, float part) {
	return AskVeto(chains.featureBuild, s, builder, f, part);
}

bool AllowChain::Answered(const Veto& s, const char* callIn, bool lua) {
	s.Each([&](const std::string& system, bool yes) { Compare(system, callIn, yes == lua); });
	return lua;
}

bool AllowChain::AskUnitCreation(CreationShadow& s, const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop) {
	for (const auto& item : chains.creation) {
		Creation answer;
		answer.allow = item.value.fn(def, builder, info, answer.drop);
		if (item.value.check) {
			s.Keep(item.value.system, answer);
		} else if (!answer.allow) {
			drop = answer.drop;
			return false;
		}
	}
	return true;
}

// drop only means something with a refusal
std::pair<bool, bool> AllowChain::AnsweredCreation(const CreationShadow& s, std::pair<bool, bool> lua) {
	s.Each([&](const std::string& system, const Creation& a) {
		Compare(system, "AllowUnitCreation", a.allow == lua.first && (a.allow || a.drop == lua.second));
	});
	return lua;
}

// KT1 hot
bool AllowChain::AskWeaponTarget(TargetShadow& s, unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority) {
	for (const auto& item : chains.target) {
		if (!item.value.check) {
			if (!item.value.fn(attackerID, targetID, weaponNum, weaponDefID, priority)) return false;
			continue;
		}
		Target answer;
		answer.priority = (priority != nullptr) ? *priority : 0.0f;
		answer.allow = item.value.fn(attackerID, targetID, weaponNum, weaponDefID, (priority != nullptr) ? &answer.priority : nullptr);
		s.Keep(item.value.system, answer);
	}
	return true;
}

bool AllowChain::AnsweredTarget(const TargetShadow& s, const float* priority, bool lua) {
	s.Each([&](const std::string& system, const Target& a) {
		const bool samePriority = !lua || priority == nullptr || CheckTally::SameBits(a.priority, *priority);
		Compare(system, "AllowWeaponTarget", a.allow == lua && samePriority);
	});
	return lua;
}

AllowChain::Damaged AllowChain::AskPreDamaged(const PreDamage& d, float& damage, float* newDamage) {
	Damaged native;
	if (chains.damage.Empty()) return native;
	const float incoming = damage;
	for (const auto& item : chains.damage) {
		Damage answer;
		answer.damage = item.value.check ? incoming : damage;
		item.value.fn(d, answer.damage, answer.impulse);
		if (item.value.check) {
			native.shadow.Keep(item.value.system, answer);
		} else {
			native.ran = true;
			damage = answer.damage;
			native.impulse *= answer.impulse;
		}
	}
	if (native.ran) *newDamage = damage;
	return native;
}

// The engine's rule for skipping its own damage handling: no damage and no impulse.
bool AllowChain::AnsweredPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult) {
	native.shadow.Each([&](const std::string& system, const Damage& a) {
		Compare(system, "UnitPreDamaged", CheckTally::SameBits(a.damage, *newDamage) && CheckTally::SameBits(a.impulse, *impulseMult));
	});
	if (!native.ran) return skip;
	*impulseMult *= native.impulse;
	return skip || (*newDamage == 0.0f && *impulseMult == 0.0f);
}

} // namespace KT1
