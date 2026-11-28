/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 host: DrawView's reads of unit and feature defs and of features (see DrawView.h, DrawReads.h).

#include "DrawView.h"

#include <algorithm>

#include "ViewerRules.h"

#include "Rendering/Models/3DModel.hpp"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/CollisionVolume.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/UnitDefHandler.h"

namespace KT1 {

namespace {

const CFeature* LiveFeature(int id) { return (id < 0) ? nullptr : featureHandler.GetFeature(id); }

const CFeature* SeenFeature(int id) {
	const CFeature* f = LiveFeature(id);
	return (f != nullptr && ViewerRules::Seen(*f)) ? f : nullptr;
}

bool FindParam(const SolidObjectDef& def, const std::string& key, std::string_view& out) {
	const auto it = def.customParams.find(key);
	if (it == def.customParams.end()) return false;
	out = it->second;
	return true;
}

} // namespace

int DrawView::UnitDefCount() const { return static_cast<int>(unitDefHandler->NumUnitDefs()); }

bool DrawView::UnitDefInfo(int defId, UnitDefView& out) const {
	const UnitDef* def = unitDefHandler->GetUnitDefByID(defId);
	if (def == nullptr) return false;
	out.id = defId;
	out.name = def->name;
	out.modelName = def->modelName;
	out.iconType = def->iconName;
	out.xsize = def->xsize;
	out.zsize = def->zsize;
	out.canMove = def->canmove;
	return true;
}

bool DrawView::UnitDefParam(int defId, const std::string& key, std::string_view& out) const {
	const UnitDef* def = unitDefHandler->GetUnitDefByID(defId);
	return def != nullptr && FindParam(*def, key, out);
}

float DrawView::UnitDefModelHeight(int defId) const {
	const UnitDef* def = unitDefHandler->GetUnitDefByID(defId);
	if (def == nullptr) return 0.0f;
	const S3DModel* model = def->LoadModel();
	return (model != nullptr) ? model->height : 0.0f;
}

int DrawView::FeatureDefCount() const { return static_cast<int>(featureDefHandler->NumFeatureDefs()); }

bool DrawView::FeatureDefInfo(int defId, FeatureDefView& out) const {
	const FeatureDef* def = featureDefHandler->GetFeatureDefByID(defId);
	if (def == nullptr) return false;
	out.id = defId;
	out.name = def->name;
	out.modelName = def->modelName;
	out.xsize = def->xsize;
	out.zsize = def->zsize;
	return true;
}

bool DrawView::FeatureDefParam(int defId, const std::string& key, std::string_view& out) const {
	const FeatureDef* def = featureDefHandler->GetFeatureDefByID(defId);
	return def != nullptr && FindParam(*def, key, out);
}

// FeatureDefs[id].radius (LuaUtils::PushModelRadius): a tree drawn by the engine has the fixed tree radius
float DrawView::FeatureDefModelRadius(int defId) const {
	const FeatureDef* def = featureDefHandler->GetFeatureDefByID(defId);
	if (def == nullptr || def->drawType == DRAWTYPE_NONE) return 0.0f;
	if (def->drawType != DRAWTYPE_MODEL) return TREE_RADIUS;
	const S3DModel* model = def->LoadModel();
	return (model != nullptr) ? model->radius : 0.0f;
}

int DrawView::FeatureCapacity() const { return MAX_FEATURES; }

bool DrawView::FeatureAlive(int id) const { return SeenFeature(id) != nullptr; }

void DrawView::FeatureIds(std::vector<int>& out) const {
	out.clear();
	for (const int id : featureHandler.GetActiveFeatureIDs()) {
		if (SeenFeature(id) != nullptr) out.push_back(id);
	}
	std::sort(out.begin(), out.end());
}

bool DrawView::Feature(int id, FeatureView& out) const {   // KT1 hot
	const CFeature* f = SeenFeature(id);
	if (f == nullptr) return false;
	out.id = id;
	out.defId = f->def->id;
	out.team = (f->allyteam < 0) ? -1 : f->team;
	out.allyTeam = f->allyteam;
	out.pos = f->pos;
	out.midPos = f->midPos;
	out.radius = f->radius;
	out.colVolScales = f->collisionVolume.GetScales();
	return true;
}

bool DrawView::FeatureDrawState(int id, ObjectDrawState& out) const {
	const CFeature* f = LiveFeature(id);
	if (f == nullptr) return false;
	out.drawMask = f->engineDrawMask;
	out.drawFlag = f->drawFlag;
	out.noDraw = f->noDraw;
	out.inVoid = f->IsInVoid();
	out.icon = false;
	return true;
}

} // namespace KT1
