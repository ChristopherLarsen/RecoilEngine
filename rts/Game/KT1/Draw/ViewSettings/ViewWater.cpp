/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Water by zoom (RND-004 reflections, the far cut, the mode and texture guards) and the ground look by camera height.

#include "ViewWorld.h"

#include <string>
#include <utility>

#include "Map/BaseGroundDrawer.h"
#include "Map/ReadMap.h"
#include "Rendering/Env/IWater.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "System/Config/ConfigHandler.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Log/ILog.h"

namespace KT1::ViewSet {

namespace View = Core::View;

namespace {

int WaterMode() { return static_cast<int>(IWater::GetWater()->GetID()); }

// a changed water parameter takes effect by building the renderer again, as Spring.SetWaterParams did
void ReloadWater() {
	const int mode = WaterMode();
	IWater::KillWater();
	IWater::SetWater(mode);
	waterRendering->SetUpdated();
}

bool Exists(const std::string& path) { return CFileHandler::FileExists(path, SPRING_VFS_ALL); }

} // namespace

// a missing water texture is logged and swapped for the engine's base one, so the water never turns a flat colour
void ViewWorld::CheckWaterTextures() {
	if (!opt.waterGuard) return;
	const View::WaterConfig& w = Shared().config.water;
	const std::pair<std::string*, const std::string*> textures[] = {
		{&waterRendering->normalTexture, &w.base_normal},
		{&waterRendering->foamTexture, &w.base_foam},
		{&waterRendering->texture, &w.base_ocean},
	};
	int missing = 0, swapped = 0;
	for (const auto& [name, base] : textures) {
		if (!Exists(*base)) {
			LOG("[WATER] missing %s", base->c_str());
			++missing;
		}
		if (name->empty() || Exists(*name) || Exists("maps/" + *name)) continue;
		LOG("[WATER] missing %s", name->c_str());
		*name = *base;
		++missing;
		++swapped;
	}
	if (missing == 0) LOG("[WATER] ok: water textures found");
	if (swapped > 0) {
		ReloadWater();
		LOG("[WATER] base textures used for the missing ones");
	}
}

void ViewWorld::SetReflection(float s) {
	// the fresnel terms carry the fade; reflectionStrength lets the engine skip the pass at 0
	if (haveWaterBase) {
		waterRendering->fresnelMin = fresnelMin * s;
		waterRendering->fresnelMax = fresnelMax * s;
	}
	waterRendering->reflectionStrength = s;
	ReloadWater();
}

void ViewWorld::UpdateReflection(const ViewNow& now) {
	if (!haveWaterBase) {
		haveWaterBase = true;
		fresnelMin = waterRendering->fresnelMin;
		fresnelMax = waterRendering->fresnelMax;
	}
	const View::ZoomSwitchConfig& z = Shared().config.zoom_switch;
	const float value = configHandler->IsSet(z.setting) ? static_cast<float>(configHandler->GetInt(z.setting)) : z.default_level;
	const float s = View::ReflectionStrength(z, now.dist, View::ZoomSwitchLevel(z, value), now.viewH, now.fov);
	if (s == Shared().scene.reflection) return;
	Shared().scene.reflection = s;
	SetReflection(s);
}

void ViewWorld::UpdateWater() {
	const View::WaterConfig& w = Shared().config.water;
	// dynamic water hangs the engine when the camera is far out: checked every frame, whoever chose it
	const int safe = opt.waterGuard ? View::GuardWaterMode(w, WaterMode()) : View::NO_WATER_MODE;
	if (safe != View::NO_WATER_MODE) {
		IWater::SetWater(safe);
		if (configHandler->IsSet(w.safe_setting) && configHandler->GetString(w.safe_setting) == std::to_string(w.dynamic_mode))
			configHandler->SetString(w.safe_setting, std::to_string(safe));
		LOG("[WATER] mode %d switched to %d (dynamic water can hang the engine)", w.dynamic_mode, safe);
	}
	if (opt.frontend || !(w.far_cut || opt.waterFar)) return;
	const int level = Shared().view.level;
	const int mode = View::FarWaterMode(Shared().scene, w, level, WaterMode());
	if (mode == View::NO_WATER_MODE) return;
	LOG("[WATER] far cut: mode %d to %d at L%d", WaterMode(), mode, level);
	IWater::SetWater(mode);
}

void ViewWorld::RestoreWater() {
	View::SceneState& scene = Shared().scene;
	if (scene.wantedWater != View::NO_WATER_MODE && WaterMode() == Shared().config.water.basic_mode)
		IWater::SetWater(scene.wantedWater);
	scene.wantedWater = View::NO_WATER_MODE;
	scene.waterFar = false;
}

void ViewWorld::WriteLook(float far, float near) {
	const View::GroundLook l = View::ApplyLook(Shared().config.ground_look, lookBase, far, near);
	mapRendering->splatTexMults = float4(l.splat[0], l.splat[1], l.splat[2], l.splat[3]);
	readMap->GetGroundDrawer()->UpdateRenderState();
	CSunLighting sun = *sunLighting;
	sun.SetValue("groundAmbientColor", float4(l.ambient[0], l.ambient[1], l.ambient[2], sun.groundAmbientColor.w));
	sun.SetValue("groundDiffuseColor", float4(l.diffuse[0], l.diffuse[1], l.diffuse[2], sun.groundDiffuseColor.w));
	*sunLighting = sun;
	sunLighting->SetUpdated();
}

void ViewWorld::UpdateLook(const ViewNow& now) {
	const View::GroundLookConfig& c = Shared().config.ground_look;
	if (!haveLookBase) {
		haveLookBase = true;
		for (int i = 0; i < 4; ++i) lookBase.splat[i] = mapRendering->splatTexMults[i];   // DESIGN: four detail layers, three colour channels
		for (int i = 0; i < 3; ++i) {   // DESIGN: four detail layers, three colour channels
			lookBase.ambient[i] = sunLighting->groundAmbientColor[i];
			lookBase.diffuse[i] = sunLighting->groundDiffuseColor[i];
		}
	}
	View::SceneState& scene = Shared().scene;
	const float far = View::LookFar(c, now.lookHeight), near = View::LookNear(c, now.lookHeight);
	if (!View::LookMoved(scene, c, far, near)) return;
	scene.lookFar = far;
	scene.lookNear = near;
	WriteLook(far, near);
}

} // namespace KT1::ViewSet
