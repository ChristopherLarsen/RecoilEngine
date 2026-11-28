/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: Spring.KT1.Icons, what WG.iconLex gave the Lua HUD, and the HUD's options for the native icons, minimap
// layer and bars (LUA-FREE H01, UNITDRAW §4.4). Unsynced. The table is a rule's: Spring.KT1.<rule name>, so the rule
// whose PushLua calls this is registered as "Icons".
//
//   Version()                          -> 3                                      (WG.iconLex.Version)
//   Ok()                               -> bool, why (string when false)
//   HudRml(defID, px [, team [, badges]])  -> markup; badges { cond = "crit"|"dmg"|"fuel", act = "build"|"idle",
//                                         st = bool, vet = level }
//   UnitHudRml(unitID, px)             -> markup with the unit's live badges, "" for no unit
//   EngineIconType(defID)              -> the def's engine icon type before the transparent one was put on, or nil
//   Scale()                            -> the icon scale step in use
//   SwitchDistance()                   -> elmos (hud_core's WG.iconSwitch; it keeps sending iconfadestart from this)
//   PastSwitch()                       -> bool
//   HitAt(x, y)                        -> unitID or nil
//   Rects()                            -> { { unitID, x, y, w, h }, ... } in draw order, count
//   Hovered()                          -> unitID or nil
//   Kind(unitID)                       -> "full", "contact" or nil
//   KindCount()                        -> number
//   SetOptions(t)                      -> nothing; t: relation ("off", "minimap", "everywhere"), scale, uiScale, hidden,
//                                         hideWithUI, minimapHidden, bars, enemybars, palette = { healthy = {r,g,b},
//                                         damaged = {r,g,b}, critical = {r,g,b} } or nil; keys not given keep their value
//   Pause(on)                          -> nothing
//   FlickerTest(unitID)                -> bool
//
// Clicking on icons (icon_lex's MousePress and MouseRelease: box, drag and double-click selection) is input, not drawing:
// it stays a small widget over HitAt and Rects until INPUT moves.

#ifndef KT1_DRAW_ICONS_BRIDGE_H
#define KT1_DRAW_ICONS_BRIDGE_H

struct lua_State;

namespace KT1 {

void PushIconsBridge(lua_State* L);   // fills the table on top of the stack

} // namespace KT1

#endif
