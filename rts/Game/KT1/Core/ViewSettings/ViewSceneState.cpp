/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of ViewSceneState.h (LUA-FREE H01). FRAME step D07 writes the bodies.

#include "ViewSceneState.h"

namespace KT1::Core::View {

float ZoomSwitchLevel(const ZoomSwitchConfig&, float) { return 0; }
float TankPixels(float, int, float, float) { return 0; }
float DistanceForPixels(float, int, float, float) { return 0; }
float ReflectionStrength(const ZoomSwitchConfig&, float, float, int, float) { return 1; }
float PropFadeEnd(const ViewSettingsConfig&, float, float) { return 0; }
float PropFadeAlpha(const ViewSettingsConfig&, float, float, float) { return 1; }

float LookFar(const GroundLookConfig&, float) { return 0; }
float LookNear(const GroundLookConfig&, float) { return 0; }
bool LookMoved(const SceneState&, const GroundLookConfig&, float, float) { return false; }
GroundLook ApplyLook(const GroundLookConfig&, const GroundLook& base, float, float) { return base; }

int FarWaterMode(SceneState&, const WaterConfig&, int, int) { return NO_WATER_MODE; }
int GuardWaterMode(const WaterConfig&, int) { return NO_WATER_MODE; }

float MinimapGapSeconds(const MinimapRateConfig&, int) { return 0; }
bool MinimapLayersOff(const MinimapRateConfig&, int, bool) { return false; }

} // namespace KT1::Core::View
