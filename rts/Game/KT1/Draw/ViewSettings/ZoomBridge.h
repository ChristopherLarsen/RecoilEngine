/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: Spring.KT1.Zoom for the widgets that still run (LUA-FREE H01, FRAME §4.3). It replaces WG.ZoomLevel,
// WG.shadowZoom and WG.zoomSwitch (the camera widgets' WG.stratZoom stays with them until INPUT moves).
// Unsynced. The table is a rule's: Spring.KT1.<rule name>, so the rule whose PushLua calls this is registered as "Zoom".
//
//   Level()                      -> level (0..7, -1 before the first read), distance in elmos      (WG.ZoomLevel.Get)
//   WholeMap()                   -> bool                                                          (WG.ZoomLevel.WholeMap)
//   Bounds()                     -> { b1 .. b7 }, 7 (the highest level)                           (WG.ZoomLevel.BOUNDS, MAX)
//   Changes()                    -> counter of level and option changes (a Lua shim raises ZoomLevelChanged from it)
//   Gate(name)                   -> open (bool), value, start, end; nil for an unknown gate
//   DetailFactor(kind)           -> number
//   MinimapGap()                 -> seconds between minimap model redraws at this level
//   MinimapLayersOff()           -> bool
//   ShadowDistance()             -> setting (mu), far (bool), cut (mu)                            (WG.shadowZoom.Get)
//   SetShadowDistance(value)     -> nothing: clamps, applies at once                              (WG.shadowZoom.Set)
//   ShadowFix(mask)              -> the mask with its shadow bits as the setting has them         (WG.shadowZoom.Fix)
//   ZoomSwitch()                 -> level (fraction), reflection (0..1), native (bool)            (WG.zoomSwitch)
//   ZoomSwitchLevels()           -> { 1, 0.5, 0.25 }
//   SetZoomSwitch(value)         -> bool: value in percent or as a fraction, one of the levels
//   SetEngineFog(on)             -> nothing: the vision widget says the engine fog mask is on, so the deferred model
//                                   pass is unused (WG.vision.Engine)
//   Report(target, id, value)    -> nothing: check mode, the Lua widgets report what they set (column settings_written;
//                                   id -1 for an engine setting)

#ifndef KT1_DRAW_ZOOM_BRIDGE_H
#define KT1_DRAW_ZOOM_BRIDGE_H

struct lua_State;

namespace KT1 {

void PushZoomBridge(lua_State* L);   // fills the table on top of the stack

} // namespace KT1

#endif
