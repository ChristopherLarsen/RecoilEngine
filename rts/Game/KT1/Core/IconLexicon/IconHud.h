/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the icon lexicon's markup for the Lua HUD (LUA-FREE H01, UNITDRAW §4.4; replaces HudRml, HudCell and Badges
// of icon_lex, which hud_build, hud_infocard, hud_quick, hud_select, hud_strip and hud_unitpanel call). No engine include.
// Image paths are relative to a document two folders deep (LuaUI/hud/).

#ifndef KT1_CORE_ICON_HUD_H
#define KT1_CORE_ICON_HUD_H

#include <cstdint>
#include <string>
#include <string_view>

#include "IconState.h"

namespace KT1::Core::IconLexicon {

enum class Condition : uint8_t { None, Damaged, Critical, Fuel };
enum class Activity : uint8_t { None, Build, Idle };

struct Badges {
	Condition condition = Condition::None;
	Activity activity = Activity::None;
	bool cloaked = false;
	int veterancy = 0;
};

// condition from health (and fuel for own aircraft), activity and cloak for own and allied units, veterancy for all
Badges BadgesFor(const IconStyleConfig& style, const IconClass& cls, const IconUnitFacts& unit);

// an aircraft's low-fuel badge: capacity and low share are the def's parameters (low empty: the style's), fuel its rules
// parameter (not known: full). False for a def without a capacity.
bool FuelLow(const IconStyleConfig& style, std::string_view capacity, std::string_view lowShare, bool known, float fuel);

// markup for one upright icon `px` wide: the fill tinted, the line work, then the badges. Empty for a key not in the atlas.
std::string HudRml(const IconConfig& config, const IconClass& cls, int px, const Paint& paint, const Badges& badges);

} // namespace KT1::Core::IconLexicon

#endif
