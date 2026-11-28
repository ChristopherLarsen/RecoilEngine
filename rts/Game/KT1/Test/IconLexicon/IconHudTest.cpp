/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core tests: the HUD markup and badges (LUA-FREE D01). The expected markup is what icon_lex's HudRml writes for
// the same atlas cell.

#include <string>

#include "Game/KT1/Core/IconLexicon/IconHud.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::IconLexicon;

namespace {

IconConfig Atlas() {
	IconConfig c;
	IconAtlasEntry e;
	e.key = "land|guns|1|0";
	const float cells[5][3] = {{1, 2, 18}, {3, 4, 24}, {5, 6, 34}, {7, 8, 52}, {9, 10, 102}};
	for (int i = 0; i < 3; ++i) {
		e.l0[i] = cells[0][i];
		e.l1[i] = cells[1][i];
		e.l2[i] = cells[2][i];
		e.l3[i] = cells[3][i];
		e.l4[i] = cells[4][i];
	}
	c.atlas.icons.push_back(e);
	for (const char* key: {"b|crit|2", "b|idle|2", "b|vet|2", "b|vet|3"}) {
		IconBadgeEntry b;
		b.key = key;
		const float cell[7] = {11, 12, 8, 6, 1, 2, 20};
		for (int i = 0; i < 7; ++i) b.cell[i] = cell[i];
		c.atlas.badges.push_back(b);
	}
	return c;
}

IconClass Tank() {
	IconClass c;
	c.key = "land|guns|1|0";
	c.mobile = true;
	return c;
}

int Count(const std::string& text, const std::string& part) {
	int n = 0;
	for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) ++n;
	return n;
}

} // namespace

KT1_CASE("ICOHUD-RML", "one icon: the tinted fill and the line work in a box of the asked size") {
	const IconConfig c = Atlas();
	const std::string got = HudRml(c, Tank(), 14, {{0.5f, 0.25f, 1.0f}, false}, {});
	const std::string want =
		"<div class=\"lexico\" style=\"position: relative; width: 14dp; height: 14dp;\">"
		"<img src=\"../../LuaUI/icons/hud3_fill.png\" rect=\"3 4 24 24\" style=\"position: absolute; left: -5.00dp; "
		"top: -5.00dp; width: 24.00dp; height: 24.00dp; image-color: rgba(127, 63, 255, 255);\"/>"
		"<img src=\"../../LuaUI/icons/hud3_line.png\" rect=\"3 4 24 24\" style=\"position: absolute; left: -5.00dp; "
		"top: -5.00dp; width: 24.00dp; height: 24.00dp;\"/></div>";
	KT1_CHECK_EQ(got, want);
	IconClass unknown = Tank();
	unknown.key = "none";
	KT1_CHECK_EQ(HudRml(c, unknown, 14, {}, {}), std::string());
}

KT1_CASE("ICOHUD-INVERTED", "an enemy in relation mode takes the dark line work; a cloaked unit's fill is faded") {
	const IconConfig c = Atlas();
	Badges badges;
	badges.cloaked = true;
	const std::string got = HudRml(c, Tank(), 14, {{1.0f, 0.0f, 0.0f}, true}, badges);
	KT1_CHECK(got.find("hud3_line_inv.png") != std::string::npos);
	KT1_CHECK(got.find("rgba(255, 0, 0, 255); opacity: 0.35;\"/>") != std::string::npos);
}

KT1_CASE("ICOHUD-BADGES", "badges from the third detail level: two at that level, all above it") {
	const IconConfig c = Atlas();
	Badges badges;
	badges.condition = Condition::Critical;
	badges.activity = Activity::Idle;
	badges.veterancy = 2;
	KT1_CHECK_EQ(Count(HudRml(c, Tank(), 16, {}, badges), "hud_badges.png"), 0);
	const std::string two = HudRml(c, Tank(), 20, {}, badges);
	KT1_CHECK_EQ(Count(two, "hud_badges.png"), 2);
	KT1_CHECK(two.find("rect=\"11 12 8 6\" style=\"position: absolute; left: 1.00dp; top: 2.00dp; width: 8.00dp; "
	                   "height: 6.00dp;\"/>") != std::string::npos);
	KT1_CHECK_EQ(Count(HudRml(c, Tank(), 32, {}, badges), "hud_badges.png"), 1);
}

KT1_CASE("ICOHUD-FOR", "condition from health and fuel, activity and cloak for own and allies, veterancy for all") {
	const IconStyleConfig style;
	IconClass fighter = Tank();
	fighter.domain = Domain::Air;
	IconUnitFacts u;
	u.relation = Relation::Self;
	u.maxHealth = 100.0f;
	u.health = 30.0f;
	u.idle = true;
	u.cloaked = true;
	u.fuelLow = true;
	u.veterancy = 3;
	Badges b = BadgesFor(style, fighter, u);
	KT1_CHECK(b.condition == Condition::Damaged && b.activity == Activity::Idle && b.cloaked && b.veterancy == 3);
	u.health = 20.0f;
	KT1_CHECK(BadgesFor(style, fighter, u).condition == Condition::Critical);
	u.health = 90.0f;
	KT1_CHECK(BadgesFor(style, fighter, u).condition == Condition::Fuel);
	KT1_CHECK(BadgesFor(style, Tank(), u).condition == Condition::None);
	u.building = true;
	KT1_CHECK(BadgesFor(style, fighter, u).activity == Activity::Build);
	u.relation = Relation::Enemy;
	b = BadgesFor(style, fighter, u);
	KT1_CHECK(b.condition == Condition::None && b.activity == Activity::None && !b.cloaked && b.veterancy == 3);
}

KT1_CASE("ICOHUD-FUEL", "low fuel needs a capacity; the def's low share or the style's; unknown fuel reads full") {
	const IconStyleConfig style;
	KT1_CHECK(!FuelLow(style, "", "", true, 0.0f));
	KT1_CHECK(!FuelLow(style, "0", "", true, 0.0f));
	KT1_CHECK(FuelLow(style, "100", "", true, 18.0f));
	KT1_CHECK(!FuelLow(style, "100", "", true, 19.0f));
	KT1_CHECK(FuelLow(style, "100", "0.5", true, 40.0f));
	KT1_CHECK(!FuelLow(style, "100", "0.5", false, 0.0f));
}
