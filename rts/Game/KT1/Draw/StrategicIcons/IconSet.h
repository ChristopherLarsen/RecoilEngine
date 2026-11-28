/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the one icon set both "icons" and "minimapunits" draw from, and what the HUD asks of it (LUA-FREE H01,
// UNITDRAW §4.4; steps D01 and D02 write the bodies). System "Icons" owns it and updates it before its ScreenEffects draw, from this frame's facts:
// rebuilt on unit create, destroy, sight, radar, selection, hover, cloak, option and switch changes, never per frame.
// Inputs from other families come through DrawContext: the selection and the pointer from FrameFacts.
// Lua: Spring.KT1.Icons (IconsBridge.h).

#ifndef KT1_DRAW_ICON_SET_H
#define KT1_DRAW_ICON_SET_H

#include <cstdint>
#include <span>
#include <string>

#include "Game/KT1/Core/IconLexicon/HudOptions.h"
#include "Game/KT1/Core/IconLexicon/IconHud.h"
#include "Game/KT1/Core/IconLexicon/IconState.h"

namespace KT1::Icons {

const Core::IconLexicon::IconState& Set();
const Core::IconLexicon::IconConfig& Config();
const Core::IconLexicon::IconClass* ClassOf(int defId);   // made at Start from def data; null: unknown def
std::span<const Core::IconLexicon::IconClass> Classes();  // by def id

const Core::IconLexicon::HudOptions& Options();
void SetOptions(const Core::IconLexicon::HudOptions& options);   // a differing set marks the whole set

float SwitchDistance();                                    // the config's, scaled by the level-of-detail option
bool PastSwitch();                                         // as of the last update

// the boxes of the drawn icons in draw order, projected when asked and kept for the frame
std::span<const Core::IconLexicon::IconRect> Rects();
int HitAt(int x, int y);                                   // view px, y up: the topmost box's unit, -1
int Hovered();                                             // the unit under the pointer in the icon view, -1
void SetPaused(bool paused);                               // draw nothing (tests, cost comparison)
bool Paused();
bool FlickerTest(int unitId);                              // start the damage flicker; false: the unit has no icon

// the HUD's icon markup (WG.iconLex HudRml and UnitHudRml): "" for no def, no unit, or a key not in the atlas;
// team below 0: the viewer's
std::string Markup(int defId, int px, int team, const Core::IconLexicon::Badges& badges);
std::string UnitMarkup(int unitId, int px);               // with the unit's live badges
const std::string* EngineIconType(int defId);             // the def's engine icon type before the swap; null: not swapped
const char* Kind(int unitId);                             // "full", "contact", or null: no icon
int KindCount();
const char* Failure();                                    // why nothing draws; null: it draws

} // namespace KT1::Icons

#endif
