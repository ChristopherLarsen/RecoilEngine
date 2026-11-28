/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxEvents: what other native systems ask the synced effect system (LUA-FREE H01, EFFECTS 4; step S30). It gives
// what GG.Fx, GG.FxEvents, GG.CombatFx and GG.WreckPhysics give the Lua today.
//
// Synced. Ids and values only. While the system is not live (off or check) every call does nothing and answers its
// neutral value, and Active() is false: the caller then asks the Lua. Display events a rule owns (Scorch, ShieldHit,
// ShieldState; Core/FxEvents/FxEvent.h) are sent with DisplayEvents::Send, not through here.

#ifndef KT1_FX_EVENTS_API_H
#define KT1_FX_EVENTS_API_H

#include <cstdint>
#include <span>
#include <string_view>

#include "Game/KT1/Core/FxEvents/FxBudget.h"
#include "Game/KT1/Core/FxEvents/FxStrategic.h"
#include "Game/KT1/Core/FxEvents/FxTypes.h"

class float3;

namespace KT1::Fx {

bool Active();

// ---------------------------------------------------------------- effects (GG.Fx)

// an effect's id by name, for Spawn; look up once. Core::Fx::NO_EFFECT: unknown
int32_t EffectId(std::string_view name);
// the library name sp_<faction>_<class>_<scale>_<part> written into `out` (cut to size); false: it did not fit
bool Name(std::string_view faction, std::string_view fxClass, std::string_view scale, std::string_view part,
	char* out, size_t size);
bool Spawn(int32_t effect, const float3& pos, const float3& dir, Core::Fx::Priority priority, float length = 0.0f);
bool Spawn(std::string_view name, const float3& pos, const float3& dir, Core::Fx::Priority priority, float length = 0.0f);
// movement dust, spray, wake ...: priority "state"; the option decides which kinds draw
bool Move(Core::Fx::MoveKind kind, Core::Fx::SizeBucket size, const float3& pos);
// a charge or torpedo met by a counter-shot at pos, fired from `from` (nullptr: unknown)
bool Intercept(const float3& pos, const float3* from);

float Scale();                                   // the budget's option value
Core::Fx::BudgetStats Stats();
uint32_t Intercepts();

// ---------------------------------------------------------------- abilities (GG.FxEvents); false: over the step's limit

bool Teleport(Core::Fx::TeleportKind kind, int32_t unit, float fromX, float fromZ, float toX, float toZ, float value);
bool Crash(int32_t unit);
bool Remnant(int32_t unit, const float3& pos);
bool Sacrifice(int32_t engineer, int32_t site, const float3& pos, float buildBefore, float buildAfter);

// ---------------------------------------------------------------- classified blasts (GG.CombatFx)

bool ForwardBlast(Core::Fx::BlastClass cls, Core::Fx::BlastFamily family, Core::Fx::Faction faction, const float3& pos,
	int32_t weaponDef = -1);
Core::Fx::BlastClass ClassOf(int32_t weaponDef);
Core::Fx::BlastFamily FamilyOf(int32_t weaponDef);

// ---------------------------------------------------------------- wrecks (GG.WreckPhysics, GG.WreckPose)

void WreckPlaced(int32_t feature);               // ECO: a wreck was made; it may slide at once
bool WreckMoving(int32_t feature);               // leave a posed wreck alone while this moves it
// a push in elmos per second; up and spin (radians per second) throw it into the air; false: no room for another
bool WreckPush(int32_t feature, float vx, float vz, float up, float spin);
void SetWreckPose(int32_t feature, float rx, float ry, float rz);   // it lies as its death left it
void ClearWreckPose(int32_t feature);
float WreckDamageScale();                        // the share of an explosion's damage a wreck takes

// ---------------------------------------------------------------- the missile relay (family SPECIAL's silos)

// once a step: every missile and interceptor record, and the redirections made since the last call
void ReportSilo(std::span<const Core::Fx::MissileIn> missiles, std::span<const Core::Fx::InterceptorIn> interceptors,
	std::span<const Core::Fx::RedirectIn> redirects);

} // namespace KT1::Fx

#endif
