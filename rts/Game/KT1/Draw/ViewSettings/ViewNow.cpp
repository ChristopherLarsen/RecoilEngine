/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#include "ViewNow.h"

#include <algorithm>
#include <cstdlib>

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Camera/CameraController.h"
#include "Game/GameSetup.h"
#include "Map/Ground.h"
#include "Rendering/GlobalRendering.h"

namespace KT1::ViewSet {
namespace {

bool On(const char* name) { return ModOption(name) != "0"; }   // an unset option is on

} // namespace

std::string ModOption(const char* name) {
	const auto& opts = CGameSetup::GetModOptions();
	const auto it = opts.find(name);
	return it == opts.end() ? std::string() : std::string(it->second.c_str());
}

ViewOptions ReadOptions() {
	ViewOptions o;
	o.frontend = ModOption("frontend") == "1";
	o.cullModels = On("cull_models");
	o.cullEarly = On("model_cull_early");
	o.cullProps = On("cull_props");
	o.wreckCull = On("wreck_cull");
	o.zoomSwitch = On("zoomswitch");
	o.shadowCap = On("shadow_zoom_cap");
	o.shadowLog = ModOption("shadow_log") == "1";
	o.gates = On("draw_zoom_gates");
	o.reflGate = On("dzg_refl");
	o.deferred = On("dzg_deferred");
	o.waterFar = ModOption("water_far") == "1";
	o.waterGuard = On("water_guard");
	const std::string shadowMax = ModOption("dzg_shadow_max");
	if (!shadowMax.empty()) o.shadowMaxOverride = std::atoi(shadowMax.c_str());
	const std::string scale = ModOption("prop_lod_scale");
	if (!scale.empty()) o.propScale = std::strtof(scale.c_str(), nullptr);
	return o;
}

ViewNow ReadCamera() {
	ViewNow n;
	CCameraController& con = camHandler->GetCurrentController();
	n.pos = camera->GetPos();
	n.zoomDist = con.GetSupComDist();
	n.fov = con.GetFOV();
	n.viewH = globalRendering->viewSizeY;
	const float aboveGround = n.pos.y - std::max(CGround::GetHeightReal(n.pos.x, n.pos.z, false), 0.0f);
	n.dist = n.zoomDist >= 0.0f ? n.zoomDist : aboveGround;
	n.lookHeight = n.zoomDist >= 0.0f ? n.zoomDist : n.pos.y;
	if (n.zoomDist < 0.0f) {
		// only the cameras without a zoom distance keep a "height" in their state
		CCameraController::StateMap state;
		con.GetState(state);
		const auto it = state.find("height");
		if (it != state.end()) n.lookHeight = it->second;
	}
	return n;
}

} // namespace KT1::ViewSet
