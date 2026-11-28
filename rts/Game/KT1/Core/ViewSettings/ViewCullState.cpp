/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: stubs of ViewCullState.h (LUA-FREE H01). FRAME step D07 writes the bodies.

#include "ViewCullState.h"

namespace KT1::Core::View {

ModelClass ClassOf(const ModelCullConfig&, const ModelDefFacts&) { return ModelClass::Small; }
float CullZoom(const ViewSettingsConfig&, ModelClass, int) { return 0; }
void MergeWreckZoom(WreckZoom&, float, bool) {}

void CullState::Init(int32_t) {}
void CullState::Clear() {}
void AddCullable(CullState&, int32_t, float, std::vector<MaskWrite>&) {}
void RemoveCullable(CullState&, int32_t) {}
void StepCull(CullState&, float, std::vector<MaskWrite>&) {}
void RestoreCull(CullState&, std::vector<MaskWrite>&) {}

int ShadowClamp(const ShadowCutConfig&, float) { return 0; }
float ShadowCutZoom(const ShadowCutConfig&, int, bool) { return 0; }
bool ShadowNext(bool, float, float, float) { return false; }
uint8_t ShadowMask(const ShadowCutConfig&, uint8_t mask, bool) { return mask; }
void StartSweep(IdSweep&, std::span<const int32_t>, bool) {}
bool StepSweep(IdSweep&, int, std::vector<int32_t>&) { return true; }

float PropCut(const PropCullConfig&, const PropFacts&, float) { return NO_END; }
int PropSize(const PropCullConfig&, const PropFacts&) { return 0; }
void PropState::Init(int32_t) {}
void AddProp(PropState&, const PropCullConfig&, int32_t, const PropFacts&, float, std::vector<MaskWrite>&) {}
void RemoveProp(PropState&, int32_t) {}
void PropDistancePass(PropState&, const PropCullConfig&, const float (&)[3], std::vector<MaskWrite>&) {}
void PropZoomPass(PropState&, const ViewSettingsConfig&, float, float, std::vector<MaskWrite>&) {}
void RestoreProps(PropState&, std::vector<MaskWrite>&) {}

} // namespace KT1::Core::View
