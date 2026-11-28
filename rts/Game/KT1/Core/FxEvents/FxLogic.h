/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect logic as free functions (replaces fx_core, fx_build_sets, fx_death_sets,
// fx_script_relay, fx_events and the synced halves of gfx_scorch, fx_blasts, gfx_criteria_fx, gfx_fog_death_puff and
// fx_depth). Wreck physics: FxWreck.h. The missile relay: FxStrategic.h. No engine include.
//
// Look only: nothing here changes the game except the wrecks' places and the unit wreck's draw flag.

#ifndef KT1_FX_LOGIC_H
#define KT1_FX_LOGIC_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxState.h"

namespace KT1::Core::Fx {

struct FxEnv {                          // what every call needs
	const FxEventsConfig& config;
	const FxDefs& defs;
	const WorldQuery& world;
	FxState& state;
	FxOutput& out;
	int32_t step = 0;
};

struct FxCapacity { int32_t units = 0, features = 0, weaponDefs = 0; };

// after Rate is set and defs are built; scale: the mod option fx_scale, else config.core.scale
void InitFx(FxState& s, const FxEventsConfig& config, const FxCapacity& capacity, float scale);
void StartStep(const FxEnv& env);       // the budget's step, the per step limits, the Scale event

// the effect budget (GG.Fx.Spawn): false when pooled, unknown or refused
bool Spawn(const FxEnv& env, const EffectRef& effect, const Vec3& pos, const Vec3& dir, Priority priority, float length = 0.0f);
bool Intercept(const FxEnv& env, const Vec3& pos, const Vec3* from);     // GG.Fx.Intercept

// sim events, in the order raised
struct ExplosionIn { int32_t weaponDef = -1, owner = -1; Vec3 pos; };
// hit set, own and gated marks, depth hit, script tree, scorch request, blast core and blast, wreck push, Impact
void OnExplosion(const FxEnv& env, const ExplosionIn& in);

struct UnitHitIn { int32_t unit = -1, attacker = -1, weaponDef = -1; float damage = 0.0f; };
void OnUnitDamaged(const FxEnv& env, const UnitHitIn& in);               // confirmation; damage-state tracking

struct UnitGoneIn { int32_t unit = -1; UnitNow now; };                   // read while the unit still exists
void OnUnitDestroyed(const FxEnv& env, const UnitGoneIn& in);            // death sets, Death, FogDeath, CritDeath, giant ring
void OnUnitCreated(const FxEnv& env, int32_t unit, int32_t defId, float build);
void OnUnitFinished(const FxEnv& env, int32_t unit);

struct ProjectileIn { int32_t projectile = -1, weaponDef = -1, owner = -1; Vec3 pos, vel; };
void OnProjectileCreated(const FxEnv& env, const ProjectileIn& in);      // ShotStart, DepthLaunch
void OnProjectileDestroyed(const FxEnv& env, const ProjectileIn& in);    // ShotEnd
void OnFeatureCreated(const FxEnv& env, int32_t feature, int32_t defId, const Vec3& pos);   // hides a unit wreck
void OnFeatureDestroyed(const FxEnv& env, int32_t feature);

// the end of the step: due death sets, damage states, build sets, wrecks to show again, wrecks in motion
void StepFx(const FxEnv& env);

// a classified blast from a rule (GG.CombatFx.Forward); false: over the step's limit
bool ForwardBlast(const FxEnv& env, BlastClass cls, BlastFamily family, Faction faction, const Vec3& pos, int32_t weaponDef);

// abilities (GG.FxEvents); false: over the step's limit
bool RelayTeleport(const FxEnv& env, Teleport event);                    // y and seen are filled here
bool RelayCrash(const FxEnv& env, int32_t unit);
bool RelayRemnant(const FxEnv& env, int32_t unit, const Vec3& pos);
bool RelaySacrifice(const FxEnv& env, Sacrifice event);                  // sizes and seen are filled here

} // namespace KT1::Core::Fx

#endif
