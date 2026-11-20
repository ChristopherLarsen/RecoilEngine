/* SupCom: the feature drawer per-frame update skips features that will not be drawn (render-only, 2026-11-20).
 * CFeatureDrawerData::Update walked every feature every drawn frame (draw position, view tests for up to three cameras,
 * transform upload, uniforms): with 32,000 props on Sunken it cost frame time even with the props hidden.
 * Skipped: a feature whose engine draw mask is 0 (Lua hid it: every draw path checks the mask, so its stale flags are never
 * used), and a fading feature already flagged not drawn that is beyond the feature draw distance by more than its radius
 * (the stock update would flag it not drawn again). Both run the stock update again on the frame they come back. */
#ifndef SUPCOM_FEATURE_DRAW_SKIP_H
#define SUPCOM_FEATURE_DRAW_SKIP_H

#include "Sim/Features/Feature.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"

inline bool SupComFeatureDrawSkip(const CFeature* f, float drawDist)
{
	if (f->engineDrawMask == 0)
		return true;
	if (f->drawFlag != 0 || !f->alphaFade)
		return false;
	const float r = f->GetDrawRadius();
	const float lim = drawDist + r;
	return (f->pos - CCameraHandler::GetCamera(CCamera::CAMTYPE_PLAYER)->GetPos()).SqLength() > lim * lim;
}

#endif
