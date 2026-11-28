/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the HUD's choices the native icon, minimap and bar draws follow (LUA-FREE H01, UNITDRAW). No engine include.
// The HUD is still Lua: it hands them over with Spring.KT1.Icons.SetOptions (Draw/StrategicIcons/IconsBridge.h) when one
// changes (WG.hud.profile, hud.hidden, hud.Scale, the colour-vision palette of hud_access). Defaults: no HUD.

#ifndef KT1_CORE_ICON_HUD_OPTIONS_H
#define KT1_CORE_ICON_HUD_OPTIONS_H

#include <cstdint>

namespace KT1::Core::IconLexicon {

enum class RelationMode : uint8_t { Off, Minimap, Everywhere };   // icon colours by self, ally, enemy

struct HudOptions {
	RelationMode relation = RelationMode::Off;   // profile.iconRelation
	float iconScale = 1.0f;                      // profile.iconscale, before the nearest step
	float uiScale = 1.0f;                        // hud.Scale()
	bool hidden = false;                         // the HUD is hidden
	bool iconsHideWithUi = false;                // profile.iconsHideWithUI
	bool minimapHidden = false;                  // profile.minimapHidden
	bool bars = true;                            // profile.bars
	bool enemyBars = false;                      // profile.enemybars
	bool paletteChosen = false;                  // a colour-vision palette replaces the bars' status colours
	float healthy[3] = {0.0f, 0.0f, 0.0f};
	float damaged[3] = {0.0f, 0.0f, 0.0f};
	float critical[3] = {0.0f, 0.0f, 0.0f};
	uint32_t changes = 0;                        // moves with every SetOptions that differs
};

} // namespace KT1::Core::IconLexicon

#endif
