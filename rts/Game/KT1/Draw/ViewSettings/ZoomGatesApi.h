/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the zoom level and the zoom gates, for every draw system (LUA-FREE H01, FRAME §4.3; UNITDRAW and EFFECTS include
// this). Bodies are FRAME step D07's. Until that lands the stubs answer from FrameFacts alone: Level is the facts' level,
// every gate is open, every factor 1 and the change counter stays 0, so a caller's `other` value stands.
// Gate names are the `gate` column of data/kt1/viewsettings.json (Core/ViewSettings/ZoomLevel.h).
// Lua, for widgets that still run: Spring.KT1.Zoom (ZoomBridge.h).

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
// a distance row's start and end in elmos, scaled by the level-of-detail option and capped; `other` for NO_GATE (and an
// end that is not set)
float GateStart(GateId gate, float other);
float GateEnd(GateId gate, float other);
bool GateOpenAtDepth(GateId gate, float depth);   // a depth row against one object's own view depth; true for NO_GATE
// the level-of-detail option's factor for a sweep kind ("models", "bars", "icons", "plants", "effects", ...); 1: unknown
float DetailFactor(std::string_view kind);
bool MinimapLayersOff();                    // far out with the whole map in view: minimap overlay layers are skipped
uint32_t Changes();                         // counts level and option changes: act when it differs from last time

} // namespace ZoomGates
} // namespace KT1

#endif
