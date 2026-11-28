/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: the native Allow* chains. See AllowChain.h.

#include "AllowChain.h"

#include "Game/KT1/Core/SimHost/Ordered.h"

namespace KT1 {
namespace {

using Core::SimHost::AllAllow;
using Core::SimHost::Ordered;

struct Chains {
	Ordered<AllowChain::CommandFn> command;
	Ordered<AllowChain::CreationFn> creation;
	Ordered<AllowChain::TransferFn> transfer;
	Ordered<AllowChain::UnitStepFn> build;
	Ordered<AllowChain::UnitStepFn> capture;
	Ordered<AllowChain::FeatureStepFn> featureBuild;
	Ordered<AllowChain::TargetFn> target;
	Ordered<AllowChain::DamageFn> damage;
};

Chains chains;

} // namespace

void AllowChain::AddCommand(const char* system, int order, CommandFn fn) { chains.command.Add(order, system, std::move(fn)); }
void AllowChain::AddUnitCreation(const char* system, int order, CreationFn fn) { chains.creation.Add(order, system, std::move(fn)); }
void AllowChain::AddUnitTransfer(const char* system, int order, TransferFn fn) { chains.transfer.Add(order, system, std::move(fn)); }
void AllowChain::AddBuildStep(const char* system, int order, UnitStepFn fn) { chains.build.Add(order, system, std::move(fn)); }
void AllowChain::AddCaptureStep(const char* system, int order, UnitStepFn fn) { chains.capture.Add(order, system, std::move(fn)); }
void AllowChain::AddFeatureBuildStep(const char* system, int order, FeatureStepFn fn) { chains.featureBuild.Add(order, system, std::move(fn)); }
void AllowChain::AddWeaponTarget(const char* system, int order, TargetFn fn) { chains.target.Add(order, system, std::move(fn)); }
void AllowChain::AddPreDamage(const char* system, int order, DamageFn fn) { chains.damage.Add(order, system, std::move(fn)); }
void AllowChain::Reset() { chains = Chains(); }

bool AllowChain::CheckCommand(const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua) {
	return AllAllow(chains.command, u, c, playerNum, fromSynced, fromLua);
}

bool AllowChain::CheckUnitCreation(const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop) {
	for (const auto& item : chains.creation) {
		bool d = true;
		if (!item.value(def, builder, info, d)) {
			drop = d;
			return false;
		}
	}
	return true;
}

bool AllowChain::CheckUnitTransfer(const CUnit* u, int newTeam, bool capture) { return AllAllow(chains.transfer, u, newTeam, capture); }
bool AllowChain::CheckBuildStep(const CUnit* builder, const CUnit* u, float part) { return AllAllow(chains.build, builder, u, part); }
bool AllowChain::CheckCaptureStep(const CUnit* builder, const CUnit* u, float part) { return AllAllow(chains.capture, builder, u, part); }
bool AllowChain::CheckFeatureBuildStep(const CUnit* builder, const CFeature* f, float part) { return AllAllow(chains.featureBuild, builder, f, part); }

// KT1 hot
bool AllowChain::CheckWeaponTarget(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority) {
	return AllAllow(chains.target, attackerID, targetID, weaponNum, weaponDefID, priority);
}

AllowChain::Damaged AllowChain::PreDamaged(const PreDamage& d, float& damage, float* newDamage) {
	Damaged native;
	if (chains.damage.Empty()) return native;
	native.ran = true;
	for (const auto& item : chains.damage) {
		float impulse = 1.0f;
		item.value(d, damage, impulse);
		native.impulse *= impulse;
	}
	*newDamage = damage;
	return native;
}

// The engine's rule for skipping its own damage handling: no damage and no impulse.
bool AllowChain::AfterPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult) {
	if (!native.ran) return skip;
	*impulseMult *= native.impulse;
	return skip || (*newDamage == 0.0f && *impulseMult == 0.0f);
}

} // namespace KT1
