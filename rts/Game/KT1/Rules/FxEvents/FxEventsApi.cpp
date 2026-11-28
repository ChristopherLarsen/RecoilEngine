/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxEvents: the API of the synced effect system. Stub (LUA-FREE H01): step S30 writes the bodies.

#include "Game/KT1/Rules/FxEvents/FxEventsApi.h"

#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "System/float3.h"

namespace KT1::Fx {

bool Active() { return false; }

int32_t EffectId(std::string_view) { return Core::Fx::NO_EFFECT; }
bool Name(std::string_view, std::string_view, std::string_view, std::string_view, char* out, size_t size) {
	if (out != nullptr && size > 0) out[0] = 0;
	return false;
}
bool Spawn(int32_t, const float3&, const float3&, Core::Fx::Priority, float) { return false; }
bool Spawn(std::string_view, const float3&, const float3&, Core::Fx::Priority, float) { return false; }
bool Move(Core::Fx::MoveKind, Core::Fx::SizeBucket, const float3&) { return false; }
bool Intercept(const float3&, const float3*) { return false; }

float Scale() { return 0.0f; }
Core::Fx::BudgetStats Stats() { return {}; }
uint32_t Intercepts() { return 0; }

bool Teleport(Core::Fx::TeleportKind, int32_t, float, float, float, float, float) { return false; }
bool Crash(int32_t) { return false; }
bool Remnant(int32_t, const float3&) { return false; }
bool Sacrifice(int32_t, int32_t, const float3&, float, float) { return false; }

bool ForwardBlast(Core::Fx::BlastClass, Core::Fx::BlastFamily, Core::Fx::Faction, const float3&, int32_t) { return false; }
Core::Fx::BlastClass ClassOf(int32_t) { return Core::Fx::BlastClass::None; }
Core::Fx::BlastFamily FamilyOf(int32_t) { return Core::Fx::BlastFamily::None; }

void WreckPlaced(int32_t) {}
bool WreckMoving(int32_t) { return false; }
bool WreckPush(int32_t, float, float, float, float) { return false; }
void SetWreckPose(int32_t, float, float, float) {}
void ClearWreckPose(int32_t) {}
float WreckDamageScale() { return 0.0f; }

void ReportSilo(std::span<const Core::Fx::MissileIn>, std::span<const Core::Fx::InterceptorIn>,
	std::span<const Core::Fx::RedirectIn>) {}

} // namespace KT1::Fx
