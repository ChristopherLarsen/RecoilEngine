/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the zoom level and gate state, and the log of what is written (LUA-FREE D07).

#include "ViewSettingsState.h"

#include <algorithm>
#include <cstring>

namespace KT1::Core::View {

void SettingsLog::Record(std::string_view target, int32_t id, float value) {
	SettingWrite w;
	const size_t n = std::min(target.size(), static_cast<size_t>(TARGET_TEXT - 1));
	std::memcpy(w.target, target.data(), n);
	w.id = id;
	w.value = value;
	writes.push_back(w);
}

bool WholeMapCheckNeeded(const ZoomBounds& bounds, float distance, int engineLevel) {
	if (engineLevel != ZOOM_NONE) return engineLevel >= ZOOM_LEVELS - 1;
	return distance >= bounds.upper[ZOOM_LEVELS - 2] * bounds.whole_map_from;
}

bool UpdateView(ViewState& state, const ViewSettingsConfig& config, const ViewInputs& inputs) {
	const float distance = inputs.distance >= 0 ? inputs.distance : inputs.height;
	int level = inputs.engineLevel;
	bool wholeMap = inputs.wholeMap;
	if (level == ZOOM_NONE) level = LevelFor(config.bounds, distance, state.level, wholeMap);
	else wholeMap = level >= ZOOM_LEVELS - 1 && wholeMap;
	const bool moved = state.first || level != state.level || inputs.detail != state.detail;
	const uint64_t all = state.table.count >= MAX_GATES ? ~uint64_t(0) : (uint64_t(1) << state.table.count) - 1;
	const uint64_t mask = Evaluate(state.table, level, distance, inputs.detail, state.mask, state.first) & ~state.disabled;
	state.changed = state.first ? all : mask ^ state.mask;
	state.mask = mask;
	state.level = level;
	state.detail = inputs.detail;
	state.distance = distance;
	state.wholeMap = wholeMap;
	state.first = false;
	if (moved) ++state.changes;
	return moved;
}

void RowsToWrite(ViewState& state, const ViewSettingsConfig& config, std::vector<int>& out) {
	for (int i = 0; i < state.table.count; ++i) {
		const ViewRow& row = config.rows[i];
		const uint64_t bit = uint64_t(1) << i;
		if (row.target.empty() || (state.changed & bit) == 0) continue;
		const bool holds = (state.mask & bit) != 0;
		if (!holds && !row.write_other) continue;
		state.log.Record(row.target, -1, holds ? row.value : row.other_value);
		out.push_back(i);
	}
}

int FindGate(const ViewSettingsConfig& config, std::string_view name) {
	for (size_t i = 0; i < config.rows.size() && i < static_cast<size_t>(MAX_GATES); ++i)
		if (!name.empty() && config.rows[i].gate == name) return static_cast<int>(i);
	return -1;
}

int HoldingRow(const ViewState& state, const ViewSettingsConfig& config, int gate) {
	if (gate < 0 || gate >= state.table.count) return -1;
	const std::string& name = config.rows[gate].gate;
	for (int i = gate; i < state.table.count; ++i)
		if (config.rows[i].gate == name && (state.mask & (uint64_t(1) << i)) != 0) return i;
	return -1;
}

} // namespace KT1::Core::View
