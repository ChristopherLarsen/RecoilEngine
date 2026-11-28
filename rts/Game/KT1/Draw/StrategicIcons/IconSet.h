/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the one icon set both "icons" and "minimapunits" draw from, and what the HUD asks of it (LUA-FREE H01,
// UNITDRAW §4.4; steps D01 and D02 write the bodies). System "icons" owns and updates it in the draw Update phase:
// rebuilt on unit create, destroy, sight, radar, selection, hover, cloak, option and switch changes, never per frame.
// Inputs from other families come through DrawContext: the selection and the pointer from FrameFacts.
// Lua: Spring.KT1.Icons (IconsBridge.h).

#ifndef KT1_DRAW_ICON_SET_H
#define KT1_DRAW_ICON_SET_H

#include <cstdint>
#include <span>

#include "Game/KT1/Core/IconLexicon/HudOptions.h"
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

} // namespace KT1::Icons

#endif
