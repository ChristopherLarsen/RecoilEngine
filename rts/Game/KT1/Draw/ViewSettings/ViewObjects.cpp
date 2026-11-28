/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Model removal (CAM-003 v4 R3), wrecks (RND-006 R6a) and shadows by zoom (RND-002 v4 R8) on the draw masks.

#include "ViewObjects.h"

#include "Rendering/Env/Decals/SupComDecalCut.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

namespace KT1::ViewSet {

namespace View = Core::View;

namespace {

constexpr float NOT_LISTED = -1;

} // namespace

void ViewObjects::Init(const ViewOptions& o) {
	opt = o;
	const View::ViewSettingsConfig& c = Shared().config;
	defs = ViewDoor::UnitDefs();
	// the prop scale takes the detail option at load only: a changed option applies from the next game
	propScale = (opt.propScale >= 0 ? opt.propScale : c.props.lod_scale) * View::DetailFactor(c.detail, "plants", Shared().view.detail);
	longestCut = c.props.last_plant * propScale * c.props.elmos_per_map_unit;
}

void ViewObjects::BuildZooms() {
	View::ViewSettingsConfig c = Shared().config;
	c.model_cull.early = c.model_cull.early && opt.cullEarly;
	detail = Shared().view.detail;
	unitZoom.clear();
	wreckZoom.clear();
	for (const ViewDoor::UnitDefFacts& d : defs) {
		const View::ModelDefFacts facts = {d.name, d.xsize, d.zsize, d.commandIcon};
		const float zoom = View::CullZoom(c, View::ClassOf(c.model_cull, facts), detail);
		if (d.defId >= static_cast<int>(unitZoom.size())) unitZoom.resize(d.defId + 1, NOT_LISTED);
		unitZoom[d.defId] = zoom;
		// an experimental's wreck is never cut
		const bool never = d.tier >= c.model_cull.experimental_tier;
		const auto put = [this, zoom, never](const std::string& name) {
			const int id = name.empty() ? -1 : ViewDoor::FeatureDefId(name);
			if (id >= 0) View::MergeWreckZoom(wreckZoom[id], zoom, never);
		};
		if (!d.model.empty()) put("pmu_" + d.model);
		put(d.wreck);
	}
}

void ViewObjects::Begin(const ViewNow& now) {
	ViewShared& s = Shared();
	units.clear();
	features.clear();
	models.Init(ViewDoor::MaxUnits());
	wrecks.Init(ViewDoor::MaxFeatures());
	props.Init(ViewDoor::MaxFeatures());
	BuildZooms();
	s.shadow = View::ShadowState{};
	const View::ShadowCutConfig& sc = s.config.shadow_cut;
	s.shadow.far = !opt.frontend &&
		View::ShadowNext(false, now.dist / sc.elmos_per_map_unit, ShadowCut(), sc.hysteresis);
	ViewDoor::Listen();
}

void ViewObjects::WriteCull(View::CullState& state, bool feature, bool unitRule) {
	const View::ViewSettingsConfig& c = Shared().config;
	for (const View::MaskWrite& w : writes) {
		const Core::DenseIndex::Slot slot = state.index.SlotOf(w.id);
		if (slot == Core::DenseIndex::NONE) continue;
		uint8_t m = 0;
		if (!ViewDoor::Mask(feature, w.id, m)) continue;
		if (w.hide) {
			// a feature's stored 0 (hidden by someone else) is not what is given back
			state.savedMask[slot] = (m == 0 && !unitRule) ? static_cast<uint8_t>(c.props.full_mask) : m;
			ViewDoor::SetMask(feature, w.id, 0);
		} else {
			const uint8_t saved = state.savedMask[slot];
			ViewDoor::SetMask(feature, w.id, unitRule ? View::ShadowMask(c.shadow_cut, saved, Shared().shadow.far) : saved);
		}
	}
	writes.clear();
}

void ViewObjects::UnitArrived(int id) {
	const int def = ViewDoor::UnitDefOf(id);
	if (def < 0) return;
	units.insert(id);
	if (opt.cullModels && def < static_cast<int>(unitZoom.size()) && unitZoom[def] != NOT_LISTED) {
		View::AddCullable(models, id, unitZoom[def], writes);
		WriteCull(models, false, true);
	}
	// always set to the current state: a unit seen again near must get its shadow back
	if (!opt.frontend) ShadowOne(false, id);
}

void ViewObjects::FeatureArrived(int id, bool atStart) {
	ViewDoor::FeatureFacts f;
	if (!ViewDoor::Feature(id, f)) return;
	features.insert(id);
	if (atStart) startFeatures.insert(id);
	if (opt.cullProps && f.seen) AddProp(id, f, startFeatures.count(id) != 0);
	if (!opt.frontend) ShadowOne(true, id);
	if (!opt.wreckCull) return;
	const auto it = wreckZoom.find(f.defId);
	if (it == wreckZoom.end() || it->second.never) return;
	View::AddCullable(wrecks, id, it->second.zoom, writes);
	WriteCull(wrecks, true, false);
}

void ViewObjects::FeatureGone(int id) {
	features.erase(id);
	startFeatures.erase(id);
	View::RemoveCullable(wrecks, id);
	View::RemoveProp(props, id);
}

void ViewObjects::Apply(const ViewDoor::Change& c) {
	if (c.kind == ViewDoor::Change::Arrived) {
		if (c.feature) FeatureArrived(c.id, c.atStart);
		else UnitArrived(c.id);
		return;
	}
	if (c.feature) {
		FeatureGone(c.id);
		return;
	}
	units.erase(c.id);
	View::RemoveCullable(models, c.id);
}

float ViewObjects::ShadowCut() const {
	const View::ShadowCutConfig& c = Shared().config.shadow_cut;
	const int set = configHandler->IsSet(c.setting) ? configHandler->GetInt(c.setting) : c.setting_default;
	Shared().shadow.setting = View::ShadowClamp(c, static_cast<float>(set));
	return View::ShadowCutZoom(c, Shared().shadow.setting, opt.shadowCap);
}

void ViewObjects::ShadowOne(bool feature, int id) {
	uint8_t m = 0;
	if (ViewDoor::Mask(feature, id, m))
		ViewDoor::SetMask(feature, id, View::ShadowMask(Shared().config.shadow_cut, m, Shared().shadow.far));
}

// a crossing is spread over frames; a newer one replaces an unfinished sweep
void ViewObjects::StartShadowSweeps() {
	View::ShadowState& s = Shared().shadow;
	const std::vector<int32_t> unitIds(units.begin(), units.end()), featureIds(features.begin(), features.end());
	View::StartSweep(s.units, unitIds, s.far);
	View::StartSweep(s.features, featureIds, s.far);
}

void ViewObjects::StepShadowSweeps() {
	View::ShadowState& s = Shared().shadow;
	const int batch = Shared().config.shadow_cut.batch;
	swept.clear();
	View::StepSweep(s.units, batch, swept);
	for (const int32_t id : swept) ShadowOne(false, id);
	swept.clear();
	View::StepSweep(s.features, batch, swept);
	for (const int32_t id : swept) ShadowOne(true, id);
}

void ViewObjects::UpdateShadows(const ViewNow& now) {
	if (opt.frontend) return;
	View::ShadowState& s = Shared().shadow;
	const View::ShadowCutConfig& c = Shared().config.shadow_cut;
	StepShadowSweeps();
	const float zoom = now.dist / c.elmos_per_map_unit;
	if (opt.shadowCap) {
		const bool far = View::ShadowNext(s.decalsFar, zoom, c.decal_cap_elmos / c.elmos_per_map_unit, c.hysteresis);
		if (far != s.decalsFar) {
			s.decalsFar = far;
			SupComDecalCut::hidden = far;
		}
	}
	const float cut = ShadowCut();
	const bool far = View::ShadowNext(s.far, zoom, cut, c.hysteresis);
	if (far == s.far) return;
	s.far = far;
	StartShadowSweeps();
	if (opt.shadowLog) LOG("[SHADOWZOOM] %s at zoom %.0f (setting %d, cut %.1f)", far ? "off" : "on", zoom, s.setting, cut);
}

void ViewObjects::UpdateCulls(const ViewNow& now) {
	if (Shared().view.detail != detail) {        // the detail option changed: new removal zooms, the sets rebuilt
		View::RestoreCull(models, writes);
		WriteCull(models, false, true);
		View::RestoreCull(wrecks, writes);
		WriteCull(wrecks, true, false);
		models.Clear();
		wrecks.Clear();
		BuildZooms();
		const std::set<int32_t> unitsNow = units, featuresNow = features;
		for (const int32_t id : unitsNow) UnitArrived(id);
		for (const int32_t id : featuresNow) FeatureArrived(id, false);
	}
	// a camera mode with no zoom distance: nothing is judged by it, everything is shown
	const bool zoomed = now.zoomDist >= 0.0f;
	if (zoomed) View::StepCull(models, now.zoomDist, writes);
	else View::RestoreCull(models, writes);
	WriteCull(models, false, true);
	if (zoomed) View::StepCull(wrecks, now.zoomDist, writes);
	else View::RestoreCull(wrecks, writes);
	WriteCull(wrecks, true, false);
}

void ViewObjects::Update(const ViewNow& now) {
	ViewDoor::Take(changes);
	for (const ViewDoor::Change& c : changes) Apply(c);
	UpdateCulls(now);
	if (opt.cullProps) UpdateProps(now);
	UpdateShadows(now);
}

void ViewObjects::Release() {
	ViewDoor::Stop();
	View::ShadowState& s = Shared().shadow;
	s.far = false;                               // the masks given back below take their shadows with them
	View::RestoreCull(models, writes);
	WriteCull(models, false, true);
	View::RestoreCull(wrecks, writes);
	WriteCull(wrecks, true, false);
	View::RestoreProps(props, writes);
	WriteProps();
	for (const int32_t id : units) ShadowOne(false, id);
	for (const int32_t id : features) ShadowOne(true, id);
	s.units = {};
	s.features = {};
	if (s.decalsFar) SupComDecalCut::hidden = false;
	s.decalsFar = false;
}

} // namespace KT1::ViewSet
