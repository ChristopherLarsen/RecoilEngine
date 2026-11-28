/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core tests: icon classes and boxes (LUA-FREE D01). Expected values are the Lua's (icon_shape.lua, icon_map.lua)
// with the config's default sizes.

#include <string>

#include "Game/KT1/Core/IconLexicon/IconClass.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::IconLexicon;

namespace {

IconDefFacts Def(const char* outline, const char* glyph, const char* tier, const char* command = "") {
	IconDefFacts d;
	d.outline = outline;
	d.glyph = glyph;
	d.tier = tier;
	d.command = command;
	return d;
}

IconConfig WithKeys(std::initializer_list<const char*> keys) {
	IconConfig c;
	for (const char* key: keys) {
		IconAtlasEntry e;
		e.key = key;
		c.atlas.icons.push_back(e);
	}
	return c;
}

} // namespace

KT1_CASE("ICOCLS-SHAPE", "the class sets the shape, command and experimental first") {
	KT1_CHECK(ShapeFor("land", "guns", 1, false) == Shape::Diamond);
	KT1_CHECK(ShapeFor("", "", 1, false) == Shape::Diamond);
	KT1_CHECK(ShapeFor("hover", "none", 3, false) == Shape::Diamond);
	KT1_CHECK(ShapeFor("square", "bolt", 2, false) == Shape::Square);
	KT1_CHECK(ShapeFor("wall", "none", 1, false) == Shape::Square);
	KT1_CHECK(ShapeFor("circle", "none", 1, false) == Shape::Square);
	KT1_CHECK(ShapeFor("square", "facAir", 2, false) == Shape::Rect);
	KT1_CHECK(ShapeFor("fbomber", "none", 2, false) == Shape::Triangle);
	KT1_CHECK(ShapeFor("transport", "none", 1, false) == Shape::Triangle);
	KT1_CHECK(ShapeFor("land", "star", 1, true) == Shape::Circle);
	KT1_CHECK(ShapeFor("square", "facLand", TIER_EXPERIMENTAL, false) == Shape::Circle);
}

KT1_CASE("ICOCLS-KEY", "the atlas key of a def: shape, mark, direct-fire bars, double outline") {
	KT1_CHECK_EQ(MapKey(Def("land", "guns", "2")), std::string("land|guns|2|0"));
	KT1_CHECK_EQ(MapKey(Def("land", "guns", "X")), std::string("land|guns|3|1"));
	KT1_CHECK_EQ(MapKey(Def("sub", "none", "2")), std::string("sub2|none|0|0"));
	KT1_CHECK_EQ(MapKey(Def("sub", "torpedo", "X")), std::string("sub3|torpedo|0|1"));
	KT1_CHECK_EQ(MapKey(Def("", "", "")), std::string("land|none|0|0"));
}

KT1_CASE("ICOCLS-FALLBACK", "a key out of the atlas falls back to the mapped key, then to the config's") {
	const IconConfig config = WithKeys({"land|guns|1|0", "land|none|0|0", "own|key"});
	IconDefFacts d = Def("land", "guns", "1");
	d.key = "own|key";
	KT1_CHECK_EQ(ClassFor(d, config).key, std::string("own|key"));
	d.key = "missing";
	KT1_CHECK_EQ(ClassFor(d, config).key, std::string("land|guns|1|0"));
	KT1_CHECK_EQ(ClassFor(Def("ship", "aa", "2"), config).key, std::string("land|none|0|0"));
}

KT1_CASE("ICOCLS-FIELDS", "tier bars, mark, domain, draw group and size follow the Lua") {
	const IconConfig config;
	const IconClass tank = ClassFor(Def("land", "guns", "2"), config);
	KT1_CHECK_EQ(static_cast<int>(tank.bars), 2);
	KT1_CHECK(tank.mark && tank.mobile && !tank.rotates && !tank.command && !tank.wall);
	KT1_CHECK_EQ(static_cast<int>(tank.drawGroup), 2);
	KT1_CHECK_EQ(tank.size, 1.0f + 0.1f * 1.0f);
	const IconClass wall = ClassFor(Def("wall", "none", "1"), config);
	KT1_CHECK(wall.wall && !wall.mark && !wall.mobile && wall.domain == Domain::Build);
	KT1_CHECK_EQ(static_cast<int>(wall.drawGroup), 0);
	const IconClass plant = ClassFor(Def("square", "bolt", "3"), config);
	KT1_CHECK_EQ(static_cast<int>(plant.drawGroup), 1);
	const IconClass fighter = ClassFor(Def("intercept", "none", "1"), config);
	KT1_CHECK(fighter.domain == Domain::Air && !fighter.mark);
	KT1_CHECK_EQ(static_cast<int>(fighter.drawGroup), 3);
	const IconClass ship = ClassFor(Def("ship", "gunsIn", "1"), config);
	KT1_CHECK(ship.domain == Domain::Sea);
	KT1_CHECK_EQ(ship.size, 1.1f);
	const IconClass commander = ClassFor(Def("land", "star", "1", "cmd"), config);
	KT1_CHECK(commander.command && commander.mark && commander.shape == Shape::Circle);
	KT1_CHECK_EQ(static_cast<int>(commander.drawGroup), 4);
	KT1_CHECK_EQ(commander.size, 1.45f);
	const IconClass experimental = ClassFor(Def("land", "guns", "X"), config);
	KT1_CHECK_EQ(static_cast<int>(experimental.bars), 3);
	KT1_CHECK_EQ(static_cast<int>(experimental.tier), TIER_EXPERIMENTAL);
	KT1_CHECK_EQ(static_cast<int>(experimental.drawGroup), 4);
	IconDefFacts sized = Def("land", "guns", "1");
	sized.size = 2.5f;
	sized.fake = true;
	KT1_CHECK_EQ(ClassFor(sized, config).size, 2.5f);
	KT1_CHECK(ClassFor(sized, config).fake);
}

KT1_CASE("ICOCLS-ROTATE", "icons stay upright unless the size config turns rotation on, and buildings never turn") {
	IconConfig config;
	config.sizes.rotate = true;
	KT1_CHECK(ClassFor(Def("land", "guns", "1"), config).rotates);
	KT1_CHECK(!ClassFor(Def("square", "bolt", "1"), config).rotates);
}

KT1_CASE("ICOCLS-LOD", "detail levels hold from their pixel sizes") {
	const IconStyleConfig style;
	KT1_CHECK_EQ(Lod(11.0f, style), 0);
	KT1_CHECK_EQ(Lod(12.0f, style), 1);
	KT1_CHECK_EQ(Lod(18.0f, style), 2);
	KT1_CHECK_EQ(Lod(27.5f, style), 2);
	KT1_CHECK_EQ(Lod(28.0f, style), 3);
	KT1_CHECK_EQ(Lod(48.0f, style), 4);
	KT1_CHECK_EQ(Lod(400.0f, style), 4);
}

KT1_CASE("ICOCLS-STEP", "a stored scale reads as its nearest step, a tie going up") {
	const IconSizeConfig sizes;
	KT1_CHECK_EQ(NearestStep(sizes, 1.0f), 1.0f);
	KT1_CHECK_EQ(NearestStep(sizes, 1.2f), 1.0f);
	KT1_CHECK_EQ(NearestStep(sizes, 1.25f), 1.5f);
	KT1_CHECK_EQ(NearestStep(sizes, 1.75f), 2.0f);
	KT1_CHECK_EQ(NearestStep(sizes, 9.0f), 2.0f);
	KT1_CHECK_EQ(NearestStep(sizes, -3.0f), 1.0f);
}

KT1_CASE("ICOCLS-BOX", "the box is the extent plus the rim on every side; unmeasured groups take the land bar strip") {
	const IconConfig config;
	const IconBox land = BoxFor(Shape::Diamond, 1.0f, config);
	KT1_CHECK_EQ(land.w, 18.0f);
	KT1_CHECK_EQ(land.h, 18.0f);
	KT1_CHECK_EQ(land.lift, 2.5f);
	const IconBox structure = BoxFor(Shape::Square, 1.0f, config);
	KT1_CHECK_EQ(structure.w, 16.0f);
	KT1_CHECK_EQ(structure.h, 16.0f);
	KT1_CHECK_EQ(structure.lift, 2.5f);
	const IconBox factory = BoxFor(Shape::Rect, 1.0f, config);
	KT1_CHECK_EQ(factory.w, 20.0f);
	KT1_CHECK_EQ(factory.h, 14.0f);
	const IconBox air = BoxFor(Shape::Triangle, 1.0f, config);
	KT1_CHECK_EQ(air.w, land.w);
	KT1_CHECK_EQ(air.lift, land.lift);
	const IconBox doubled = BoxFor(Shape::Diamond, 2.0f, config);
	KT1_CHECK_EQ(doubled.w, 36.0f);
	KT1_CHECK_EQ(doubled.lift, 5.0f);
}

KT1_CASE("ICOCLS-NUMBER", "a custom parameter is a number only when all of it is; the body size is the shape group's") {
	float value = 7.0f;
	KT1_CHECK(!ParseNumber("", value));
	KT1_CHECK(!ParseNumber("2x", value));
	KT1_CHECK_EQ(value, 7.0f);
	KT1_CHECK(ParseNumber("1.5", value));
	KT1_CHECK_EQ(value, 1.5f);
	const IconConfig config;
	float body[2] = {0.0f, 0.0f};
	BodyOf(Shape::Rect, config, body);
	KT1_CHECK(body[0] == 13.0f && body[1] == 7.0f);
	BodyOf(Shape::Triangle, config, body);
	KT1_CHECK(body[0] == 11.0f && body[1] == 11.0f);
}
