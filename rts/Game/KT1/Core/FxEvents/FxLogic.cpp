/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect logic. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Core/FxEvents/FxLogic.h"

#include "Game/KT1/Core/FxEvents/FxSamples.h"

namespace KT1::Core::Fx {

void InitFx(FxState&, const FxEventsConfig&, const FxCapacity&, float) {}
void StartStep(const FxEnv&) {}

bool Spawn(const FxEnv&, const EffectRef&, const Vec3&, const Vec3&, Priority, float) { return false; }
bool Intercept(const FxEnv&, const Vec3&, const Vec3*) { return false; }

void OnExplosion(const FxEnv&, const ExplosionIn&) {}
void OnUnitDamaged(const FxEnv&, const UnitHitIn&) {}
void OnUnitDestroyed(const FxEnv&, const UnitGoneIn&) {}
void OnUnitCreated(const FxEnv&, int32_t, int32_t, float) {}
void OnUnitFinished(const FxEnv&, int32_t) {}
void OnProjectileCreated(const FxEnv&, const ProjectileIn&) {}
void OnProjectileDestroyed(const FxEnv&, const ProjectileIn&) {}
void OnFeatureCreated(const FxEnv&, int32_t, int32_t, const Vec3&) {}
void OnFeatureDestroyed(const FxEnv&, int32_t) {}
void StepFx(const FxEnv&) {}

bool ForwardBlast(const FxEnv&, BlastClass, BlastFamily, Faction, const Vec3&, int32_t) { return false; }

bool RelayTeleport(const FxEnv&, Teleport) { return false; }
bool RelayCrash(const FxEnv&, int32_t) { return false; }
bool RelayRemnant(const FxEnv&, int32_t, const Vec3&) { return false; }
bool RelaySacrifice(const FxEnv&, Sacrifice) { return false; }

} // namespace KT1::Core::Fx
