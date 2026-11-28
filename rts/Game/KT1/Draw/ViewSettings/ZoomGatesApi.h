/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the zoom level and the zoom gates, for every draw system (LUA-FREE H01, FRAME §4.3; UNITDRAW and EFFECTS include
// this). Bodies are FRAME step D07's. Until that lands the stubs answer from FrameFacts alone: Level is the facts' level,
// every gate is open and the change counter stays 0.
// Lua, for widgets that still run: Spring.KT1.Zoom.Level(), .Gate(name), .WholeMap() (they replace WG.ZoomLevel,
// WG.shadowZoom and WG.stratZoom).

#ifndef KT1_DRAW_ZOOM_GATES_API_H
#define KT1_DRAW_ZOOM_GATES_API_H

#include <cstdint>
#include <string_view>

namespace KT1 {

class FrameFacts;

namespace ZoomGates {

using GateId = int;
constexpr GateId NO_GATE = -1;

// the zoom level of the drawn frame, 0..7; Core::View::ZOOM_NONE before the first read
int Level(const FrameFacts& facts);
bool WholeMap();                            // the whole map is in the main view
GateId Gate(std::string_view name);         // look up once (Start), NO_GATE for an unknown name
bool GateOpen(GateId gate);                 // true for NO_GATE
float GateValue(GateId gate, float other);  // the open row's value; `other` when no row of the gate holds
uint32_t Changes();                         // counts level and option changes: act when it differs from last time

} // namespace ZoomGates
} // namespace KT1

#endif
