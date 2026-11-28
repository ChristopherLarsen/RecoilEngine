/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: native validators for the engine's Allow* call-ins, run before the Lua gadget handler's.
//
// A system registers at game setup (orders can arrive before the first step), saying whether it is in check mode:
//   KT1::AllowChain::AddCommand("Orders", SwitchMode() == Mode::Check, 0, [this](const CUnit* u, const Command& c, ...) {...});
// Order within a chain: (order, system name).
//
// Live (switch on): in a veto chain the first "no" ends it and Lua is not asked, as when an earlier gadget says no. When
// every validator says yes, Lua is asked exactly as before. With nothing registered every hook is the stock engine.
// Check mode: a validator never decides and changes nothing. Its answer is compared with the Lua chain's result and
// counted on its system's [KT1CHECK] line. A call a live validator refused is not compared (Lua was not asked).
//
// Porting rule. A native validator runs before ALL Lua gadgets. A gadget that was asked before the ported one (lower
// layer) is no longer called when the native one refuses: a package that ports an Allow* handler checks every gadget
// ahead of it in layer order for side effects in that call-in, and lists what it found in its rule doc. In check mode a
// veto by any other gadget differs from a native "yes" too; the package accounts for those.
// A validator with a side effect says so in its rule doc.

#ifndef KT1_HOST_ALLOW_CHAIN_H
#define KT1_HOST_ALLOW_CHAIN_H

#include <functional>
#include <utility>

#include "Game/KT1/Core/SimHost/Validators.h"

class CUnit;
class CFeature;
struct Command;
struct UnitDef;
struct BuildInfo;

namespace KT1 {

struct PreDamage {
	const CUnit* unit;
	const CUnit* attacker;   // may be null
	int weaponDefID;
	int projectileID;
	bool paralyzer;
};

class AllowChain {
public:
	using CommandFn = std::function<bool(const CUnit*, const Command&, int playerNum, bool fromSynced, bool fromLua)>;
	// drop: with a "no", whether the factory forgets the order (the engine's second return); true unless set
	using CreationFn = std::function<bool(const UnitDef*, const CUnit* builder, const BuildInfo*, bool& drop)>;
	using TransferFn = std::function<bool(const CUnit*, int newTeam, bool capture)>;
	using UnitStepFn = std::function<bool(const CUnit* builder, const CUnit*, float part)>;
	using FeatureStepFn = std::function<bool(const CUnit* builder, const CFeature*, float part)>;
	// priority is null for a command AI's search; a live change is what Lua then receives
	using TargetFn = std::function<bool(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority)>;
	// No veto: every live one runs, each sees the damage the one before left; impulse factors multiply.
	using DamageFn = std::function<void(const PreDamage&, float& damage, float& impulseMult)>;

	static void AddCommand(const char* system, bool check, int order, CommandFn fn);
	static void AddUnitCreation(const char* system, bool check, int order, CreationFn fn);
	static void AddUnitTransfer(const char* system, bool check, int order, TransferFn fn);
	static void AddBuildStep(const char* system, bool check, int order, UnitStepFn fn);
	static void AddCaptureStep(const char* system, bool check, int order, UnitStepFn fn);
	static void AddFeatureBuildStep(const char* system, bool check, int order, FeatureStepFn fn);
	static void AddWeaponTarget(const char* system, bool check, int order, TargetFn fn);
	static void AddPreDamage(const char* system, bool check, int order, DamageFn fn);
	static void Reset();   // game shutdown

	// KT1 hook: SimHost (CEventHandler, around each call-in's Lua dispatch). Ask: false = a live validator refused.
	// Answered: the Lua chain's result, compared with the check-mode answers.
	using Veto = Core::SimHost::Shadow<bool>;
	static bool AskCommand(Veto& s, const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua);
	static bool AskUnitTransfer(Veto& s, const CUnit* u, int newTeam, bool capture);
	static bool AskBuildStep(Veto& s, const CUnit* builder, const CUnit* u, float part);
	static bool AskCaptureStep(Veto& s, const CUnit* builder, const CUnit* u, float part);
	static bool AskFeatureBuildStep(Veto& s, const CUnit* builder, const CFeature* f, float part);
	static bool Answered(const Veto& s, const char* callIn, bool lua);

	struct Creation {
		bool allow = true;
		bool drop = true;
	};
	using CreationShadow = Core::SimHost::Shadow<Creation>;
	static bool AskUnitCreation(CreationShadow& s, const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop);
	static std::pair<bool, bool> AnsweredCreation(const CreationShadow& s, std::pair<bool, bool> lua);

	struct Target {
		bool allow = true;
		float priority = 0.0f;
	};
	using TargetShadow = Core::SimHost::Shadow<Target>;
	static bool AskWeaponTarget(TargetShadow& s, unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority);
	static bool AnsweredTarget(const TargetShadow& s, const float* priority, bool lua);

	struct Damage {
		float damage = 0.0f;
		float impulse = 1.0f;
	};
	struct Damaged {
		bool ran = false;       // a live validator ran
		float impulse = 1.0f;   // the live impulse factor
		Core::SimHost::Shadow<Damage> shadow;
	};
	// Before Lua: live validators change damage (and *newDamage, which Lua overwrites when it answers). After Lua: the
	// live impulse factor applies to Lua's answer; the result is whether the engine skips its damage handling.
	static Damaged AskPreDamaged(const PreDamage& d, float& damage, float* newDamage);
	static bool AnsweredPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult);
};

} // namespace KT1

#endif
