/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: native validators for the engine's Allow* call-ins, run before the Lua gadget handler's.
//
// A system registers at game setup (orders can arrive before the first step), under its registered name:
//   KT1::AllowChain::AddCommand("Orders", 0, [this](const CUnit* u, const Command& c, ...) {...});
// Whether its validators are in check mode is the system's switch, asked of the registry.
// Order within a chain: (order, system name).
//
// Live (switch on): in a veto chain the first "no" ends it and Lua is not asked, as when an earlier gadget says no. When
// every validator says yes, Lua is asked exactly as before. With nothing registered every hook is the stock engine.
//
// Porting rule (live). A native validator runs before ALL Lua gadgets. A gadget that was asked before the ported one
// (lower layer) is no longer called when the native one refuses: a package that ports an Allow* handler checks every
// gadget ahead of it in layer order for side effects in that call-in, and lists what it found in its rule doc. A
// validator with a side effect says so there.
//
// Check mode: a validator never decides and writes nothing (Check::MayWrite is false inside it). Its answer is compared with the answer of the ONE Lua handler
// it replaces, not with the chain's result: that handler, in check mode, reports it from inside the call, on every path out:
//   Spring.KT1.Check.Answer("Orders", "AllowCommand", unitID, cmdID, 0, allow)
// with the same system name and these keys and answers, as the handler receives them:
//   call-in                 id1, id2, id3                          answer, answer2          input
//   AllowCommand            unitID, cmdID, 0                       allow
//   AllowUnitCreation       unitDefID, builderID, 0                allow, drop
//   AllowUnitTransfer       unitID, newTeam, 0                     allow
//   AllowUnitBuildStep      builderID, unitID, 0                   allow
//   AllowUnitCaptureStep    builderID, unitID, 0                   allow
//   AllowFeatureBuildStep   builderID, featureID, 0                allow
//   AllowWeaponTarget       attackerID, targetID, weaponNum        allow, priority          defPriority, if given one
//   UnitPreDamaged          unitID, projectileID, weaponDefID      newDamage, impulse (1)   damage
// AllowUnitCreation: drop is compared only when both refuse. AllowWeaponTarget: priority only when both allow.
// Never asked (another gadget refused first, or the engine did not call Lua): the native answer counts as "unmatched",
// not as a difference. The handler answered and no native answer has that key: a difference. Input differs (an earlier
// gadget changed the damage or priority): unmatched. A call a live validator refused is not compared.

#ifndef KT1_HOST_ALLOW_CHAIN_H
#define KT1_HOST_ALLOW_CHAIN_H

#include <cstddef>
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
	// priority is null for a command AI's search; a live change is what Lua then receives
	using TargetFn = std::function<bool(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority)>;
	// No veto: every live one runs, each sees the damage the one before left; impulse factors multiply.
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

	// KT1 hook: SimHost (CEventHandler). A Call lives across one call-in: check-mode answers the replaced Lua has not
	// matched by its end are unmatched.
	class Call {
	public:
		Call();
		~Call();
		Call(const Call&) = delete;
		Call& operator=(const Call&) = delete;

	private:
		size_t mark;
	};

	// Asked before the Lua dispatch; false = a live validator refused.
	static bool AskCommand(const CUnit* u, const Command& c, int playerNum, bool fromSynced, bool fromLua);
	static bool AskUnitCreation(const UnitDef* def, const CUnit* builder, const BuildInfo* info, bool& drop);
	static bool AskUnitTransfer(const CUnit* u, int newTeam, bool capture);
	static bool AskBuildStep(const CUnit* builder, const CUnit* u, float part);
	static bool AskCaptureStep(const CUnit* builder, const CUnit* u, float part);
	static bool AskFeatureBuildStep(const CUnit* builder, const CFeature* f, float part);
	static bool AskWeaponTarget(unsigned attackerID, unsigned targetID, unsigned weaponNum, unsigned weaponDefID, float* priority);

	struct Damaged {
		bool ran = false;       // a live validator ran
		float impulse = 1.0f;   // the live impulse factor
	};
	// Before Lua: live validators change damage (and *newDamage, which Lua overwrites when it answers). After Lua: the
	// live impulse factor applies to Lua's answer; the result is whether the engine skips its damage handling.
	static Damaged AskPreDamaged(const PreDamage& d, float& damage, float* newDamage);
	static bool AfterPreDamaged(const Damaged& native, bool skip, const float* newDamage, float* impulseMult);
};

} // namespace KT1

#endif
