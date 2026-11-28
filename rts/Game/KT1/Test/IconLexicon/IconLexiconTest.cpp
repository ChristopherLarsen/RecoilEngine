/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon tests: shape, size, kind and fill colour (LUA-FREE-WORKPACKAGES D01).
// The UI-424 cases use the requirement's synthetic fixture SF-UI-424 (UI-424 v5, acceptance cases); the ICO cases port
// tools/test/cases/icon_shape.lua and ico_424.lua, whose answers the Lua path gives today.

#include "Game/KT1/Core/IconLexicon/IconLexiconCore.h"
#include "Game/KT1/Test/KT1Test.h"

#include <cmath>

using namespace KT1::Core::IconLexicon;

namespace {
	bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

	// SF-UI-424: group a (body 12 x 12, extent 18 x 18) is the land vehicle (diamond); group b (body 10 x 6, extent 14 x 10)
	// is the factory (wide rectangle); rim 1 px; a is the unmeasured group.
	SizeTable Fixture() {
		SizeTable t;
		for (auto& b: t.body) b = {12, 12};
		t.body[static_cast<int>(Group::LandVehicle)] = {12, 12};
		t.extent[static_cast<int>(Group::LandVehicle)] = {18, 18};
		t.hasExtent[static_cast<int>(Group::LandVehicle)] = true;
		t.body[static_cast<int>(Group::Factory)] = {10, 6};
		t.extent[static_cast<int>(Group::Factory)] = {14, 10};
		t.hasExtent[static_cast<int>(Group::Factory)] = true;
		t.unmeasuredGroup = Group::LandVehicle;
		t.selectedRimPx = 1;
		return t;
	}
	IconFields Land(int tier = 1) { IconFields f; f.outline = Outline::Land; f.tier = tier; return f; }
	constexpr float TOL = 1; // UI-424 v5: tolerance 1 pixel
}

KT1_CASE("ICOLEX-UI424-A", "UI-424-A: the a icon at scale 1 has extent 18 x 18 and body 12 x 12 (no camera input exists)") {
	const Box b = IconBox(Shape::Diamond, 1, Fixture());
	KT1_CHECK(Near(b.extentW, 18, TOL) && Near(b.extentH, 18, TOL));
	KT1_CHECK(Near(b.bodyW, 12, TOL) && Near(b.bodyH, 12, TOL));
}

KT1_CASE("ICOLEX-UI424-B", "UI-424-B: selected adds a 1 px rim on every side: the drawn box is 20 x 20") {
	const Box b = IconBox(Shape::Diamond, 1, Fixture());
	KT1_CHECK(Near(b.w, 20, TOL) && Near(b.h, 20, TOL));
	KT1_CHECK_EQ(IconFlags(true, false, false), 1);
}

KT1_CASE("ICOLEX-UI424-C", "UI-424-C: the b icon at scale 1.75: extent 24.5 x 17.5, body 17.5 x 10.5, rim scaled") {
	const Box b = IconBox(Shape::Rect, 1.75f, Fixture());
	KT1_CHECK(Near(b.extentW, 24.5f, TOL) && Near(b.extentH, 17.5f, TOL));
	KT1_CHECK(Near(b.bodyW, 17.5f, TOL) && Near(b.bodyH, 10.5f, TOL));
	KT1_CHECK(Near(b.w - b.extentW, 2 * 1.75f, TOL));
}

KT1_CASE("ICOLEX-UI424-D", "UI-424-D/H: tiers one and three draw the same size; one and three bars") {
	const SizeTable t = Fixture();
	const IconLexiconConfig c = DefaultConfig();
	Box b1, b3;
	KT1_CHECK(DrawnBox(Land(1), 1, false, c, b1));
	KT1_CHECK(DrawnBox(Land(3), 1, false, c, b3));
	KT1_CHECK(b1.w == b3.w && b1.h == b3.h);
	KT1_CHECK_EQ(TierBars(Land(1)), 1);
	KT1_CHECK_EQ(TierBars(Land(3)), 3);
	KT1_CHECK_EQ(TierBars(Land(4)), 3);
	KT1_CHECK(IconBox(ShapeOf(Land(1)), 1, t).w == IconBox(ShapeOf(Land(3)), 1, t).w);
}

KT1_CASE("ICOLEX-UI424-G", "UI-424-G: hovered keeps its size, sets only the hovered flag") {
	KT1_CHECK_EQ(IconFlags(false, true, false), 2);
	KT1_CHECK_EQ(IconFlags(true, true, true), 7);
}

KT1_CASE("ICOLEX-UI424-I", "UI-424-I: icon scale step 1.75 multiplies extent and body") {
	const SizeTable t = Fixture();
	const Box a = IconBox(Shape::Diamond, 1, t), b = IconBox(Shape::Diamond, 1.75f, t);
	KT1_CHECK(Near(b.extentW, a.extentW * 1.75f, TOL) && Near(b.bodyH, a.bodyH * 1.75f, TOL));
}

KT1_CASE("ICOLEX-UI424-J", "UI-424-J: a shape with no measured size draws as the unmeasured group") {
	const SizeTable t = Fixture();
	KT1_CHECK(GroupOf(Shape::Triangle, t) == Group::LandVehicle);
	const Box tri = IconBox(Shape::Triangle, 1, t), dia = IconBox(Shape::Diamond, 1, t);
	KT1_CHECK(tri.w == dia.w && tri.h == dia.h);
}

KT1_CASE("ICOLEX-UI424-K", "UI-424-K: two classes with the same shape are the same size (size follows shape)") {
	IconFields gun = Land(), plain = Land();
	gun.hasGlyph = true;
	KT1_CHECK(ShapeOf(gun) == ShapeOf(plain));
	KT1_CHECK(HasMark(gun) && !HasMark(plain));
}

KT1_CASE("ICOLEX-SHAPE", "icon_shape.lua Shape: land diamond, structure and extractor square, factory rect, commander and experimental round, fighter triangle") {
	IconFields structure; structure.outline = Outline::Square;
	IconFields extractor; extractor.outline = Outline::Circle;
	IconFields factory; factory.outline = Outline::Square; factory.factoryGlyph = true;
	IconFields commander = Land(); commander.command = true;
	IconFields fighter; fighter.outline = Outline::Intercept;
	KT1_CHECK(ShapeOf(Land()) == Shape::Diamond);
	KT1_CHECK(ShapeOf(structure) == Shape::Square);
	KT1_CHECK(ShapeOf(extractor) == Shape::Square);
	KT1_CHECK(ShapeOf(factory) == Shape::Rect);
	KT1_CHECK(ShapeOf(commander) == Shape::Circle);
	KT1_CHECK(ShapeOf(Land(4)) == Shape::Circle);
	KT1_CHECK(ShapeOf(fighter) == Shape::Triangle);
	KT1_CHECK(HasMark(commander));
}

KT1_CASE("ICOLEX-DESIGN-SIZES", "design table: land box 18 x 18 with rim (16 + 2), structure smaller, commander not larger, dy puts the body on the unit") {
	const SizeTable t = DefaultConfig().sizes;
	const Box land = IconBox(Shape::Diamond, 1, t), st = IconBox(Shape::Square, 1, t), cm = IconBox(Shape::Circle, 1, t);
	KT1_CHECK_EQ(land.w, 18.0f);
	KT1_CHECK_EQ(land.h, 18.0f);
	KT1_CHECK(st.bodyW < land.bodyW);
	KT1_CHECK(cm.bodyW <= land.bodyW);
	KT1_CHECK_EQ(land.dy, 9.0f - (1 + 5.5f)); // h/2 - (rim + body/2)
	// the structure has no measured extent: its body plus the land bar strip (5 x 5)
	KT1_CHECK_EQ(st.extentW, 14.0f);
	KT1_CHECK_EQ(st.extentH, 14.0f);
}

KT1_CASE("ICOLEX-WALL-DETAIL", "a small wall is hidden unless Alt; Alt draws a small box at 20 px") {
	const IconLexiconConfig c = DefaultConfig();
	IconFields wall; wall.outline = Outline::Wall;
	Box b;
	KT1_CHECK(!DrawnBox(wall, 1, false, c, b));
	KT1_CHECK(DrawnBox(wall, 1, true, c, b));
	KT1_CHECK(DrawnBox(Land(), 0.5f, true, c, b));
	KT1_CHECK(Near(std::fmax(b.w, b.h), 20, 0.001f));
	KT1_CHECK(DrawnBox(Land(), 2, true, c, b));
	KT1_CHECK_EQ(b.w, 36.0f);                  // at least 18 px already: unchanged
}

KT1_CASE("ICOLEX-CLASSIFY-PAST", "past the switch: own, allied and seen enemies are full; an unseen enemy in radar is a contact; a ghost structure stays full") {
	const ViewFacts past{true, false};
	IconFields mobile = Land(), structure; structure.outline = Outline::Square; structure.canMove = false;
	KT1_CHECK(Classify(&mobile, {Relation::Self, true, true, false}, past) == Kind::Full);
	KT1_CHECK(Classify(&mobile, {Relation::Ally, false, false, false}, past) == Kind::Full);
	KT1_CHECK(Classify(&mobile, {Relation::Enemy, true, true, true}, past) == Kind::Full);
	KT1_CHECK(Classify(nullptr, {Relation::Enemy, false, true, false}, past) == Kind::Contact);
	KT1_CHECK(Classify(&mobile, {Relation::Enemy, false, true, true}, past) == Kind::Contact);
	KT1_CHECK(Classify(&structure, {Relation::Enemy, false, true, true}, past) == Kind::Full);
	KT1_CHECK(Classify(&structure, {Relation::Enemy, false, true, false}, past) == Kind::Contact);
}

KT1_CASE("ICOLEX-CLASSIFY-BELOW", "below the switch: only radar-only enemies that are not ghosts, none with the full view; false contacts never") {
	const ViewFacts below{false, false}, full{false, true};
	IconFields mobile = Land(), structure; structure.outline = Outline::Square; structure.canMove = false;
	IconFields blip = Land(); blip.falseContact = true;
	KT1_CHECK(Classify(&mobile, {Relation::Self, true, true, false}, below) == Kind::None);
	KT1_CHECK(Classify(&mobile, {Relation::Enemy, true, true, true}, below) == Kind::None);
	KT1_CHECK(Classify(nullptr, {Relation::Enemy, false, true, false}, below) == Kind::Contact);
	KT1_CHECK(Classify(&structure, {Relation::Enemy, false, true, true}, below) == Kind::None);
	KT1_CHECK(Classify(nullptr, {Relation::Enemy, false, true, false}, full) == Kind::None);
	KT1_CHECK(Classify(&blip, {Relation::Enemy, false, true, false}, below) == Kind::None);
	KT1_CHECK(Classify(&blip, {Relation::Enemy, true, true, true}, ViewFacts{true, false}) == Kind::None);
	KT1_CHECK(Classify(nullptr, {Relation::Enemy, false, false, false}, below) == Kind::None);
}

KT1_CASE("ICOLEX-CONTACT-SHAPE", "a contact of unknown type: triangle in the air, diamond elsewhere; known type: its own shape") {
	IconFields structure; structure.outline = Outline::Square;
	KT1_CHECK(ContactShape(nullptr, true) == Shape::Triangle);
	KT1_CHECK(ContactShape(nullptr, false) == Shape::Diamond);
	KT1_CHECK(ContactShape(&structure, true) == Shape::Square);
}

KT1_CASE("ICOLEX-FILL", "fill colour: a dark colour is unchanged, a light one is darkened along its hue until white keeps 3:1") {
	const IconLexiconConfig c = DefaultConfig();
	const auto dark = FillColour({0.1f, 0.2f, 0.4f}, c);
	KT1_CHECK(dark[0] == 0.1f && dark[1] == 0.2f && dark[2] == 0.4f);
	const auto light = FillColour({1, 1, 0.5f}, c);
	KT1_CHECK(light[0] < 1);
	KT1_CHECK(Near(light[0] / light[2], 2, 0.001f));   // same hue: each channel by the same factor
	const auto lin = [](float x) { return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f); };
	const float l = 0.2126f * lin(light[0]) + 0.7152f * lin(light[1]) + 0.0722f * lin(light[2]);
	KT1_CHECK(1.05f / (l + 0.05f) >= 3);
}
