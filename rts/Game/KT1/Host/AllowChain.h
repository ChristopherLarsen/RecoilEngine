/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: native validators for the engine's Allow* call-ins, run before the Lua gadget handler's.
//
// A system registers from its game setup (orders can arrive before the first step):
//   KT1::AllowChain::AddCommand("Orders", 0, [this](const CUnit* u, const Command& c, ...) { return true; });
// Order within a chain: (order, system name). A veto chain ends at the first "no", and then Lua is not asked, as when
// an earlier gadget says no. When every validator says yes, Lua is asked exactly as before. A validator with a side
// effect says so in its system's rule doc. With nothing registered every hook is the stock engine.

#ifndef KT1_HOST_ALLOW_CHAIN_H
#define KT1_HOST_ALLOW_CHAIN_H

#include <functional>

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
	// priority is null for a command AI's search; a change is what Lua then receives
	using TargetFn = std::function<bool(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority)>;
	// No veto: every one runs, each sees the damage the one before left; impulse factors multiply.
	using DamageFn = std::function<void(const PreDamage&, float& damage, float& impulseMult)>;

	static void AddCommand(const char* system, int order, CommandFn fn);
	static void AddUnitCreation(const char* system, int order, CreationFn fn);
	static void AddUnitTransfer(const char* system, int order, TransferFn fn);
	static void AddBuildStep(const char* system, int order, UnitStepFn fn);
	static void AddCaptureStep(const char* system, int order, UnitStepFn fn);
	static void AddFeatureBuildStep(const char* system, int order, FeatureStepFn fn);
	static void AddWeaponTarget(const char* system, int order, TargetFn fn);
	static void AddPreDamage(const char* system, int order, DamageFn fn);
	static void Reset();   // game shutdown

	// KT1 hook: SimHost (CEventHandler, one line per call-in). false = denied.
	static bool CheckCommand(const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua);
	static bool CheckUnitCreation(const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop);
	static bool CheckUnitTransfer(const CUnit* u, int newTeam, bool capture);
	static bool CheckBuildStep(const CUnit* builder, const CUnit* u, float part);
	static bool CheckCaptureStep(const CUnit* builder, const CUnit* u, float part);
	static bool CheckFeatureBuildStep(const CUnit* builder, const CFeature* f, float part);
	static bool CheckWeaponTarget(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority);

	// Around the Lua call: PreDamaged changes damage first (and *newDamage, which Lua overwrites when it answers);
	// AfterPreDamaged applies the native impulse factor to Lua's answer. Both do nothing with no validator.
	struct Damaged {
		bool ran = false;
		float impulse = 1.0f;
	};
	static Damaged PreDamaged(const PreDamage& d, float& damage, float* newDamage);
	static bool AfterPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult);
};

} // namespace KT1

#endif
