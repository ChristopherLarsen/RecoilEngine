/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: the draw-side writes (see DrawWrites.h). Each does what the named LuaUnsyncedCtrl / LuaShaders call does.

#include "DrawWrites.h"

#include <algorithm>

#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/IconHandler.h"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1::DrawWrite {

namespace {

constexpr uint32_t LUA_ICON_ATLAS = 1;   // DESIGN: the atlas Spring.AddUnitIcon adds to (0 is the game's icontypes)

CUnit* Unit(int id) { return (id < 0) ? nullptr : unitHandler.GetUnit(id); }
CFeature* Feature(int id) { return (id < 0) ? nullptr : featureHandler.GetFeature(id); }

int WriteUniforms(const CWorldObject* o, int offset, std::span<const float> values) {
	const int first = std::max(offset, 0);
	const int count = std::min(static_cast<int>(values.size()), ModelUniformData::MAX_MODEL_UD_UNIFORMS - first);
	if (count < 1) return 0;
	ModelUniformData& uni = modelUniformsStorage.GetObjUniformsArray(o);
	std::copy_n(values.begin(), count, std::begin(uni.userDefined) + first);
	return count;
}

} // namespace

bool SetUnitNoDraw(int unitId, bool noDraw) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	u->noDraw = noDraw;
	return true;
}

bool SetUnitDrawMask(int unitId, uint8_t mask) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	u->engineDrawMask = mask;
	return true;
}

bool SetUnitAlwaysUpdateMatrix(int unitId, bool on) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	u->alwaysUpdateMat = on;
	return true;
}

bool SetUnitNoMinimap(int unitId, bool noMinimap) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	u->noMinimap = noMinimap;
	return true;
}

bool SetUnitIconDraw(int unitId, bool draw) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	u->drawIcon = draw;
	return true;
}

int SetUnitUniforms(int unitId, int offset, std::span<const float> values) {
	const CUnit* u = Unit(unitId);
	return (u == nullptr) ? 0 : WriteUniforms(u, offset, values);
}

bool SetFeatureNoDraw(int featureId, bool noDraw) {
	CFeature* f = Feature(featureId);
	if (f == nullptr) return false;
	f->noDraw = noDraw;
	return true;
}

bool SetFeatureDrawMask(int featureId, uint8_t mask) {
	CFeature* f = Feature(featureId);
	if (f == nullptr) return false;
	const uint8_t old = f->engineDrawMask;
	f->engineDrawMask = mask;
	if ((old == 0) != (mask == 0) && f->def->drawType == DRAWTYPE_MODEL) CFeatureDrawer::SupComSetMasked(f, mask == 0);
	return true;
}

bool SetFeatureAlwaysUpdateMatrix(int featureId, bool on) {
	CFeature* f = Feature(featureId);
	if (f == nullptr) return false;
	f->alwaysUpdateMat = on;
	return true;
}

bool SetFeatureFade(int featureId, bool allow) {
	CFeature* f = Feature(featureId);
	if (f == nullptr) return false;
	f->alphaFade = allow;
	return true;
}

int SetFeatureUniforms(int featureId, int offset, std::span<const float> values) {
	const CFeature* f = Feature(featureId);
	return (f == nullptr) ? 0 : WriteUniforms(f, offset, values);
}

bool IconTypeExists(const std::string& name) { return icon::iconHandler.FindIconIdx(name).first; }

bool AddIconType(const std::string& name, const std::string& texture, float size, float distance, bool radiusAdjust) {
	// the whole texture, draw order 0: Spring.AddUnitIcon's defaults
	return icon::iconHandler.AddIcon(LUA_ICON_ATLAS, name, texture, size, distance, radiusAdjust, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
}

bool SetUnitDefIcon(int defId, const std::string& iconType) {
	const UnitDef* ud = unitDefHandler->GetUnitDefByID(defId);
	if (ud == nullptr || !IconTypeExists(iconType)) return false;

	ud->iconName = iconType;
	if (ud->decoyDef != nullptr) ud->decoyDef->iconName = iconType;

	const auto& decoys = unitDefHandler->GetDecoyDefIDs();
	const auto it = decoys.find((ud->decoyDef != nullptr) ? ud->decoyDef->id : ud->id);
	if (it != decoys.end()) {
		for (const int decoyId : it->second) unitDefHandler->GetUnitDefByID(decoyId)->iconName = iconType;
	}
	unitDrawer->UpdateUnitIconsByUnitDef(ud);
	return true;
}

bool SetUnitIcon(int unitId, const std::string& iconType) {
	CUnit* u = Unit(unitId);
	if (u == nullptr) return false;
	if (iconType.empty()) {
		u->customIconIndex = icon::INVALID_ICON_INDEX;
	} else {
		if (!IconTypeExists(iconType)) return false;
		u->customIconIndex = icon::iconHandler.GetIconIdx(iconType);
	}
	CUnitDrawer::UpdateCurrentUnitIcon(u);
	return true;
}

} // namespace KT1::DrawWrite
