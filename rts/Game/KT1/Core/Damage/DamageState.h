/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Damage core: the state and the decisions of cmb_damage.lua and cmb_impact_patterns.lua (LUA-FREE H01, step S21).
// No engine include: the adapter (Rules/Damage) implements World, tests give their own. death_blast.lua: BlastState.h.
//
// REQ CMB-006, CMB-008, CMB-022. The UnitPreDamaged order is the gadgets' layer order: DamageRules (layer -10), then
// PatternRules (layer -9) on its answer. The pulses and splits run after the move, in the order RunDotPulses, RunSplits,
// RunPatternPulses. AddDamage re-enters the chain from inside a run: a porter walks the pulse lists by index, and the
// state's in* marks say which run is on, as the Lua's do.
// The adapter hears Explosion, ProjectileCreated, ProjectileDestroyed and UnitDestroyed synchronously (the Lua acts
// inside the call-in: an event batch is too late). A dying unit stops colliding with shots (CMB-008 R5): the adapter
// calls World::ClearBlocking from UnitDestroyed.
// Columns (check, trace, replay): hit_result and rule_hit = lastHit, dot_entries = dots and stages, last_kill = lastKill.

#ifndef KT1_CORE_DAMAGE_STATE_H
#define KT1_CORE_DAMAGE_STATE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Damage/CombatEvents.h"
#include "Game/KT1/Core/Damage/DamageConfig.h"
#include "Game/KT1/Core/Damage/DamageTypes.h"

namespace KT1::Core::Damage {

// per weapon def, made at Start from def data
struct WeaponRules {
	bool rules = false;            // customParams.scan_interval: cmb_damage's rules apply
	bool friendlyFire = false;     // customParams.friendly_fire
	bool selfDamage = false;       // customParams.self_damage
	bool patternDot = false;       // incendiary: the pulses are the pattern's, not cmb_damage's
	bool split = false;            // split_on_water with a split_count
	bool splitDecaying = false;    // customParams.split_weave is "decaying"
	bool hasHitEffect = false;     // customParams.fx_hit (the second stage's look)
	Pattern pattern = Pattern::None;
	float damage = 0;              // damages[0]
	float patternDamage = 0;       // damages.default or damages[0] or customParams.damage
	float initial = 0;             // customParams.initial_damage
	float dotTime = 0;             // seconds
	int32_t dotPulses = 0;
	float aoe = 0;                 // damageAreaOfEffect or areaOfEffect, as the Lua value
	float radius = 0;              // aoe / 2: cmb_damage's area test and pulses, and the pattern's pulses
	float splashRadiusMu = 0;      // > 0: the first weapon of a splash unit (aoe / elmos_per_mu)
	int32_t splitCount = 0;
	float childWeaveMu = 0;        // customParams.split_child_weave
	float childWeaveFrequency = 0;
	float selfDestructS = -1;      // below 0: none
};

// per unit def
struct UnitRules {
	bool damageable = true;        // customParams.cmb_damageable is not "0"
	bool profiled = false;         // the def has an economy profile (customParams.eco_code)
	int32_t armourClass = 0;       // ArmourTable::ClassIndex of cmb_armour_class, armour_profile or the default
};

struct DamageDefs {
	std::vector<WeaponRules> weapons;   // by weapon def id
	std::vector<UnitRules> units;       // by unit def id
	// nullptr: an engine damage source or an unknown id
	const WeaponRules* Weapon(int32_t id) const {
		return (id >= 0 && static_cast<size_t>(id) < weapons.size()) ? &weapons[id] : nullptr;
	}
	const UnitRules* Unit(int32_t id) const {
		return (id >= 0 && static_cast<size_t>(id) < units.size()) ? &units[id] : nullptr;
	}
};

// the config in sim units, made at setup and again after a fixture overlay
struct DamageParams {
	float deathThreshold = 0;
	float splashQueryPad = 0;
	float elmosPerMu = 0;
	int32_t stepsPerSecond = 1;        // pulse times: step + floor(seconds * stepsPerSecond + 0.5)
	float twoStageFirst = -1;          // below 0: unset
	float twoStageSecond = -1;
	float twoStageDelayS = -1;
	float weaveHoldS = 0, weaveStartMu = 0, weaveStepS = 0, weaveEndMu = 0, weaveEndFrequency = 0;
	float weaveFrequencyPerAmplitude = 0, weaveMinFrequency = 0;
	std::vector<float> weaveStepsMu;
};

DamageParams MakeParams(const DamageConfig& config, int32_t stepsPerSecond);

// customParams-class and damage-type multipliers (CMB-007); also read by shields and the death blast
class ArmourTable {
public:
	void Build(const DamageConfig& config);
	int32_t ClassIndex(std::string_view name) const;   // an unlisted class: the default class
	int32_t TypeIndex(std::string_view type) const;    // an empty type: the default type; NONE: no class lists it
	float Mult(int32_t classIndex, int32_t typeIndex) const;   // REQ CMB-007 R2, R3: unlisted counts 1

private:
	std::vector<std::string> classes, types;
	std::vector<float> mults;                          // classes x types
	int32_t fallbackClass = NONE, fallbackType = NONE;
};

// what a projectile is aimed at, as the engine reports it
struct ProjectileAim {
	bool present = false;
	bool ground = false;           // pos is the target; else id
	int32_t type = 0;              // the engine's target type, handed back unchanged
	int32_t id = NONE;
	float pos[3] = {};
};

// the engine and the other families' rules, as the damage system reads and writes them. Writes go through
// SimContext::Write in the adapter. Unit lists come in the order the Lua's query gave them, intel's fakes left out.
class World {
public:
	virtual ~World() = default;
	virtual bool ValidUnit(int32_t unit) = 0;                       // Spring.ValidUnitID
	virtual bool DeadUnit(int32_t unit) = 0;                        // Spring.GetUnitIsDead
	virtual int32_t UnitTeam(int32_t unit) = 0;                     // NONE: gone
	virtual bool Allied(int32_t teamA, int32_t teamB) = 0;
	virtual bool Truce(int32_t attackerTeam, int32_t team) = 0;     // GG.Options.Truce; false with no truce
	virtual bool Health(int32_t unit, float& health, float& maxHealth) = 0;
	virtual bool Position(int32_t unit, float (&pos)[3]) = 0;
	virtual void UnitsInSphere(const float (&pos)[3], float radius, std::vector<int32_t>& out) = 0;
	virtual void UnitsInCylinder(float x, float z, float radius, std::vector<int32_t>& out) = 0;
	virtual void AddDamage(int32_t unit, float amount, int32_t attacker, int32_t weaponDef) = 0;   // Spring.AddUnitDamage
	virtual void ClearBlocking(int32_t unit) = 0;                   // SetUnitBlocking(unit, false, false, false)
	virtual void Show(const DotTick& tick) = 0;
	virtual void HitEffect(int32_t weaponDef, const float (&pos)[3]) = 0;   // the second stage's look (GG.Fx.Spawn)
	// split torpedoes
	virtual bool ProjectilePosition(int32_t projectile, float (&pos)[3]) = 0;     // false: gone
	virtual void ProjectileVelocity(int32_t projectile, float (&velocity)[3]) = 0;
	virtual ProjectileAim Aim(int32_t projectile) = 0;
	// Spring.SpawnProjectile with the parent's weapon, place, velocity, owner and team; NONE: refused
	virtual int32_t SpawnChild(int32_t weaponDef, const float (&pos)[3], const float (&velocity)[3], int32_t owner, int32_t team) = 0;
	virtual void SetAim(int32_t projectile, const ProjectileAim& aim) = 0;   // the engine target and the flight's (projectiles)
	// the guided flight's weave (GG.CmbProjectiles.flights: zig in elmos, redraw every n steps); false: no flight
	virtual bool SetWeave(int32_t projectile, float amplitude, int32_t everySteps) = 0;
	virtual void DeleteProjectile(int32_t projectile) = 0;
	virtual void BurstProjectile(int32_t projectile) = 0;           // Spring.SetProjectileCollision
};

// what one shot has already done (REQ CMB-006 R5, R6): cmb_damage's firstHit and dotDone
struct ShotMemory {
	int32_t projectile = NONE;
	bool first = false;                 // a point hit: the initial damage was counted
	bool dot = false;                   // an area shot's pulses were scheduled
	std::vector<int32_t> hit;           // an area shot: the units that had the initial damage
};

// a split torpedo before and after it enters the water
struct SplitParent {
	int32_t projectile = NONE;
	int32_t weaponDef = NONE;
	int32_t owner = NONE;
};

struct SplitChild {
	int32_t projectile = NONE;
	int32_t weaponDef = NONE;
	int32_t bornStep = 0;
};

struct DamageState {
	void Init(int32_t maxUnits);

	DenseIndex hurt;                    // units that were hit; a record outlives its unit until the id is hit again
	Column<HitResult> lastHit;          // columns hit_result, rule_hit
	Column<KillRecord> lastKill;        // column last_kill
	std::vector<ShotMemory> shots;      // ascending projectile id
	std::vector<DotPulse> dots;         // schedule order
	std::vector<PatternPulse> stages;   // schedule order
	std::vector<SplitParent> parents;   // ascending projectile id
	std::vector<SplitChild> children;   // ascending projectile id
	bool inDot = false;                 // RunDotPulses is giving damage
	bool inSplash = false;              // SplashExplosion is giving damage
	bool inStage = false;               // RunPatternPulses is giving damage
	int32_t splits = 0;                 // torpedoes that split (GG.CmbPatterns.splits)
};

// UnitPreDamaged, cmb_damage's rules; records lastKill
HitResult DamageRules(DamageState& state, const DamageDefs& defs, const DamageParams& params, const Hit& hit, int32_t step, World& world);
// UnitPreDamaged, cmb_impact_patterns' rules on that answer; records lastHit (the call's result)
HitResult PatternRules(DamageState& state, const DamageDefs& defs, const DamageParams& params, const Hit& hit, const HitResult& in);

// Explosion, cmb_damage: the full hit to every unit in a splash weapon's radius (edge inside); returns the units hit
int32_t SplashExplosion(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t weaponDef, const float (&pos)[3],
	int32_t attacker, World& world);
// Explosion, cmb_impact_patterns (and GG.CmbPatterns.Impact): schedules a pattern's pulses; false: no pattern
bool PatternImpact(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t weaponDef, const float (&pos)[3],
	int32_t attacker, int32_t step);

void ProjectileCreated(DamageState& state, const DamageDefs& defs, int32_t projectile, int32_t owner, int32_t weaponDef);
void ProjectileDestroyed(DamageState& state, int32_t projectile);

void RunDotPulses(DamageState& state, const DamageDefs& defs, int32_t step, World& world);
void RunSplits(DamageState& state, const DamageDefs& defs, const DamageParams& params, int32_t step, World& world);
void RunPatternPulses(DamageState& state, const DamageDefs& defs, int32_t step, World& world);

// cmb_drop.lua D.SplitWeave: amplitude (map units) and redraws a second, `age` seconds after the split (REQ CMB-022 R5)
void SplitWeave(const DamageParams& params, const WeaponRules& weapon, float age, float& amplitudeMu, float& frequency);

} // namespace KT1::Core::Damage

#endif
