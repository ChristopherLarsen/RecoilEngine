/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core tests: the icon instance set (LUA-FREE D01): what a unit shows, the instance's fields, draw order, in-place
// writes against rebuilds, the hit flicker and the hit tests.

#include <string>
#include <vector>

#include "Game/KT1/Core/IconLexicon/IconState.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::IconLexicon;

namespace {

struct World {
	IconConfig config;
	IconParams params;
	IconState state;
	std::vector<IconClass> classes;   // 0: tank, 1: structure, 2: wall, 3: fake
	std::vector<Paint> paints{{{0.1f, 0.2f, 0.3f}, false}, {{0.4f, 0.5f, 0.6f}, false}};

	explicit World(int limit = 8) {
		for (const char* outline: {"land", "square", "wall", "land"}) {
			IconDefFacts d;
			d.outline = outline;
			d.canMove = std::string(outline) == "land";
			d.fake = classes.size() == 3;
			classes.push_back(ClassFor(d, config));
		}
		params.pastSwitch = true;
		state.Init(64, limit);
	}
	IconUnitFacts Own(int id, int def = 0) const {
		IconUnitFacts u;
		u.id = id;
		u.defId = def;
		u.unitSlot = id + 100;
		u.relation = Relation::Self;
		u.inLos = true;
		u.health = u.maxHealth = 10.0f;
		return u;
	}
	IconUnitFacts Enemy(int id, bool inLos, bool inRadar, int def = 0) const {
		IconUnitFacts u = Own(id, def);
		u.relation = Relation::Enemy;
		u.team = 1;
		u.inLos = inLos;
		u.inRadar = inRadar;
		return u;
	}
	void Rebuild(const std::vector<IconUnitFacts>& facts) { RebuildIcons(state, config, params, facts, classes, paints); }
	bool Apply(const IconUnitFacts& u) {
		return ApplyIcon(state, config, params, u, &classes[u.defId], paints[u.team]);
	}
	const IconInstance& At(int place) const { return state.instances[place]; }
};

} // namespace

KT1_CASE("ICOSET-KIND", "past the switch every known unit; below it radar-only enemies alone; a known fake never") {
	World w;
	const IconClass* tank = &w.classes[0];
	const IconClass* building = &w.classes[1];
	KT1_CHECK(KindOf(tank, w.Own(1), true) == IconKind::Full);
	KT1_CHECK(KindOf(tank, w.Own(1), false) == IconKind::None);
	KT1_CHECK(KindOf(tank, w.Enemy(2, true, true), true) == IconKind::Full);
	KT1_CHECK(KindOf(tank, w.Enemy(2, true, true), false) == IconKind::None);
	KT1_CHECK(KindOf(tank, w.Enemy(2, false, true), true) == IconKind::Contact);
	KT1_CHECK(KindOf(tank, w.Enemy(2, false, true), false) == IconKind::Contact);
	KT1_CHECK(KindOf(tank, w.Enemy(2, false, false), true) == IconKind::None);
	KT1_CHECK(KindOf(nullptr, w.Enemy(2, false, true), true) == IconKind::Contact);
	IconUnitFacts ghost = w.Enemy(3, false, false, 1);
	ghost.seenBefore = true;
	KT1_CHECK(KindOf(building, ghost, true) == IconKind::Full);
	KT1_CHECK(KindOf(building, ghost, false) == IconKind::None);
	KT1_CHECK(KindOf(&w.classes[3], w.Own(4, 3), true) == IconKind::None);
	IconUnitFacts full = w.Enemy(5, false, true);
	full.fullView = true;
	KT1_CHECK(KindOf(tank, full, true) == IconKind::Full);
	KT1_CHECK(KindOf(tank, full, false) == IconKind::None);
}

KT1_CASE("ICOSET-LAYER", "a contact's layer from its height over ground and water") {
	const IconViewConfig view;
	KT1_CHECK(LayerOf(100.0f, 20.0f, view) == ContactLayer::Air);
	KT1_CHECK(LayerOf(30.0f, 20.0f, view) == ContactLayer::Land);
	KT1_CHECK(LayerOf(45.0f, -100.0f, view) == ContactLayer::Air);
	KT1_CHECK(LayerOf(0.0f, -100.0f, view) == ContactLayer::Naval);
	KT1_CHECK(LayerOf(-30.0f, -100.0f, view) == ContactLayer::Sub);
}

KT1_CASE("ICOSET-INSTANCE", "box, shape, tier bars, flags, colour, cloak alpha; a contact grey at its position") {
	World w;
	IconUnitFacts own = w.Own(7);
	own.selected = true;
	own.cloaked = true;
	w.params.hovered = 7;
	IconInstance out;
	KT1_CHECK(MakeInstance(w.config, w.params, &w.classes[0], own, w.paints[0], IconKind::Full, out));
	KT1_CHECK_EQ(out.width, 18.0f);
	KT1_CHECK_EQ(out.lift, 2.5f);
	KT1_CHECK_EQ(out.unitSlot, 107.0f);
	KT1_CHECK_EQ(out.bars, 1.0f);
	KT1_CHECK_EQ(out.flags, 3.0f);
	KT1_CHECK_EQ(out.colour[1], 0.2f);
	KT1_CHECK_EQ(out.colour[3], 0.35f);
	KT1_CHECK_EQ(out.world[3], 0.0f);
	IconUnitFacts blip = w.Enemy(8, false, true);
	blip.selected = true;
	blip.cloaked = true;
	blip.pos[0] = 5.0f;
	blip.pos[1] = 200.0f;
	KT1_CHECK(MakeInstance(w.config, w.params, nullptr, blip, w.paints[1], IconKind::Contact, out));
	KT1_CHECK_EQ(out.shape, static_cast<float>(Shape::Triangle));
	KT1_CHECK_EQ(out.bars, 0.0f);
	KT1_CHECK_EQ(out.flags, 0.0f);
	KT1_CHECK_EQ(out.colour[0], 128.0f / 255.0f);
	KT1_CHECK_EQ(out.colour[3], 1.0f);
	KT1_CHECK_EQ(out.world[0], 5.0f);
	KT1_CHECK_EQ(out.world[3], 1.0f);
	KT1_CHECK(!MakeInstance(w.config, w.params, nullptr, w.Own(9), w.paints[0], IconKind::Full, out));
}

KT1_CASE("ICOSET-WALL-ALT", "a small wall hides unless Alt, which enlarges small icons") {
	World w;
	IconInstance out;
	KT1_CHECK(!MakeInstance(w.config, w.params, &w.classes[2], w.Own(1, 2), w.paints[0], IconKind::Full, out));
	w.params.alt = true;
	KT1_CHECK(MakeInstance(w.config, w.params, &w.classes[2], w.Own(1, 2), w.paints[0], IconKind::Full, out));
	KT1_CHECK_EQ(out.width, 20.0f);
	KT1_CHECK(MakeInstance(w.config, w.params, &w.classes[0], w.Own(2), w.paints[0], IconKind::Full, out));
	KT1_CHECK_EQ(out.width, 18.0f);
}

KT1_CASE("ICOSET-ORDER", "creation order, later on top, ties by id; the limit keeps the oldest") {
	World w(3);
	IconCreated(w.state, 9);
	IconCreated(w.state, 4);
	w.Rebuild({w.Own(4), w.Own(9), w.Own(2), w.Own(1)});
	KT1_CHECK(w.state.order == (std::vector<int32_t>{1, 2, 9}));
	KT1_CHECK_EQ(w.state.instances.size(), size_t(3));
	KT1_CHECK_EQ(w.At(2).unitSlot, 109.0f);
	KT1_CHECK_EQ(w.state.dirty.size(), size_t(1));
	KT1_CHECK(!w.state.rebuild);
	KT1_CHECK(w.state.pending.empty());
}

KT1_CASE("ICOSET-APPLY", "a changed unit is rewritten in place; the newest is appended; others force a rebuild") {
	World w;
	IconCreated(w.state, 1);
	IconSeen(w.state, 2);
	w.Rebuild({w.Own(1), w.Enemy(2, true, true)});
	w.state.dirty.clear();
	KT1_CHECK(w.Apply(w.Enemy(2, false, true)));
	KT1_CHECK(w.state.contacts == (std::vector<int32_t>{1}));
	KT1_CHECK_EQ(w.At(1).world[3], 1.0f);
	KT1_CHECK_EQ(w.state.dirty[0].first, 1);
	KT1_CHECK(w.Apply(w.Enemy(2, false, false)));     // lost: hidden, keeps its place
	KT1_CHECK_EQ(w.At(1).width, 0.0f);
	KT1_CHECK(w.state.contacts.empty());
	KT1_CHECK(w.Apply(w.Enemy(2, true, true)));
	KT1_CHECK_EQ(w.At(1).width, 18.0f);
	IconCreated(w.state, 3);
	KT1_CHECK(w.Apply(w.Own(3)));
	KT1_CHECK(w.state.order == (std::vector<int32_t>{1, 2, 3}));
	KT1_CHECK(!w.Apply(w.Own(4)));                    // no creation number: it belongs first
	KT1_CHECK(w.state.rebuild);
}

KT1_CASE("ICOSET-GONE", "a dead unit's instance leaves at once and its id can come back as the newest") {
	World w;
	IconCreated(w.state, 1);
	IconCreated(w.state, 2);
	w.Rebuild({w.Own(1), w.Enemy(2, false, true)});
	IconMark(w.state, 2);
	IconMark(w.state, 2);
	KT1_CHECK_EQ(w.state.pending.size(), size_t(1));
	IconGone(w.state, 2);
	KT1_CHECK_EQ(w.At(1).width, 0.0f);
	KT1_CHECK(w.state.contacts.empty() && w.state.pending.empty());
	IconCreated(w.state, 2);
	KT1_CHECK(w.Apply(w.Own(2)));
	KT1_CHECK(w.state.order == (std::vector<int32_t>{1, -1, 2}));
	IconGone(w.state, 40);                            // unknown: nothing
}

KT1_CASE("ICOSET-FLICKER", "a hit shows red for its window in draw seconds, over rewrites, then the own colour") {
	World w;
	IconCreated(w.state, 1);
	w.Rebuild({w.Own(1)});
	w.state.dirty.clear();
	FlickerStart(w.state, w.config.view, 1, 2.0f);
	FlickerStep(w.state, w.config.view, 2.0f);
	KT1_CHECK_EQ(w.At(0).colour[0], 1.0f);
	KT1_CHECK_EQ(w.At(0).colour[3], 1.0f);
	KT1_CHECK_EQ(w.state.dirty.size(), size_t(1));
	w.Rebuild({w.Own(1)});
	KT1_CHECK_EQ(w.At(0).colour[0], 1.0f);
	FlickerStep(w.state, w.config.view, 2.1f);
	KT1_CHECK_EQ(w.At(0).colour[0], 1.0f);
	FlickerStep(w.state, w.config.view, 2.2f);
	KT1_CHECK_EQ(w.At(0).colour[0], 0.1f);
	KT1_CHECK(w.state.lit.empty());
	FlickerStart(w.state, w.config.view, 1, 3.0f);
	IconGone(w.state, 1);
	KT1_CHECK(w.state.lit.empty());
}

KT1_CASE("ICOSET-STEADY", "no allocation after the first frames") {
	World w;
	std::vector<IconUnitFacts> facts{w.Own(1), w.Enemy(2, false, true), w.Own(3)};
	for (const IconUnitFacts& u: facts) IconCreated(w.state, u.id);
	w.Rebuild(facts);
	const long long before = KT1::Test::AllocationCount();
	for (int i = 0; i < 50; ++i) {
		w.Rebuild(facts);
		IconMark(w.state, 2);
		w.Apply(facts[1]);
		FlickerStart(w.state, w.config.view, 1, static_cast<float>(i));
		FlickerStep(w.state, w.config.view, static_cast<float>(i) + 1.0f);
	}
	KT1_CHECK_EQ(KT1::Test::AllocationCount(), before);
}

KT1_CASE("ICOSET-PAINT", "team fill darkened until white keeps its contrast; relation colours by mode") {
	const IconConfig config;
	const Rgb dark = FillColour({0.0f, 0.0f, 0.4f}, config.style);
	KT1_CHECK_EQ(dark.b, 0.4f);
	const Rgb yellow = FillColour({1.0f, 1.0f, 0.0f}, config.style);
	KT1_CHECK(yellow.r < 0.75f && yellow.r > 0.5f && yellow.r == yellow.g && yellow.b == 0.0f);
	const Rgb team{0.2f, 0.3f, 0.4f};
	KT1_CHECK_EQ(PaintFor(config, RelationMode::Off, false, Relation::Enemy, team).colour.r, team.r);
	KT1_CHECK_EQ(PaintFor(config, RelationMode::Minimap, false, Relation::Self, team).colour.r, team.r);
	KT1_CHECK_EQ(PaintFor(config, RelationMode::Minimap, true, Relation::Self, team).colour.r, 47.0f / 255.0f);
	const Paint enemy = PaintFor(config, RelationMode::Everywhere, false, Relation::Enemy, team);
	KT1_CHECK(enemy.inverted && enemy.colour.r == 1.0f);
}

KT1_CASE("ICOSET-HIT", "the topmost box under the point wins; a double click needs type, time and distance") {
	const std::vector<IconRect> rects{{1, 10.0f, 10.0f, 8.0f, 8.0f}, {2, 14.0f, 10.0f, 8.0f, 8.0f}};
	KT1_CHECK_EQ(HitRect(rects, 12.0f, 10.0f), 2);
	KT1_CHECK_EQ(HitRect(rects, 6.0f, 14.0f), 1);
	KT1_CHECK_EQ(HitRect(rects, 30.0f, 10.0f), -1);
	KT1_CHECK(IsDoubleClick(0.2f, 6.0f, -6.0f, true, 0.2f, 6.0f));
	KT1_CHECK(!IsDoubleClick(0.3f, 0.0f, 0.0f, true, 0.2f, 6.0f));
	KT1_CHECK(!IsDoubleClick(0.1f, 7.0f, 0.0f, true, 0.2f, 6.0f));
	KT1_CHECK(!IsDoubleClick(0.1f, 0.0f, 0.0f, false, 0.2f, 6.0f));
}
