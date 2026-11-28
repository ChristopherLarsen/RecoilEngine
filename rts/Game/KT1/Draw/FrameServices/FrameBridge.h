/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: Spring.KT1.Frame, the frame services for the widgets that still run (LUA-FREE H01, FRAME §4.1). Unsynced.
// The table is a rule's: Spring.KT1.<rule name>, so the rule whose PushLua calls this is registered as "Frame".
//
//   Selection()      -> ids (ascending array), count, changes. The same table is returned until the selection changes:
//                       a reader must not write to it
//   IsSelected(id)   -> bool
//   Hover()          -> kind ("unit", "feature", "ground", "sky", nil outside the view), id or nil, x, y, z of the ground
//                       under the pointer (nil when none), overMiniMap          (Spring.TraceScreenRay at the mouse)
//   HoverGround()    -> x, y, z or nothing: the ground under the pointer, the minimap's world point when over it
//   ViewCentre()     -> x, y, z or nothing: the ground at the middle of the view
//   Camera()         -> table { px, py, pz, dx, dy, dz, rx, ry, dist, height, fov, dMin, dMax, name, vsx, vsy }; the
//                       same table, refilled when the camera changes
//   Mouse()          -> x, y, left, middle, right, offscreen, alt, ctrl, meta, shift
//   Zoom()           -> level, distance, wholeMap
//   Changed(what)    -> counter; what: "selection", "hover", "camera", "zoom" or "mouse"

#ifndef KT1_DRAW_FRAME_BRIDGE_H
#define KT1_DRAW_FRAME_BRIDGE_H

struct lua_State;

namespace KT1 {

void PushFrameBridge(lua_State* L);   // fills the table on top of the stack

} // namespace KT1

#endif
