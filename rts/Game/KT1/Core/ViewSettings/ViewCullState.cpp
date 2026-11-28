/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: model and wreck removal (model_cull.lua, gfx_wreck_cull) and the shadow cut (shadow_zoom.lua). LUA-FREE D07.

#include "ViewCullState.h"

#include <algorithm>
#include <cmath>

namespace KT1::Core::View {

ModelClass ClassOf(const ModelCullConfig& config, const ModelDefFacts& def) {
	if (def.commandIcon || def.name.find(config.command_name_part) != std::string_view::npos) return ModelClass::Lead;
	return std::min(def.xsize, def.zsize) >= config.large_squares ? ModelClass::Large : ModelClass::Small;
}

float CullZoom(const ViewSettingsConfig& config, ModelClass cls, int detail) {
	const ModelCullConfig& m = config.model_cull;
	// the pack gives a range for the small and the large class: the class takes the middle
	float zoom = (m.small_range[0] + m.small_range[1]) / 2;   // DESIGN: the middle of the range, rounding to the step
	const char* sweep = "models_small";
	if (cls == ModelClass::Lead) {
		zoom = m.lead_zoom;
		sweep = "models_lead";
	} else if (cls == ModelClass::Large) {
		zoom = (m.large_range[0] + m.large_range[1]) / 2;   // DESIGN: the middle of the range, rounding to the step
		sweep = "models";
	}
	zoom = zoom * m.elmos_per_map_unit * DetailFactor(config.detail, sweep, detail);
	return m.early ? std::min(m.early_zoom, zoom) : zoom;
}

void MergeWreckZoom(WreckZoom& wreck, float zoom, bool never) {
	if (never || wreck.never) {
		wreck.never = true;
	} else if (!wreck.known || zoom > wreck.zoom) {
		wreck.zoom = zoom;
	}
	wreck.known = true;
}

void CullState::Init(int32_t maxIds) {
	index.Init(maxIds);
	set.Init(index);
	hidden.Init(index);
	savedMask.Init(index);
	zoom.clear();
	culled.clear();
	members.clear();
}

void CullState::Clear() { Init(index.Capacity()); }

void AddCullable(CullState& state, int32_t id, float zoom, std::vector<MaskWrite>& out) {
	if (!state.index.InRange(id) || state.index.Has(id)) return;
	const auto found = std::find(state.zoom.begin(), state.zoom.end(), zoom);
	const size_t at = static_cast<size_t>(found - state.zoom.begin());
	if (found == state.zoom.end()) {
		state.zoom.push_back(zoom);
		state.culled.push_back(0);
		state.members.emplace_back();
	}
	const DenseIndex::Slot slot = state.index.Add(id);
	state.set[slot] = static_cast<int32_t>(at);
	state.members[at].push_back(id);
	if (state.culled[at] != 0) {                       // a new object takes its set's state on arrival
		state.hidden[slot] = 1;
		out.push_back({id, true});
	}
}

// the object is gone: nothing is written to it
void RemoveCullable(CullState& state, int32_t id) {
	if (!state.index.Has(id)) return;
	std::erase(state.members[state.set[state.index.SlotOf(id)]], id);
	const DenseIndex::Move move = state.index.Remove(id);
	state.set.Apply(move);
	state.hidden.Apply(move);
	state.savedMask.Apply(move);
}

namespace {

void SetSet(CullState& state, size_t at, bool cull, std::vector<MaskWrite>& out) {
	state.culled[at] = cull ? 1 : 0;
	for (const int32_t id : state.members[at]) {
		const DenseIndex::Slot slot = state.index.SlotOf(id);
		if ((state.hidden[slot] != 0) == cull) continue;
		state.hidden[slot] = cull ? 1 : 0;
		out.push_back({id, cull});
	}
}

} // namespace

void StepCull(CullState& state, float distance, std::vector<MaskWrite>& out) {
	for (size_t i = 0; i < state.zoom.size(); ++i) {
		const bool cull = distance >= state.zoom[i];
		if (cull != (state.culled[i] != 0)) SetSet(state, i, cull, out);
	}
}

void RestoreCull(CullState& state, std::vector<MaskWrite>& out) {
	for (size_t i = 0; i < state.zoom.size(); ++i) SetSet(state, i, false, out);
}

int ShadowClamp(const ShadowCutConfig& config, float value) {
	const float lo = static_cast<float>(config.setting_min), step = static_cast<float>(config.setting_step);
	value = std::clamp(value, lo, static_cast<float>(config.setting_max));
	return static_cast<int>(lo + std::floor((value - lo) / step + 0.5f) * step);   // DESIGN: the middle of the range, rounding to the step
}

float ShadowCutZoom(const ShadowCutConfig& config, int setting, bool capOn) {
	const float own = static_cast<float>(setting);
	return capOn ? std::min(own, config.unit_cap_elmos / config.elmos_per_map_unit) : own;
}

bool ShadowNext(bool far, float zoom, float cut, float hysteresis) {
	if (far) return zoom >= cut * (1 - hysteresis);
	return zoom >= cut * (1 + hysteresis);
}

uint8_t ShadowMask(const ShadowCutConfig& config, uint8_t mask, bool far) {
	if (mask == 0) return 0;
	const uint8_t bits = static_cast<uint8_t>(config.shadow_bits);
	return far ? static_cast<uint8_t>(mask & ~bits) : static_cast<uint8_t>(mask | bits);
}

void StartSweep(IdSweep& sweep, std::span<const int32_t> ids, bool far) {
	sweep.ids.assign(ids.begin(), ids.end());
	sweep.next = 0;
	sweep.far = far;
	sweep.active = true;
}

bool StepSweep(IdSweep& sweep, int batch, std::vector<int32_t>& out) {
	if (!sweep.active) return true;
	const size_t end = std::min(sweep.ids.size(), sweep.next + static_cast<size_t>(std::max(1, batch)));
	out.insert(out.end(), sweep.ids.begin() + sweep.next, sweep.ids.begin() + end);
	sweep.next = end;
	sweep.active = end < sweep.ids.size();
	return !sweep.active;
}

} // namespace KT1::Core::View
