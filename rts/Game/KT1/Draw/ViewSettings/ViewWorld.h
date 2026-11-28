/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ViewSettings: the parts that set engine settings: the rows of the view table (the pass gates), the effect cull
// scale, the unused deferred pass, the feature draw distance, water reflections and renderer, and the ground look.

#ifndef KT1_DRAW_VIEW_WORLD_H
#define KT1_DRAW_VIEW_WORLD_H

#include <vector>

#include "ViewNow.h"
#include "ViewShared.h"

namespace KT1::ViewSet {

class ViewWorld {
public:
	void Init(const ViewOptions& o);
	// once per switch-on: every row with a target, the feature distance (longest in elmos), the water texture check
	void Begin(float longestPropCut, int propCount);
	void Update(const ViewNow& now);
	void Release();

private:
	void WriteRow(int row);
	void ApplyFeatureDistance(float longest, int propCount);
	void CheckWaterTextures();
	void UpdateEffects();
	void UpdateDeferred();
	void UpdateReflection(const ViewNow& now);
	void SetReflection(float s);
	void UpdateWater();
	void RestoreWater();                     // the player's mode back after the far cut
	void UpdateLook(const ViewNow& now);
	void WriteLook(float far, float near);

	ViewOptions opt;
	float cegScale = -1.0f;
	bool deferredOff = false;
	bool haveWaterBase = false;
	float fresnelMin = 0, fresnelMax = 0;
	bool lookOn = true;
	bool haveLookBase = false;
	Core::View::GroundLook lookBase;
	std::vector<int> rows;
};

} // namespace KT1::ViewSet

#endif
