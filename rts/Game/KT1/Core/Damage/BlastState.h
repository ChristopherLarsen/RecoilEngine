/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: death blasts and falling aircraft, the state and decisions of death_blast.lua (LUA-FREE H01, S21).
// No engine include: the adapter (Rules/Damage) implements BlastWorld, tests give their own.
//
// REQ SPC-014 v3, v4 R6 (giant units), CMB-008 v1 R5 and v2 R7 (death blasts, falling aircraft), MOV-014 (the fall),
// ORD-014 (cargo of a dying transport), CMB-007 (armour). The adapter hears UnitDamaged and UnitDestroyed
// synchronously: the fall starts at the killing hit, before the engine kills the unit. UnitCreated clears the marks an
// earlier unit with the id left. RunBlasts runs after the move: queued blasts, water contacts, worked-out falls.
// Columns (check, trace, replay): blast = lastBlast.

#ifndef KT1_CORE_BLAST_STATE_H
#define KT1_CORE_BLAST_STATE_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/Damage/DamageState.h"

namespace KT1::Core::Damage {

enum class BlastKind : uint8_t { Death, Ground, Crash, Structure, FallImpact, CrashWorkedOut, Landed };
const char* BlastKindName(BlastKind kind);   // "death" .. "fall-impact", "crash-worked-out", "landed"

struct BlastZone {
	float damage = 0;
	float radius = 0;              // elmos
	int32_t damageType = NONE;     // ArmourTable::TypeIndex
};

// per unit def
struct BlastDef {
	bool present = false;          // the def has a death effect
	bool crash = false;            // a giant aircraft: the blast where it lands
	bool air = false;              // an aircraft that is not a giant crash: it falls all the way
	bool fall = false;             // the move type has a fall of its own (supcom_air_fall_gravity above 0)
	bool shield = false;           // the def has a shield weapon
	bool hasAlso = false;          // a giant structure's second zone
	BlastKind kind = BlastKind::Death;
	BlastZone zone, also;
	float mass = 0;                // metal cost: the kill credit of a crash
};

// the def data MakeBlastDef reads (spc_death.lua ForDef)
struct BlastDefSource {
	std::string code;              // customParams.eco_code
	std::string deathBlast;        // customParams.spc_death_blast: "damage,radius[,type]"
	std::string deathBlast2;       // customParams.spc_death_blast2: "inner damage,radius,outer damage,radius[,type]"
	bool deathCrash = false;       // customParams.spc_death_crash is "1"
	bool command = false;          // the command unit: its blast is not this system's
	int32_t tier = 0;              // customParams.eco_tier
	bool mobile = false;           // canMove
	bool canFly = false;
	bool aircraft = false;         // customParams.eco_mode is "aircraft"
	bool shield = false;
	float fallGravity = 0;         // customParams.supcom_air_fall_gravity
	float mass = 0;
};

// def parameters first, else the config's roster values by code; the armour table gives the damage type's index
BlastDef MakeBlastDef(const BlastDefSource& source, const DamageConfig& config, const ArmourTable& armour);

struct BlastDefs {
	std::vector<BlastDef> units;   // by unit def id
	const BlastDef* Unit(int32_t id) const {
		return (id >= 0 && static_cast<size_t>(id) < units.size()) ? &units[id] : nullptr;
	}
};

// the config in sim units
struct BlastParams {
	int32_t beatSteps = 0;         // max(0, min(floor(beat * rate + 0.5), floor(death delay * rate) - 1))
	int32_t landingBoundSteps = 0;
	int32_t weaponDef = NONE;      // the blast weapon
	float queryPad = 0;
	float inFlightHeight = 0;
	float fallGravity = 0;         // elmos per step per step
};

BlastParams MakeBlastParams(const DamageConfig& config, int32_t stepsPerSecond, int32_t blastWeaponDef);

enum class AirState : uint8_t { NotAircraft, Landed, Flying, Crashing };

struct BlastVictim {
	int32_t unit = NONE;
	int32_t team = NONE;
	float distance = 0;
	float amount = 0;
};

// one blast with what it hit: the check column blast, and the test log (GG.SpcExp.log)
struct BlastRecord {
	int32_t step = 0;
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	BlastKind kind = BlastKind::Death;
	float pos[3] = {};
	float radius = 0;
	float damage = 0;
	std::vector<BlastVictim> hits;
};

class BlastWorld : public World {
public:
	virtual int32_t UnitDef(int32_t unit) = 0;
	virtual bool BeingBuilt(int32_t unit) = 0;
	virtual float BuildProgress(int32_t unit) = 0;
	virtual bool Transported(int32_t unit) = 0;
	virtual AirState Air(int32_t unit) = 0;                       // the move type's aircraftState
	virtual bool Velocity(int32_t unit, float (&velocity)[3]) = 0;
	virtual float GroundHeight(float x, float z) = 0;
	virtual bool StartCrash(int32_t unit) = 0;                    // SetUnitCrashing; true: it is crashing now
	virtual void SetNeutral(int32_t unit) = 0;
	virtual void StopOrder(int32_t unit) = 0;
	virtual void SensorsOff(int32_t unit) = 0;                    // los, airLos, radar, sonar, seismic to 0
	virtual void ShieldOff(int32_t unit) = 0;
	virtual void Destroy(int32_t unit) = 0;                       // Spring.DestroyUnit(unit, false, false)
	virtual bool CargoNoBlast(int32_t unit, int32_t weaponDef) = 0;   // GG.CargoDeath.NoBlast
	virtual void CargoFall(int32_t unit, int32_t team, int32_t attacker, int32_t attackerTeam) = 0;   // GG.CargoDeath.Fall
	// shields in a blast (GG.Shields.Blast and Shelter): NONE shelters nobody
	virtual int32_t ShieldBlast(const float (&pos)[3], float radius, float damage, int32_t damageType, int32_t source) = 0;
	virtual float Shelter(int32_t shelter, int32_t unit, float amount) = 0;
	virtual void CreditKill(int32_t attacker, float mass) = 0;    // veterancy
	virtual void CrashTrail(int32_t unit) = 0;                    // look: GG.FxEvents.Crash
	virtual void CrashBurst(const float (&pos)[3]) = 0;           // look: GG.CombatFx.Forward
	virtual void Scorch(const float (&pos)[3]) = 0;               // look: EFFECTS' scorch event
	virtual void LogBlast(const BlastRecord& record) = 0;         // the giant units' log line
};

struct AirFallState {
	int32_t from = NONE;           // the step of the killing hit; NONE: not falling
	int32_t team = NONE;
	int32_t contact = NONE;        // the step it met ground or water
};

struct CrashKill {
	bool set = false;
	int32_t attacker = NONE;
	int32_t team = NONE;
	int32_t step = 0;
};

struct CrashContact {
	bool set = false;
	int32_t step = 0;
	int32_t from = 0;
	float pos[3] = {};
};

struct QueuedBlast {
	int32_t atStep = 0;
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	float pos[3] = {};
	BlastKind kind = BlastKind::Death;
};

struct BlastState {
	void Init(int32_t maxUnits);

	DenseIndex marked;                       // units with a mark; records outlive their unit until the id is created again
	Column<int32_t> crashingSince;           // GG.SpcExp.crashing; NONE: not crashing
	Column<AirFallState> airFall;
	Column<CrashKill> crashKill;             // GG.SpcExp.crashKill (statistics)
	Column<CrashContact> crashContact;       // GG.SpcExp.crashContact (cargo)
	Column<uint8_t> noBlast;                 // GG.RemoveWithoutBlast: removed, not dead
	SortedIdList falling;                    // the units in airFall
	std::vector<QueuedBlast> beats;          // death blasts waiting for their beat, in queue order
	std::vector<QueuedBlast> falls;          // falls worked out here
	BlastRecord lastBlast;
	std::vector<BlastRecord> log;            // while keepLog (tests)
	bool keepLog = false;
	int32_t work = 0;                        // steps that had worked-out falls (tests)
};

struct BlastContext {
	const DamageDefs& damage;
	const BlastDefs& blasts;
	const ArmourTable& armour;
	const BlastParams& params;
};

struct UnitDamage {
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	int32_t attacker = NONE;
	int32_t attackerTeam = NONE;
	bool paralyzer = false;
};

struct UnitDeath {
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	int32_t weaponDef = NONE;
	bool removal = false;          // an engine damage source that is a removal (reclaimed, unfinished ...): no blast
	bool aircraftCrashed = false;  // the engine's own crash ended
};

void BlastUnitCreated(BlastState& state, int32_t unit);
void MarkNoBlast(BlastState& state, int32_t unit);
void BlastUnitDamaged(BlastState& state, const BlastContext& context, const UnitDamage& damage, int32_t step, BlastWorld& world);
void BlastUnitDestroyed(BlastState& state, const BlastContext& context, const UnitDeath& death, int32_t step, BlastWorld& world);
void RunBlasts(BlastState& state, const BlastContext& context, int32_t step, BlastWorld& world);

} // namespace KT1::Core::Damage

#endif
