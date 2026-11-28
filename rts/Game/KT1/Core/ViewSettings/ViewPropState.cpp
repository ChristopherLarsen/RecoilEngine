/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: prop distances (gfx_prop_distance, RND-003 R5) and the map objects' zoom gate (RND-004). Props are bucketed
// in square cells: a cell wholly inside or outside a distance is switched once when that changes, only cells the
// distance passes through are checked prop by prop. LUA-FREE D07; the types are in ViewCullState.h.

#include "ViewCullState.h"

#include <algorithm>
#include <cmath>

#include "ViewSceneState.h"

namespace KT1::Core::View {
namespace {

constexpr int NOT_LISTED = -1;
constexpr uint8_t ALL_BEYOND = 0, ALL_WITHIN = 1, MIXED = 2, LOOK = 3;   // PropCell::inside; DESIGN: size bands, cell key stride and axes
constexpr int LONGEST_BAND = 4;                                         // sides past the last edge; DESIGN: size bands, cell key stride and axes
constexpr int64_t KEY_STRIDE = 65536;                                   // cell key: x * stride + z; DESIGN: size bands, cell key stride and axes

// the band the config's class table gives; NOT_LISTED: by size
int ClassBand(const PropCullConfig& config, std::string_view cls) {
	for (const PropClassBand& c : config.classes)
		if (c.name == cls) return c.band;
	return NOT_LISTED;
}

int Band(const PropCullConfig& config, const PropFacts& prop) {
	const int listed = ClassBand(config, prop.propClass);
	if (listed != NOT_LISTED) return listed;
	const float side = prop.side / config.elmos_per_map_unit;
	for (int i = 0; i < 3; ++i)   // DESIGN: size bands, cell key stride and axes
		if (side <= config.size_edges[i]) return i + 1;
	return LONGEST_BAND;
}

void SetHidden(PropState& state, DenseIndex::Slot slot, uint8_t reason, bool hide, std::vector<MaskWrite>& out) {
	uint8_t& by = state.hiddenBy[slot];
	const bool was = by != 0;
	by = hide ? static_cast<uint8_t>(by | reason) : static_cast<uint8_t>(by & ~reason);
	if (was != (by != 0)) out.push_back({state.index.IdOf(slot), !was});
}

void PassGroup(PropState& state, PropCell& cell, size_t g, uint8_t now, const float (&cam)[3], std::vector<MaskWrite>& out) {
	const float cut = cell.cuts[g];
	if (now == MIXED) {
		for (const int32_t id : cell.members[g]) {
			const DenseIndex::Slot s = state.index.SlotOf(id);
			const float dx = state.x[s] - cam[0], dy = state.y[s] - cam[1], dz = state.z[s] - cam[2];
			// not drawn at or beyond the last distance (RND-002 R2)
			SetHidden(state, s, HIDDEN_BY_DISTANCE, std::sqrt(dx * dx + dy * dy + dz * dz) >= cut, out);
		}
	} else if (now != cell.inside[g]) {
		for (const int32_t id : cell.members[g])
			SetHidden(state, state.index.SlotOf(id), HIDDEN_BY_DISTANCE, now == ALL_BEYOND, out);
	}
	cell.inside[g] = now;
}

int32_t CellAt(PropState& state, float side, float x, float y, float z) {
	const int cx = static_cast<int>(std::floor(x / side)), cz = static_cast<int>(std::floor(z / side));
	const int64_t key = cx * KEY_STRIDE + cz;
	const auto [it, made] = state.cellOf.try_emplace(key, static_cast<int32_t>(state.cells.size()));
	if (made) {
		PropCell c;
		c.key = key;
		c.x0 = cx * side;
		c.z0 = cz * side;
		c.ymin = c.ymax = y;
		state.cells.push_back(std::move(c));
	}
	PropCell& c = state.cells[it->second];
	c.ymin = std::min(c.ymin, y);
	c.ymax = std::max(c.ymax, y);
	return it->second;
}

} // namespace

float PropCut(const PropCullConfig& config, const PropFacts& prop, float lodScale) {
	// the map's own features that are no props by class (decor wrecks) take the plants' scaled distance, no floor
	if (prop.decor) return config.last_plant * lodScale * config.elmos_per_map_unit;
	if (prop.wreck || prop.propClass.empty()) return NO_END;
	const int band = Band(config, prop);
	float cut = config.last_plant;
	if (band != 0) cut = band < LONGEST_BAND ? config.last_by_size[band - 1] : config.last_longest;
	const float own = std::min(cut, config.last_plant) * config.elmos_per_map_unit;   // the R5 ceiling
	cut = own * lodScale;
	if (lodScale > 0) cut = std::max(cut, std::min(own, config.cut_floor_elmos));     // never a cut at play zoom
	return cut;
}

int PropSize(const PropCullConfig& config, const PropFacts& prop) {
	return std::clamp(Band(config, prop), 1, PROP_SIZES) - 1;   // plants and small rocks, the middle band, the large
}

void PropState::Init(int32_t maxFeatureIds) {
	index.Init(maxFeatureIds);
	x.Init(index);
	y.Init(index);
	z.Init(index);
	cut.Init(index);
	size.Init(index);
	hiddenBy.Init(index);
	savedMask.Init(index);
	cell.Init(index);
	cells.clear();
	cellOf.clear();
	for (int i = 0; i < PROP_SIZES; ++i) {
		bySize[i].clear();
		gone[i] = false;
	}
	dirty = true;
}

void AddProp(PropState& state, const PropCullConfig& config, int32_t id, const PropFacts& prop, float cut, std::vector<MaskWrite>& out) {
	if (!state.index.InRange(id) || state.index.Has(id)) return;
	const int32_t at = CellAt(state, config.cell_elmos, prop.x, prop.y, prop.z);
	PropCell& c = state.cells[at];
	const auto found = std::find(c.cuts.begin(), c.cuts.end(), cut);
	const size_t g = static_cast<size_t>(found - c.cuts.begin());
	if (found == c.cuts.end()) {
		c.cuts.push_back(cut);
		c.inside.push_back(LOOK);
		c.members.emplace_back();
	}
	c.members[g].push_back(id);
	c.inside[g] = LOOK;                                    // evaluated again on the next pass
	const DenseIndex::Slot s = state.index.Add(id);
	state.x[s] = prop.x;
	state.y[s] = prop.y;
	state.z[s] = prop.z;
	state.cut[s] = cut;
	state.size[s] = static_cast<uint8_t>(PropSize(config, prop));
	state.cell[s] = at;
	state.bySize[state.size[s]].push_back(id);
	state.dirty = true;
	if (state.gone[state.size[s]]) SetHidden(state, s, HIDDEN_BY_ZOOM, true, out);
}

void RemoveProp(PropState& state, int32_t id) {
	if (!state.index.Has(id)) return;
	const DenseIndex::Slot s = state.index.SlotOf(id);
	std::erase(state.bySize[state.size[s]], id);
	for (std::vector<int32_t>& m : state.cells[state.cell[s]].members) std::erase(m, id);
	const DenseIndex::Move move = state.index.Remove(id);
	state.x.Apply(move);
	state.y.Apply(move);
	state.z.Apply(move);
	state.cut.Apply(move);
	state.size.Apply(move);
	state.hiddenBy.Apply(move);
	state.savedMask.Apply(move);
	state.cell.Apply(move);
}

void PropDistancePass(PropState& state, const PropCullConfig& config, const float (&cam)[3], std::vector<MaskWrite>& out) {
	const float mx = cam[0] - state.lastCam[0], my = cam[1] - state.lastCam[1], mz = cam[2] - state.lastCam[2];
	if (!state.dirty && mx * mx + my * my + mz * mz <= config.move_elmos * config.move_elmos) return;
	std::copy(std::begin(cam), std::end(cam), std::begin(state.lastCam));
	state.dirty = false;
	const float side = config.cell_elmos;
	for (PropCell& c : state.cells) {
		// the nearest and farthest point of the cell box (ground height range of its props) from the camera
		const float nx = std::max(c.x0, std::min(cam[0], c.x0 + side)) - cam[0];
		const float nz = std::max(c.z0, std::min(cam[2], c.z0 + side)) - cam[2];
		const float ny = std::max(c.ymin, std::min(cam[1], c.ymax)) - cam[1];
		const float fx = std::max(std::fabs(c.x0 - cam[0]), std::fabs(c.x0 + side - cam[0]));
		const float fz = std::max(std::fabs(c.z0 - cam[2]), std::fabs(c.z0 + side - cam[2]));
		const float fy = std::max(std::fabs(c.ymin - cam[1]), std::fabs(c.ymax - cam[1]));
		const float near2 = nx * nx + ny * ny + nz * nz, far2 = fx * fx + fy * fy + fz * fz;
		for (size_t g = 0; g < c.cuts.size(); ++g) {
			const float cut2 = c.cuts[g] * c.cuts[g];
			const uint8_t now = far2 <= cut2 ? ALL_WITHIN : (near2 > cut2 ? ALL_BEYOND : MIXED);
			PassGroup(state, c, g, now, cam, out);
		}
	}
}

void PropZoomPass(PropState& state, const ViewSettingsConfig& config, float distance, float level, std::vector<MaskWrite>& out) {
	for (int k = 0; k < PROP_SIZES; ++k) {
		const float size = static_cast<float>(k) / (PROP_SIZES - 1);   // 0 small, half middle, 1 large
		const float alpha = PropFadeAlpha(config, distance, size, level);
		bool gone = state.gone[k];
		if (!gone && alpha < config.props.gone_below) gone = true;
		else if (gone && alpha > config.props.back_above) gone = false;
		if (gone == state.gone[k]) continue;
		state.gone[k] = gone;
		for (const int32_t id : state.bySize[k]) SetHidden(state, state.index.SlotOf(id), HIDDEN_BY_ZOOM, gone, out);
	}
}

void RestoreProps(PropState& state, std::vector<MaskWrite>& out) {
	for (DenseIndex::Slot s = 0; s < state.index.Size(); ++s) {
		if (state.hiddenBy[s] == 0) continue;
		state.hiddenBy[s] = 0;
		out.push_back({state.index.IdOf(s), false});
	}
	for (bool& g : state.gone) g = false;
	for (PropCell& c : state.cells) std::fill(c.inside.begin(), c.inside.end(), LOOK);
	state.dirty = true;
}

} // namespace KT1::Core::View
