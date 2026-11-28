/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 TimedRules core: the per-unit state of every timed row, and the decisions of stun, weapon charge and crushing
// (LUA-FREE H01, step S21). No engine include: the adapter (Rules/TimedRules) implements TimedWorld.
//
// REQ CMB-023 (stun), ECO-018 (charge), CMB-017 (crushing). After the move, once per step, in this order (the gadgets'
// order): RunCharges, RunCrush, RunDue. StunExplosion runs inside the Explosion call-in, before the damage; ChargeFire
// and ChargeWind run when a unit script asks. Units are visited in ascending id. A row's end waits on one DueWheel,
// keyed by row * maxUnits + unit, so a step's ends come out row by row in unit order.
// Columns (check, trace, replay): rule_state and rule_due = rules.

#ifndef KT1_CORE_TIMED_STATE_H
#define KT1_CORE_TIMED_STATE_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DueWheel.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/Damage/CombatEvents.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"
#include "Game/KT1/Core/TimedRules/TimedRule.h"

namespace KT1::Core::TimedRules {

constexpr int32_t NONE = -1;
constexpr int32_t MAX_RULES = 16;           // rows of the table
constexpr int32_t MAX_UNIT_WEAPONS = 16;    // weapon numbers 1 .. this

// one stun entry in sim units
struct StunEntry {
	int32_t steps = 0;                      // floor(seconds * rate + 0.5)
	float seconds = 0;
	float radius = 0;                       // elmos
	std::vector<int32_t> allow, deny;       // class ids; no allow ids: any
};

struct WeaponStun {
	std::vector<StunEntry> entries;         // empty: the weapon stuns nobody
	bool needsUpgrade = false;
};

struct ChargeSpec {
	float cost = 0;                         // customParams.shot_cost_energy; 0: not a charged weapon
	float drain = 0;                        // energy a second: drain_energy, else cost / max(reload, 1 / rate)
};

struct UnitTimed {
	std::vector<int32_t> classes;           // target_classes and the extra classes, as class ids
	std::array<int32_t, MAX_UNIT_WEAPONS> weapons{};   // weapon def by weapon number - 1; 0: no weapon
	bool crusher = false;                   // customParams.crushes
	bool crushable = false;                 // customParams.crushable
};

struct TimedDefs {
	std::vector<std::string> classNames;    // class id to name ("anything" and "any" pass every unit)
	std::vector<WeaponStun> stuns;          // by weapon def id
	std::vector<ChargeSpec> charges;        // by weapon def id
	std::vector<UnitTimed> units;           // by unit def id
};

// the config in sim units
struct TimedParams {
	int32_t stepsPerSecond = 1;
	int32_t chargeLookSteps = 1;
	int32_t chargeLookCap = 0;
	int32_t stunLogCap = 0;
	float paralyzeOver = 0;
	float stunHitSearch = 0, stunAreaPad = 0, stunHitPad = 0, stunAreaEpsilon = 0;
	float chargeReadyEpsilon = 0, chargeMinRequest = 0;
	float crushContactPad = 0, crushQueryPad = 0, crushMoveThreshold = 0;
};

TimedParams MakeTimedParams(const TimedRulesConfig& config, int32_t stepsPerSecond);

// the engine and the other rules, as the timed rows read and write them; writes go through SimContext::Write
class TimedWorld {
public:
	virtual ~TimedWorld() = default;
	virtual bool ValidUnit(int32_t unit) = 0;
	virtual bool DeadUnit(int32_t unit) = 0;
	virtual int32_t UnitTeam(int32_t unit) = 0;                 // NONE: gone
	virtual int32_t UnitDef(int32_t unit) = 0;                  // NONE: gone
	virtual bool Allied(int32_t teamA, int32_t teamB) = 0;
	virtual bool Neutral(int32_t unit) = 0;
	virtual bool Health(int32_t unit, float& health, float& maxHealth) = 0;
	virtual bool Position(int32_t unit, float (&pos)[3]) = 0;
	virtual bool MidPosition(int32_t unit, float (&pos)[3]) = 0;
	virtual bool Velocity(int32_t unit, float (&velocity)[3]) = 0;
	virtual float Radius(int32_t unit) = 0;
	virtual bool InAirLayer(int32_t unit) = 0;                  // supcomLayer "air", else a flying aircraft
	virtual bool HasOrder(int32_t unit) = 0;                    // the queue is not empty
	virtual void UnitsInSphere(const float (&pos)[3], float radius, std::vector<int32_t>& out) = 0;   // fakes left out
	virtual void UnitsInCylinder(float x, float z, float radius, std::vector<int32_t>& out) = 0;
	virtual void SetParalyze(int32_t unit, float damage) = 0;
	virtual void Destroy(int32_t unit, int32_t attacker) = 0;   // Spring.DestroyUnit(unit, false, false, attacker)
	virtual bool StunParam(int32_t unit) = 0;                   // the rules param stun_enabled is 1
	virtual void WriteStunParam(int32_t unit, bool on) = 0;
	// the economy (GG.Eco.Request): energy for one weapon's charge, answered later by ChargeGrant
	virtual void RequestEnergy(int32_t team, float want, int32_t unit, int32_t weapon) = 0;
	virtual void Show(const ChargeProgress& event) = 0;
	virtual void Show(const ChargeFired& event) = 0;
};

struct RuleSlot {
	uint8_t state = 0;                      // 0: not active
	int32_t due = NONE;                     // the step it ends
};

struct ChargeSlot {
	bool used = false;                      // a weapon the script asked about
	bool started = false;                   // REQ ECO-018 R5: it has had a first target
	bool charging = false;
	float drawn = 0;
	float pending = 0;                      // asked of the economy and not yet answered
	float sent = -1;                        // the progress the last look event told
	int32_t weaponDef = 0;
};

struct StunLogEntry {
	int32_t step = 0;
	int32_t unit = NONE;
	int32_t weaponDef = NONE;
	float seconds = 0;
};

// a shot the charge gate let fire (tests: GG.Charge.shots)
struct ShotLogEntry {
	int32_t step = 0;
	int32_t unit = NONE;
};

// what a row does to a unit at its start and its end, for a row whose effect another system registers
using EffectFn = void (*)(void* context, int32_t unit, int32_t rule, bool start);

struct RegisteredEffect {
	std::string name;
	EffectFn fn = nullptr;
	void* context = nullptr;
};

struct TimedState {
	void Init(int32_t maxUnits, const TimedRulesConfig& config);

	SimHost::UnitColumns units;
	Column<std::array<RuleSlot, MAX_RULES>> rules;               // columns rule_state, rule_due
	Column<std::array<ChargeSlot, MAX_UNIT_WEAPONS>> charge;
	Column<uint8_t> stunEnabled;            // REQ CMB-023 R6: GG.Stun.SetEnabled
	SortedIdList crushers;
	SortedIdList charging;                  // units with a used charge slot
	DueWheel wheel;                         // key: rule * maxUnits + unit
	std::vector<std::string> rowIds;        // row index to id
	std::vector<RegisteredEffect> effects;
	std::vector<StunLogEntry> stunLog;      // the newest stunLogCap stuns (tests)
	std::vector<ShotLogEntry> shotLog;      // every shot the gate let fire, while keepShots (tests)
	bool keepShots = false;
	int32_t maxIds = 0;
	int32_t lookStep = NONE, lookCount = 0; // the look events sent this step
};

struct TimedContext {
	const TimedDefs& defs;
	const TimedParams& params;
};

void TimedUnitCreated(TimedState& state, const TimedContext& context, int32_t unit, int32_t unitDef);
void TimedUnitDestroyed(TimedState& state, int32_t unit);

// stun
void StunExplosion(TimedState& state, const TimedContext& context, int32_t weaponDef, const float (&pos)[3], int32_t attacker,
	int32_t step, TimedWorld& world);
// REQ CMB-023 R4, R8: one stun; false: a stun that ends as late or later stands
bool Stun(TimedState& state, const TimedContext& context, int32_t unit, const StunEntry& entry, int32_t weaponDef, int32_t step,
	TimedWorld& world);
bool Stunned(const TimedState& state, int32_t unit);
void SetStunEnabled(TimedState& state, int32_t unit, bool on, TimedWorld& world);

// weapon charge; weapon numbers from 1
bool ChargeFire(TimedState& state, const TimedContext& context, int32_t unit, int32_t weapon, int32_t step, TimedWorld& world);
void ChargeWind(TimedState& state, const TimedContext& context, int32_t unit, int32_t weapon, float progress, int32_t step,
	TimedWorld& world);
void ChargeSet(TimedState& state, const TimedContext& context, int32_t unit, int32_t weapon, float drawn, TimedWorld& world);
// the economy's answer to RequestEnergy: `share` of `want` was supplied
void ChargeGrant(TimedState& state, const TimedContext& context, int32_t unit, int32_t weapon, float want, float share);

void RunCharges(TimedState& state, const TimedContext& context, int32_t step, TimedWorld& world);
void RunCrush(TimedState& state, const TimedContext& context, int32_t step, TimedWorld& world);
void RunDue(TimedState& state, const TimedContext& context, int32_t step, TimedWorld& world);   // once per step

// any row
int32_t RuleIndex(const TimedState& state, std::string_view id);   // NONE: no such row
void SetEffect(TimedState& state, std::string_view name, EffectFn fn, void* context);
bool Arm(TimedState& state, int32_t rule, int32_t unit, int32_t dueStep);   // the later end stands; false: refused
void Disarm(TimedState& state, int32_t rule, int32_t unit);
int32_t DueStep(const TimedState& state, int32_t rule, int32_t unit);       // NONE: not active

} // namespace KT1::Core::TimedRules

#endif
