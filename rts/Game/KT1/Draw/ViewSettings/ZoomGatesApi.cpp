/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the zoom gates for every draw system (ZoomGatesApi.h), answered from the state the Zoom system keeps. Before
// its first frame the level is the frame facts' and every gate is open.

#include "ZoomGatesApi.h"

#include <string>

#include "ViewShared.h"

#include "Game/KT1/Draw/Common/FrameFacts.h"

namespace KT1 {
namespace ZoomGates {
namespace {

namespace View = Core::View;
using ViewSet::Shared;

bool Known(GateId g) { return g >= 0 && g < Shared().view.table.count; }

} // namespace

int Level(const FrameFacts& facts) { return Shared().view.level == View::ZOOM_NONE ? facts.ZoomLevel() : Shared().view.level; }
bool WholeMap() { return Shared().view.wholeMap; }
GateId Gate(std::string_view name) { return View::FindGate(Shared().config, name); }
bool GateOpen(GateId g) {
	if (!Known(g) || Shared().view.first) return true;
	return View::HoldingRow(Shared().view, Shared().config, g) >= 0;
}
float GateValue(GateId g, float other) {
	const int row = Known(g) ? View::HoldingRow(Shared().view, Shared().config, g) : -1;
	return row < 0 ? other : Shared().config.rows[row].value;
}
float GateStart(GateId g, float other) {
	if (!Known(g)) return other;
	return View::Start(Shared().view.table.gates[g], Shared().view.detail);
}
float GateEnd(GateId g, float other) {
	if (!Known(g)) return other;
	const float end = View::End(Shared().view.table.gates[g], Shared().view.detail);
	return end < 0 ? other : end;
}
bool GateOpenAtDepth(GateId g, float depth) {
	return !Known(g) || View::HoldsAtDepth(Shared().view.table.gates[g], depth, Shared().view.detail);
}
float DetailFactor(std::string_view kind) {
	return View::DetailFactor(Shared().config.detail, std::string(kind), Shared().view.detail);
}
bool MinimapLayersOff() { return View::MinimapLayersOff(Shared().config.minimap, Shared().view.level, Shared().view.wholeMap); }
uint32_t Changes() { return Shared().view.changes; }

} // namespace ZoomGates

} // namespace KT1
