/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: system "MinimapUnits", the minimap's unit layer (LUA-FREE H01, UNITDRAW §3; D02, KT1MinimapUnits.cpp). It
// draws in the MiniMap phase from the icon set (Draw/StrategicIcons/IconSet.h); the instance layout and the rules are
// Core/IconLexicon/IconMinimapState.h. While it draws, the engine's own minimap icon pass is off (SupComMiniMapUnitIcons)
// and MiniMapRefreshRate is the config's. Hidden with the HUD or a hidden minimap (HudOptions).

#ifndef KT1_DRAW_MINIMAP_LAYER_H
#define KT1_DRAW_MINIMAP_LAYER_H

#include "Game/KT1/Core/IconLexicon/IconMinimapState.h"

namespace KT1::MinimapUnits {

const Core::IconLexicon::MinimapState& State();
void SetShown(bool shown);   // a layout without the map hides the layer (WG.iconMinimap.SetEnabled)
bool Shown();

} // namespace KT1::MinimapUnits

#endif
