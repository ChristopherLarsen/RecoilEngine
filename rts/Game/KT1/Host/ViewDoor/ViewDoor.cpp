/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#include "ViewDoor.h"

#include <algorithm>
#include <memory>

#include "Game/GlobalUnsynced.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Models/3DModel.hpp"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Features/FeatureHandler.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"
#include "System/StringUtil.h"

namespace KT1::ViewDoor {
namespace {

constexpr int CLIENT_ORDER = 1000;     // DESIGN: it only records, so its place in the event lists does not matter
constexpr int FIRST_STEP = 0;          // features there before the first step are the map's own

class Listener : public CEventClient {
public:
	Listener(): CEventClient("[KT1ViewDoor]", CLIENT_ORDER, false) {}

	bool WantsEvent(const std::string& e) override {
		return e == "UnitCreated" || e == "UnitGiven" || e == "UnitEnteredLos" || e == "UnitDestroyed" ||
		       e == "FeatureCreated" || e == "FeatureDestroyed";
	}
	bool GetFullRead() const override { return true; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }

	void UnitCreated(const CUnit* u, const CUnit*) override { Push(Change::Arrived, false, u->id); }
	void UnitGiven(const CUnit* u, int, int) override { Push(Change::Arrived, false, u->id); }
	void UnitEnteredLos(const CUnit* u, int) override { Push(Change::Arrived, false, u->id); }
	void UnitDestroyed(const CUnit* u, const CUnit*, int) override { Push(Change::Gone, false, u->id); }
	void FeatureCreated(const CFeature* f) override { Push(Change::Arrived, true, f->id); }
	void FeatureDestroyed(const CFeature* f) override { Push(Change::Gone, true, f->id); }

	void Push(Change::Kind kind, bool feature, int id) {
		const bool atStart = feature && gs->frameNum <= FIRST_STEP;
		queue.push_back({kind, feature, atStart, id});
	}

	std::vector<Change> queue;
};

std::unique_ptr<Listener> listener;

CSolidObject* Object(bool feature, int id) {
	if (id < 0) return nullptr;
	if (feature) return featureHandler.GetFeature(id);
	return unitHandler.GetUnit(id);
}

std::string Param(const spring::unordered_map<std::string, std::string>& p, const char* key) {
	const auto it = p.find(key);
	return it == p.end() ? std::string() : it->second;
}

// "dir/Name.s3o" -> "name"; empty for any other model
std::string ModelStem(const std::string& path) {
	const std::string ext = ".s3o";
	if (path.size() <= ext.size() || path.compare(path.size() - ext.size(), ext.size(), ext) != 0) return {};
	const size_t slash = path.find_last_of("/\\");
	const size_t from = slash == std::string::npos ? 0 : slash + 1;
	return StringToLower(path.substr(from, path.size() - ext.size() - from));
}

} // namespace

void Listen() {
	Stop();
	listener = std::make_unique<Listener>();
	eventHandler.AddClient(listener.get());
	for (const CUnit* u : unitHandler.GetActiveUnits()) listener->Push(Change::Arrived, false, u->id);
	for (const int id : FeatureIds()) listener->Push(Change::Arrived, true, id);
}

void Stop() {
	if (listener == nullptr) return;
	eventHandler.RemoveClient(listener.get());
	listener.reset();
}

void Take(std::vector<Change>& out) {
	out.clear();
	if (listener != nullptr) out.swap(listener->queue);
}

int MaxUnits() { return static_cast<int>(unitHandler.MaxUnits()); }
int MaxFeatures() { return MAX_FEATURES; }

std::vector<UnitDefFacts> UnitDefs() {
	std::vector<UnitDefFacts> out;
	for (const UnitDef& ud : unitDefHandler->GetUnitDefsVec()) {
		if (ud.id <= 0) continue;
		UnitDefFacts f;
		f.defId = ud.id;
		f.name = ud.name;
		f.commandIcon = ud.customParams.find("icon_cmd") != ud.customParams.end();
		f.xsize = ud.xsize;
		f.zsize = ud.zsize;
		const std::string tier = Param(ud.customParams, "eco_tier");
		if (!tier.empty()) f.tier = StringToInt(tier);
		f.model = ModelStem(ud.modelName);
		f.wreck = Param(ud.customParams, "pm_wreck");
		out.push_back(std::move(f));
	}
	return out;
}

int FeatureDefId(const std::string& name) {
	const FeatureDef* fd = featureDefHandler->GetFeatureDef(name, false);
	return fd == nullptr ? -1 : fd->id;
}

std::vector<int> FeatureIds() {
	std::vector<int> ids(featureHandler.GetActiveFeatureIDs().begin(), featureHandler.GetActiveFeatureIDs().end());
	std::sort(ids.begin(), ids.end());
	return ids;
}

int UnitDefOf(int id) {
	const CUnit* u = id < 0 ? nullptr : unitHandler.GetUnit(id);
	return u == nullptr ? -1 : u->unitDef->id;
}

bool Feature(int id, FeatureFacts& out) {
	const CFeature* f = id < 0 ? nullptr : featureHandler.GetFeature(id);
	if (f == nullptr) return false;
	out = FeatureFacts{};
	out.defId = f->def->id;
	out.x = f->pos.x;
	out.y = f->pos.y;
	out.z = f->pos.z;
	out.propClass = Param(f->def->customParams, "prop_class");
	out.wreck = f->def->customParams.find("pm_wreck") != f->def->customParams.end();
	const float3& box = f->collisionVolume.GetScales();
	if (box.x > 0.0f) out.sideElmos = std::max(box.x, box.z);
	else {
		const S3DModel* model = f->def->LoadModel();
		out.sideElmos = (model == nullptr ? 0.0f : model->radius) * 2.0f;   // DESIGN: radius to side
	}
	out.seen = gu->spectatingFullView || f->IsInLosForAllyTeam(gu->myAllyTeam);
	return true;
}

bool Mask(bool feature, int id, uint8_t& out) {
	const CSolidObject* o = Object(feature, id);
	if (o == nullptr) return false;
	out = o->engineDrawMask;
	return true;
}

void SetMask(bool feature, int id, uint8_t mask) {
	CSolidObject* o = Object(feature, id);
	if (o == nullptr || o->engineDrawMask == mask) return;
	const uint8_t old = o->engineDrawMask;
	o->engineDrawMask = mask;
	if (!feature || (old == 0) == (mask == 0)) return;
	const CFeature* f = static_cast<const CFeature*>(o);
	if (f->def->drawType == DRAWTYPE_MODEL) CFeatureDrawer::SupComSetMasked(f, mask == 0);
}

} // namespace KT1::ViewDoor
