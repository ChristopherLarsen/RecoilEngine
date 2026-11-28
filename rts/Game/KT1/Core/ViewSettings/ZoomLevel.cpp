/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the zoom level (as LuaUI/lib/zoomlevel.lua and SupComZoomLevel have it), the level-of-detail option's
// factors (detail_option.lua) and the gate table. LUA-FREE D07.

#include "ZoomLevel.h"

#include <algorithm>
#include <cstdlib>

namespace KT1::Core::View {

int RawLevel(const ZoomBounds& bounds, float distance) {
	for (int i = 0; i < ZOOM_LEVELS - 1; ++i)
		if (distance < bounds.upper[i]) return i;
	return ZOOM_LEVELS - 1;
}

int LevelFor(const ZoomBounds& bounds, float distance, int previous, bool wholeMap) {
	if (wholeMap) return ZOOM_LEVELS - 1;
	const int raw = RawLevel(bounds, distance);
	if (previous == ZOOM_NONE || std::abs(raw - previous) != 1) return raw;
	const int split = raw > previous ? raw : raw + 1;     // counted from 1, as no_hysteresis_split
	if (split == bounds.no_hysteresis_split) return raw;
	const float b = bounds.upper[split - 1];
	if (raw > previous && distance < b * (1 + bounds.hysteresis)) return previous;
	if (raw < previous && distance >= b * (1 - bounds.hysteresis)) return previous;
	return raw;
}

int DetailIndex(const DetailConfig& config, int settingValue) {
	if (settingValue < config.lowest_below) return 0;
	return settingValue >= config.highest_from ? 2 : 1;   // DESIGN: the middle of a bracket
}

float SweepFactor(const DetailSweep& sweep, int detail) {
	detail = std::clamp(detail, 0, DETAILS - 1);
	if (detail == 1) return 1;
	if (sweep.mid[1] < 0 && detail == 2) return 1;        // middle open: "farther or equal" taken as equal; DESIGN: the middle of a bracket
	const auto middle = [](const float (&b)[2]) {
		if (b[0] == 0) return b[1];                       // gone at the first sampled zoom: the upper end is all that is known
		return b[1] < 0 ? b[0] : (b[0] + b[1]) / 2;   // DESIGN: the middle of a bracket
	};
	return middle(detail == 0 ? sweep.low : sweep.high) / middle(sweep.mid);
}

float DetailFactor(const DetailConfig& config, const std::string& kind, int detail) {
	const std::string& name = kind == "effects" ? config.effects_kind : kind;
	for (const DetailSweep& s : config.sweeps)
		if (s.kind == name) return SweepFactor(s, detail);
	return 1;
}

bool CompileRows(const ViewSettingsConfig& config, ViewTable& out) {
	out.count = 0;
	if (config.rows.size() > static_cast<size_t>(MAX_GATES)) return false;
	for (const ViewRow& row : config.rows) {
		ViewGate g;
		if (row.unit == "level") g.unit = RowUnit::Level;
		else if (row.unit == "elmos") g.unit = RowUnit::Elmos;
		else if (row.unit == "depth") g.unit = RowUnit::Depth;
		else if (row.unit == "scale") g.unit = RowUnit::Scale;
		else return false;
		g.from = row.from;
		g.to = row.to;
		g.cap = row.cap;
		g.hysteresis = row.hysteresis;
		for (int i = 0; i < DETAILS; ++i)
			g.detail[i] = row.sweep.empty() ? row.detail[i] : DetailFactor(config.detail, row.sweep, i);
		out.gates[out.count++] = g;
	}
	return true;
}

float Factor(const ViewGate& gate, int detail) { return gate.detail[std::clamp(detail, 0, DETAILS - 1)]; }

float Start(const ViewGate& gate, int detail) { return gate.from * Factor(gate, detail); }

float End(const ViewGate& gate, int detail) {
	if (gate.to < 0) return NO_END;
	const float end = gate.to * Factor(gate, detail);
	return (gate.cap > 0 && gate.cap < end) ? gate.cap : end;
}

bool HoldsAtLevel(const ViewGate& gate, int level) {
	return level >= static_cast<int>(gate.from) && (gate.to < 0 || level <= static_cast<int>(gate.to));
}

bool HoldsAtDistance(const ViewGate& gate, float distance, int detail, bool was) {
	const float band = was ? gate.hysteresis : -gate.hysteresis;   // holding: leave late; not holding: come back late
	const float start = Start(gate, detail);
	if (start > 0 && distance < start * (1 - band)) return false;
	const float end = End(gate, detail);
	return end < 0 || distance < end * (1 + band);
}

bool HoldsAtDepth(const ViewGate& gate, float depth, int detail) {
	ViewGate exact = gate;
	exact.hysteresis = 0;
	return HoldsAtDistance(exact, depth, detail, true);
}

uint64_t Evaluate(const ViewTable& table, int level, float distance, int detail, uint64_t previous, bool first) {
	uint64_t mask = 0;
	for (int i = 0; i < table.count; ++i) {
		const ViewGate& g = table.gates[i];
		const uint64_t bit = uint64_t(1) << i;
		bool on = true;
		if (g.unit == RowUnit::Level) on = HoldsAtLevel(g, level);
		else if (g.unit == RowUnit::Elmos)
			on = first ? HoldsAtDepth(g, distance, detail) : HoldsAtDistance(g, distance, detail, (previous & bit) != 0);
		if (on) mask |= bit;
	}
	return mask;
}

} // namespace KT1::Core::View
