/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// The view table's engine settings (DRAW-ZOOM-MAP) and the settings that follow them (RND-006 R8).

#include "ViewWorld.h"

#include <algorithm>
#include <cmath>

#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Units/UnitDrawer.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

namespace KT1::ViewSet {

namespace View = Core::View;

void ViewWorld::Init(const ViewOptions& o) {
	opt = o;
	const std::string& setting = Shared().config.ground_look.setting;
	lookOn = !configHandler->IsSet(setting) || configHandler->GetInt(setting) != 0;
}

void ViewWorld::Begin(float longestPropCut, int propCount) {
	ViewShared& s = Shared();
	cegScale = -1.0f;
	s.scene = View::SceneState{};
	s.scene.lookFar = -1.0f;
	for (int i = 0; i < s.view.table.count; ++i)
		if (!s.config.rows[i].target.empty()) WriteRow(i);
	if (opt.cullProps) ApplyFeatureDistance(longestPropCut, propCount);
	if (!opt.frontend) CheckWaterTextures();
}

// an engine setting row, written as an overlay so the player's saved settings are not changed
void ViewWorld::WriteRow(int i) {
	const View::ViewRow& row = Shared().config.rows[i];
	if (row.action != "set_engine_setting") return;
	const bool holds = (Shared().view.mask & (uint64_t(1) << i)) != 0;
	if (!holds && !row.write_other) return;
	float value = holds ? row.value : row.other_value;
	// the owner-decision captures' overrides of the shadow and reflection gates (mod options)
	if (row.target == "SupComShadowMaxLevel" && opt.gates && opt.shadowMaxOverride >= 0) value = static_cast<float>(opt.shadowMaxOverride);
	if (row.target == "SupComReflTexMaxLevel" && !opt.reflGate) value = static_cast<float>(Shared().config.draw_passes.always_level);
	if (value == std::floor(value)) configHandler->Set(row.target, static_cast<int>(value), true);
	else configHandler->Set(row.target, value, true);
	LOG("[DRAWZOOM] %s=%g", row.target.c_str(), value);
}

// the engine's one feature distance is raised to the longest last distance in use, when the preset's differs
void ViewWorld::ApplyFeatureDistance(float longest, int propCount) {
	const int d = std::max(1, static_cast<int>(longest));
	// DESIGN: an unset key reads as 0, so it is always written
	const int cur = configHandler->IsSet("FeatureDrawDistance") ? static_cast<int>(configHandler->GetFloat("FeatureDrawDistance")) : 0;
	if (cur != d) {
		configHandler->Set("FeatureDrawDistance", d, true);
		configHandler->Set("FeatureFadeDistance", static_cast<int>(d * Shared().config.props.fade_share), true);
	}
	LOG("[PROPDIST] engine feature distance %d elmos (preset %d); %d props", d, cur, propCount);
}

// the level-of-detail option scales every effect cut-off
void ViewWorld::UpdateEffects() {
	const float f = opt.gates ? View::DetailFactor(Shared().config.detail, "effects", Shared().view.detail) : 1.0f;
	if (f == cegScale) return;
	cegScale = f;
	configHandler->Set("KardashevCegCullScale", f, true);
}

// the deferred model pass draws every model a second time into a G-buffer; with the engine fog mask on nothing reads it
void ViewWorld::UpdateDeferred() {
	const bool want = opt.gates && opt.deferred && Shared().engineFog;
	if (want == deferredOff) return;
	deferredOff = want;
	CUnitDrawer::SetDrawDeferredPass(!want);
	CFeatureDrawer::SetDrawDeferredPass(!want);
	LOG("[DRAWZOOM] deferred model pass %s", want ? "off (engine fog mask on: G-buffer unused)" : "on");
}

void ViewWorld::Update(const ViewNow& now) {
	rows.clear();
	View::RowsToWrite(Shared().view, Shared().config, rows);
	for (const int i : rows) WriteRow(i);
	UpdateEffects();
	UpdateDeferred();
	if (opt.zoomSwitch) UpdateReflection(now);
	UpdateWater();
	if (lookOn) UpdateLook(now);
}

void ViewWorld::Release() {
	if (deferredOff) {
		CUnitDrawer::SetDrawDeferredPass(true);
		CFeatureDrawer::SetDrawDeferredPass(true);
		deferredOff = false;
	}
	View::SceneState& scene = Shared().scene;
	if (scene.reflection != 1.0f) {
		scene.reflection = 1.0f;
		SetReflection(1.0f);
	}
	RestoreWater();
	if (haveLookBase) WriteLook(0.0f, 0.0f);
	scene.lookFar = -1.0f;
}

} // namespace KT1::ViewSet
