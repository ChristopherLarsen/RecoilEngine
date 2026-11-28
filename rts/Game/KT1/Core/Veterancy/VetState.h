/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Veterancy core: experience, levels and shared kills, the state and decisions of unit_veterancy.lua (LUA-FREE H01,
// step S26). No engine include: the adapter (Rules/Veterancy) implements VetWorld. Draw code includes this header for
// VetMark, published as Snapshot<VetMark>.
//
// REQ VET-001 v3, VET-002 v3, VET-003 v2, ORD-014 v3 R5. The adapter hears UnitDamaged, UnitDestroyed and UnitGiven
// synchronously, in the Lua's order (a dying unit's health and cargo are read there; death_blast asks for a credit from
// inside the damage chain). RunRegen runs after the move, once per step. The mod option veterancy "0" keeps the system
// off. Columns (check, trace, replay): xp, level, allow_transfer = captureNow.

#ifndef KT1_CORE_VET_STATE_H
#define KT1_CORE_VET_STATE_H

#include <cstdint>
#include <string>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/Damage/CombatEvents.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"
#include "Game/KT1/Core/Veterancy/VetConfig.h"

namespace KT1::Core::Veterancy {

constexpr int32_t NONE = -1;

// one unit's veterancy as the interface shows it (the rules params vet_level, vet_xp, vet_prev, vet_next, vet_regen);
// a unit with no experience is not in the snapshot
struct VetMark {
	int32_t level = 0;
	float xp = 0;
	float prev = 0;                // this level's threshold; 0 at level 0
	float next = 0;                // the next one; 0 at the last level or with none
	float regen = 0;               // health a second the level adds
};

enum class VetClass : uint8_t { T1, T2, T3, SupportCommand, Experimental, Command, Other };

// per unit def, made at Start from def data
struct VetDef {
	std::string code;              // customParams.eco_code, else parity_code
	VetClass cls = VetClass::Other;
	int32_t tier = 0;              // customParams.eco_tier; 0: none
	float ownFactor = -1;          // customParams.vet_mass_factor; below 0: none
	bool command = false;          // customParams.command_unit is "1"
	bool naval = false;            // icon_outline is one of naval_outlines
	bool canGain = false;          // REQ VET-001 R5: armed and vet_enabled is not "0"
	float mass = 0;                // metal cost
	float health = 0;              // the def's maximum health
};

struct VetDefs {
	std::vector<VetDef> units;     // by unit def id
	const VetDef* Unit(int32_t id) const {
		return (id >= 0 && static_cast<size_t>(id) < units.size()) ? &units[id] : nullptr;
	}
};

struct VetContext {
	const VetConfig& config;
	const VetFixture* fixture;     // the fixture in use; nullptr: none
	const VetDefs& defs;
	int32_t stepsPerSecond;        // regeneration per step: per second / this
};

// the engine and the other rules, as veterancy reads and writes them; writes go through SimContext::Write
class VetWorld {
public:
	virtual ~VetWorld() = default;
	virtual bool ValidUnit(int32_t unit) = 0;
	virtual bool DeadUnit(int32_t unit) = 0;
	virtual int32_t UnitDef(int32_t unit) = 0;                  // NONE: gone
	virtual int32_t UnitTeam(int32_t unit) = 0;                 // NONE: gone
	virtual bool Allied(int32_t teamA, int32_t teamB) = 0;
	virtual bool Health(int32_t unit, float& health, float& maxHealth) = 0;
	virtual bool BeingBuilt(int32_t unit, float& progress) = 0;
	// the engine's transporter, else the cargo rule's carrier (GG.CargoDeath.Carrier); NONE: not carried
	virtual int32_t Carrier(int32_t unit) = 0;
	// the engine's cargo in load order, then a carrier's stored aircraft (GG.SpcCargo.stored)
	virtual void Cargo(int32_t transport, std::vector<int32_t>& out) = 0;
	virtual void SetMaxHealth(int32_t unit, float maxHealth) = 0;
	virtual void SetHealth(int32_t unit, float health) = 0;
	virtual void WriteParams(int32_t unit, const VetMark& mark) = 0;   // the five rules params, in sight
	virtual void Show(const VetLevelUp& event) = 0;
};

struct VetShare {
	int32_t attacker = NONE;
	float damage = 0;
};

struct VetState {
	void Init(int32_t maxUnits);

	SimHost::UnitColumns units;                  // a unit enters at its first record and leaves at its death
	Column<float> xp;                            // column xp
	Column<uint8_t> hasXp;                       // the Lua's xp[u] ~= nil
	Column<uint8_t> level;                       // column level
	Column<float> baseMax;                       // the maximum health before the levels; below 0: not read yet
	Column<float> cost;                          // a fixture victim's mass cost; below 0: the def's
	Column<float> regen;                         // health a second; 0: none
	Column<uint8_t> captureNow;                  // column allow_transfer: the transfer being made is a capture
	Column<std::vector<VetShare>> dealt;         // by victim: damage by attacker, ascending attacker (REQ VET-003 R1)
	Column<float> dealtSum;                      // all of it, dead attackers' included
	Column<std::vector<int32_t>> victims;        // by attacker: the units it hit
	Column<std::vector<float>> cargoDue;         // by transport: shares of cargo that died before it, this step
	Column<int32_t> cargoStep;                   // the step of cargoDue
	Column<int32_t> cargoPaid;                   // the step a cargo unit's share was taken
	SortedIdList regenerating;
};

float Factor(const VetContext& context, int32_t unitDef);                    // REQ VET-001 R3, R4
bool Threshold(const VetContext& context, int32_t unitDef, int32_t level, float& out);   // false: no such level
float RegenAt(const VetContext& context, int32_t unitDef, int32_t level);    // REQ VET-002 R5
float ClassFactor(const VetContext& context, VetClass cls);                  // 0: the class has none
VetMark Mark(const VetState& state, const VetContext& context, int32_t unit, int32_t unitDef);

// a kill share worth `mass` to a unit (GG.Vet.Credit): capped at the band, at most one level (REQ VET-002 R1)
void Credit(VetState& state, const VetContext& context, int32_t unit, float mass, VetWorld& world);
void SetBase(VetState& state, const VetContext& context, int32_t unit, float maxHealth, VetWorld& world);   // GG.Vet.SetBase
void SetCost(VetState& state, int32_t unit, float mass);                     // GG.Vet.SetCost
// the economy's capture: the new unit takes the old one's experience and level (REQ VET-003 R4)
void Carry(VetState& state, int32_t unit, bool hasXp, float xp, int32_t level);
void Rebuild(VetState& state, const VetContext& context, int32_t unit, VetWorld& world);   // REQ VET-003 R3

struct VetDamage {
	int32_t unit = NONE;
	int32_t team = NONE;
	int32_t attacker = NONE;
	int32_t attackerTeam = NONE;
	float damage = 0;
	bool paralyzer = false;
};

struct VetDeath {
	int32_t unit = NONE;
	int32_t unitDef = NONE;
	int32_t team = NONE;
	int32_t attacker = NONE;
	int32_t attackerTeam = NONE;
};

void VetUnitDamaged(VetState& state, const VetDamage& damage, VetWorld& world);
void VetUnitDestroyed(VetState& state, const VetContext& context, const VetDeath& death, int32_t step, VetWorld& world);
// REQ ORD-014 R5: a transport killed in flight pays its cargo's shares at the killing hit (GG.Vet.CreditCargo)
void CreditCargo(VetState& state, const VetContext& context, int32_t transport, int32_t team, int32_t attacker, int32_t attackerTeam,
	int32_t step, VetWorld& world);
void VetAllowTransfer(VetState& state, int32_t unit, bool capture);
void VetUnitGiven(VetState& state, const VetContext& context, int32_t unit, VetWorld& world);
void RunRegen(VetState& state, const VetContext& context, VetWorld& world);

} // namespace KT1::Core::Veterancy

#endif
