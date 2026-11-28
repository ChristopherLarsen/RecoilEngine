/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 engine-free
// KT1 FxFields: the strategic missile look rules. Stub (LUA-FREE H01): step D14 writes the bodies.

#include "Game/KT1/Draw/FxFields/Strategic.h"

namespace KT1::FxFields {

bool Allowed(StrategicEffect, const EffectSwitches&) { return false; }
Viewer ViewerOf(int32_t, int32_t, bool) { return Viewer::Enemy; }
Warning LaunchWarning(const StrategicLookConfig&, const Core::Fx::StrategicLaunch&, Viewer, const char*, float) { return {}; }
Warning InterceptWarning(const StrategicLookConfig&, const Core::Fx::StrategicLaunch&, Viewer, bool, bool, float, float,
	float) { return {}; }
bool MergePing(const StrategicLookConfig&, std::vector<RecentPing>&, float, float, int32_t, float) { return false; }
FlightPlan PlanFlight(const StrategicLookConfig&, const Core::Fx::StrategicLaunch&, float) { return {}; }
FlightPoint MissileAt(const StrategicLookConfig&, const Core::Fx::StrategicLaunch&, const FlightPlan&, float,
	const GroundAt&) { return {}; }
int32_t TrailStage(const StrategicLookConfig&, const FlightPlan&, float) { return 0; }
void SimMissileAt(const Core::Fx::StrategicLaunch&, float, float, float& x, float& z) { x = 0.0f; z = 0.0f; }
Core::Fx::Vec3 InterceptorDrawn(const StrategicLookConfig&, const Core::Fx::StrategicLaunch&, const FlightPlan&, float,
	float, float, float, float, float, const GroundAt&) { return {}; }
FlightPoint InterceptorAt(const FlightPlan&, const Core::Fx::Vec3&, const Core::Fx::Vec3&, float) { return {}; }
ImpactLook ImpactAt(const StrategicLookConfig&, float, float) { return {}; }
ImpactLook ShellImpactAt(const StrategicLookConfig&, float) { return {}; }
float ShakeAt(float, float, float, float, float) { return 0.0f; }
bool IsStrategicShell(const StrategicLookConfig&, const ShellInfo&) { return false; }
bool Landing(const StrategicLookConfig&, const Core::Fx::Vec3&, const Core::Fx::Vec3&, float, float, const GroundAt&,
	Core::Fx::Vec3&, int32_t&) { return false; }

} // namespace KT1::FxFields
