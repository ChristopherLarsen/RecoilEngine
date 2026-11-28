/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Targeting core: the targeting state and the decisions around the per-step choice (LUA-FREE H01, step S19). No
// engine include: the adapter (Rules/Targeting) implements TargetingWorld, tests give their own.
//
// REQ CMB-004, CMB-016, CMB-018, CMB-019, CMB-024, CMB-027, ORD-002. Replaces cmb_targeting.lua and unit_cmd_gun.lua.
// NOT declared here: the per-step choice itself (due wheel, scan, holds, applying the target) runs in the fork already
// (Lua/SupComTargeting.cpp, on by default through unitpass_target) and is reused; the adapter feeds it ranks, rules,
// guns, focus and shots, and reads its targets back. Declared here is what the Lua still computes beside it: ranks, scan
// delays, shots in flight, the order refusals, the Allow answers, Engage for orders, and the command unit's gun.
// Columns (check, trace, replay): target and scan_due = guns, focus, in_flight = inFlight (damage, not a count),
// allow_answer = answer.

#ifndef KT1_CORE_TARGET_STATE_H
#define KT1_CORE_TARGET_STATE_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"
#include "Game/KT1/Core/Targeting/TargetingConfig.h"

namespace KT1::Core::Targeting {

constexpr int32_t NONE = -1;
constexpr int32_t MAX_UNIT_WEAPONS = 16;    // weapon numbers 1 .. this

// why an order on a unit is refused (the rules param cmb_refused; RefusalName gives the Lua's string)
enum class Refusal : uint8_t { None, BelowWaterOnly, NoAirTarget, AirToAirOnly, TargetBelowWater, NoSurfaceTarget,
	TargetAboveWater, CannotTarget };
const char* RefusalName(Refusal reason);    // "" for None

// the target's movement layer (REQ CMB-018, MOV-009); Unknown: the height rule stands in
enum class Layer : uint8_t { Unknown, Land, Seabed, Water, Sub };

struct LayerReach {
	bool air = false;
	bool surface = false;          // land, sea floor or water surface
	bool under = false;            // under water
	bool seafloor = false;
	bool above = false;            // customParams.above_water_only
	bool below = false;            // customParams.below_water_only
	bool noGround = false;         // customParams.no_ground_target
};

// per weapon def, made at Start from def data
struct WeaponTargets {
	bool scanned = false;          // customParams.scan_interval: this system chooses its targets
	bool manualFire = false;       // a manual-fire weapon: its orders are checked against manualReach
	bool commandGun = false;       // the command gun config's weapon
	bool direct = false;           // a straight shot meets terrain
	bool retarget = false;         // customParams.recheck_target
	float scanInterval = 0;        // seconds
	float track = 0;               // customParams.tracking_factor or the default
	float rangeMin = 0;            // elmos
	LayerReach reach;              // customParams.targets
	LayerReach manualReach;        // the same, "land,water surface" when the list is empty
	std::vector<std::vector<std::string>> priorities;   // groups of classes a target must all have ("any": one group)
};

// per unit def
struct UnitTargets {
	std::vector<std::string> classes;   // customParams.target_classes
	std::array<int32_t, MAX_UNIT_WEAPONS> weapons{};    // weapon def by weapon number - 1; 0: no weapon
	int32_t armorType = 0;
	bool command = false, mobile = false, defence = false, constructor = false, plant = false, generator = false,
		sensorPost = false, barrier = false;   // the project's own class names
	bool air = false;              // canFly
	bool armed = false;
};

struct TargetingDefs {
	std::vector<WeaponTargets> weapons;          // by weapon def id
	std::vector<UnitTargets> units;              // by unit def id
	std::vector<std::vector<uint8_t>> ranks;     // [weapon def][unit def]; 0: not a target (BuildRanks)
	int32_t Rank(int32_t weaponDef, int32_t unitDef) const {
		if (weaponDef < 0 || static_cast<size_t>(weaponDef) >= ranks.size()) return 0;
		const std::vector<uint8_t>& row = ranks[weaponDef];
		return (unitDef >= 0 && static_cast<size_t>(unitDef) < row.size()) ? row[unitDef] : 0;
	}
};

// the engine's command ids
struct CommandIds {
	int32_t attack = 0, manualFire = 0, insert = 0;
	int32_t internalOption = 0;    // CMD.OPT_INTERNAL
};

// the config in sim units
struct TargetingParams {
	int32_t stepsPerSecond = 1;
	int32_t stepsPerTick = 1;      // max(1, floor(ref tick * rate + 0.5))
	int32_t skipSteps = 0;
	float refTick = 0;
	int32_t extraTicks = 0;
	float engageInset = 0;         // elmos
	float overkillFactor = 0;
	float lineSampleStep = 1, lineClearance = 0, lineFriendPad = 0;
	bool lineFriends = false;
	float inflightEpsilon = 0;
	int32_t scanPhaseFactor = 0;
	int32_t gunScanSteps = 1;
	float gunInset = 0, gunTrack = 0, gunRangeMin = 0, gunRangeMax = 0;   // elmos
	CommandIds commands;
};

TargetingParams MakeTargetingParams(const TargetingConfig& config, const CommandIds& commands, int32_t stepsPerSecond);

// REQ SIM-RATE-001 R2: a scan interval in seconds as a delay in steps (GG.Cmb.ScanFrames)
int32_t ScanSteps(const TargetingParams& params, float intervalSeconds);
// a new gun's first scan: step + 1 + (unit * factor + weapon) mod the delay
int32_t FirstScan(const TargetingParams& params, int32_t unit, int32_t weapon, int32_t scanSteps, int32_t step);
// REQ CMB-004 R4: the first priority group whose classes the def all has, from 1; 0: none
int32_t ComputeRank(const WeaponTargets& weapon, const UnitTargets& unit);
void BuildRanks(TargetingDefs& defs);
// REQ CMB-018, CMB-019: None when the reach takes a target in this layer
Refusal LayerCheck(const LayerReach& reach, bool targetAir, Layer layer, float y, float height);

// what the first command in a unit's queue is
struct CommandView {
	int32_t command = NONE;        // NONE: no command
	int32_t options = 0;
	int32_t nparams = 0;
	float p1 = 0;
};

// an order being given (AllowCommand)
struct OrderView {
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	int32_t command = NONE;
	int32_t nparams = 0;
	std::array<float, 6> params{};   // the first six parameters (an inserted order carries its own from the fourth)
};

// the engine and the engine step, as targeting reads and writes them; writes go through SimContext::Write
class TargetingWorld {
public:
	virtual ~TargetingWorld() = default;
	virtual bool ValidUnit(int32_t unit) = 0;
	virtual bool DeadUnit(int32_t unit) = 0;
	virtual int32_t UnitDef(int32_t unit) = 0;              // NONE: gone
	virtual int32_t UnitTeam(int32_t unit) = 0;             // NONE: gone
	virtual int32_t AllyTeam(int32_t unit) = 0;
	virtual bool Allied(int32_t teamA, int32_t teamB) = 0;
	virtual bool Arriving(int32_t unit) = 0;                // the rules param cmd_arriving is 1
	virtual bool ScriptAI(int32_t team) = 0;                // Spring.GetTeamLuaAI is set
	virtual int32_t LosBits(int32_t unit, int32_t allyTeam) = 0;   // 1 sight, 2 radar, 4 seen before
	virtual bool InLos(int32_t unit, int32_t allyTeam) = 0;
	virtual bool Position(int32_t unit, float (&pos)[3]) = 0;
	virtual bool MidPosition(int32_t unit, float (&pos)[3]) = 0;
	virtual float Height(int32_t unit) = 0;
	virtual float Radius(int32_t unit) = 0;
	virtual Layer MovementLayer(int32_t unit) = 0;          // the fork's supcomLayer
	virtual float GroundHeight(float x, float z) = 0;
	virtual void UnitsInCylinder(float x, float z, float radius, std::vector<int32_t>& out) = 0;   // fakes left out
	virtual void UnitsInRectangle(float x1, float z1, float x2, float z2, std::vector<int32_t>& out) = 0;
	virtual int32_t FireState(int32_t unit) = 0;            // 0: hold fire
	virtual CommandView CurrentCommand(int32_t unit) = 0;
	virtual bool WeaponRange(int32_t unit, int32_t weapon, float& range) = 0;
	virtual int32_t WeaponTarget(int32_t unit, int32_t weapon) = 0;   // the weapon's unit target; NONE
	virtual int32_t ChosenTarget(int32_t unit, int32_t weapon) = 0;   // the engine step's target; NONE
	virtual void SetWeaponTarget(int32_t unit, int32_t weapon, int32_t target) = 0;   // Spring.SetUnitTarget
	virtual void WriteRefusal(int32_t unit, Refusal reason, int32_t step) = 0;   // cmb_refused, cmb_refused_frame
};

// the columns target and scan_due, by weapon number - 1, as the choosers hold them
struct GunColumns {
	std::array<int32_t, MAX_UNIT_WEAPONS> target{};   // blank: NONE (TargetState::Init)
	std::array<int32_t, MAX_UNIT_WEAPONS> scanDue{};
};

// the last AllowCommand or AllowWeaponTarget answer about the unit's order or weapon
struct AllowAnswer {
	bool allow = true;
	Refusal reason = Refusal::None;
	int32_t step = NONE;
};

struct CommandGunner {
	bool present = false;
	int32_t target = NONE;
	int32_t nextScan = 0;
};

struct FlightShot {
	int32_t projectile = NONE;
	int32_t target = NONE;
	float damage = 0;
};

struct TargetState {
	void Init(int32_t maxUnits);

	SimHost::UnitColumns units;
	Column<GunColumns> guns;          // columns target, scan_due (the engine step's guns are mirrored here)
	Column<int32_t> focus;            // column focus: the attack-move target (GG.Cmb.focus); NONE
	Column<float> inFlight;           // column in_flight: damage of the shots flying at the unit
	Column<AllowAnswer> answer;       // column allow_answer
	Column<CommandGunner> gunner;
	SortedIdList gunners;             // units with the command gun
	std::vector<FlightShot> shots;    // ascending projectile id
};

struct TargetingContext {
	const TargetingDefs& defs;
	const TargetingParams& params;
};

void TargetUnitCreated(TargetState& state, const TargetingContext& context, int32_t unit, int32_t unitDef, int32_t step);
void TargetUnitDestroyed(TargetState& state, int32_t unit);
void SetFocus(TargetState& state, int32_t unit, int32_t target);   // NONE: cleared

// shots in flight (REQ CMB-024). true: the target's total changed and goes to the engine step
bool AddShot(TargetState& state, const TargetingContext& context, int32_t projectile, int32_t weaponDef, int32_t target,
	float damage, float& total);
bool RemoveShot(TargetState& state, const TargetingContext& context, int32_t projectile, int32_t& target, float& total);
float InFlight(const TargetState& state, int32_t target);

// REQ CMB-027 R2: the army of allyTeam holds the unit as a contact or remembers it
bool Contact(TargetingWorld& world, int32_t target, int32_t allyTeam);

struct TargetTest {
	bool ok = false;
	bool failed = false;           // a check failed (sight, layer, terrain) rather than the target being gone
};

// may this weapon take the target; noLine: the line of fire is not read
TargetTest Acceptable(const TargetingContext& context, int32_t unit, int32_t weaponDef, int32_t target, bool noLine,
	TargetingWorld& world);

struct Engagement {
	bool found = false;
	float reach = 0;               // the weapon's range less the engage inset, elmos
	int32_t rank = 0;
};

struct Engagement2 {
	Engagement primary;            // weapon 1
	Engagement best;               // over all weapons: best rank, then longest reach
};

// GG.Cmb.Engage and Engage2: nothing on hold fire. only: that weapon number alone, 0: all
Engagement Engage(const TargetingContext& context, int32_t unit, int32_t target, bool noLine, int32_t only, TargetingWorld& world);
Engagement2 Engage2(const TargetingContext& context, int32_t unit, int32_t target, bool noLine, TargetingWorld& world);
// GG.Cmb.BestKey: the best key Engage2 could give on a def: kind 0 (weapon 1) or 1; false: no weapon may take it
bool BestKey(const TargetingContext& context, int32_t unitDef, int32_t targetDef, int32_t& kind, int32_t& rank);

// REQ CMB-018 R4, CMB-019 R3: why an attack (or manual-fire) order on the target is refused; None: some weapon takes it
Refusal OrderRefusal(const TargetingContext& context, int32_t unitDef, int32_t target, bool manualFire, TargetingWorld& world);
// the AllowCommand answer; a refusal is written on the unit
bool AllowOrder(TargetState& state, const TargetingContext& context, const OrderView& order, int32_t step, TargetingWorld& world);
// the AllowWeaponTarget answer for a scanned weapon or the command gun (the priority is left as it was)
bool AllowTarget(TargetState& state, const TargetingContext& context, int32_t attacker, int32_t target, int32_t weapon,
	int32_t weaponDef, int32_t step, TargetingWorld& world);

// unit_cmd_gun.lua's pass over the command guns, ascending unit id, once per step
void RunCommandGuns(TargetState& state, const TargetingContext& context, int32_t step, TargetingWorld& world);

} // namespace KT1::Core::Targeting

#endif
