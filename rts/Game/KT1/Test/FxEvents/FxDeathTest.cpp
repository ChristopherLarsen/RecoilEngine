/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Engine-free tests of Core/FxEvents/FxDeath (LUA-FREE S30): each rule against gamedata/fx_death_logic.lua. The roster
// here is made up for the tests (the game's comes from its data file).

#include "../KT1Test.h"

#include "Game/KT1/Core/FxEvents/FxDeath.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace KT1::Core::Fx;

#define CHECK(c, msg) KT1_CHECK_MSG(c, msg)

namespace {
DeathConfig Roster() {
	DeathConfig c;
	c.bound_xz_factor = 0.5f;
	c.timed_threshold = 3.0f;
	c.selection = {
		{"land", 0.0f, 2.0f, "LA", "", 0.0f},
		{"land", 2.0f, -1.0f, "LB", "", 0.0f},
		{"water", 0.0f, 4.0f, "WA", "WX", 1.5f},
		{"air", 0.0f, -1.0f, "AA", "", 0.0f},
		{"sub", 0.0f, -1.0f, "SA", "", 0.0f},
	};
	c.sequence = {{"mobile", 1, 6}, {"structure", 2, 7}, {"air", 0, 0}, {"command", 9, 4}};
	c.damage_counts = {{0.0f, 1, 1.0f}, {12.0f, 4, 1.0f}, {30.0f, 6, 1.0f}};
	c.damage_states = {
		{"mobile", "1", 0.8f, "M1|M2"},
		{"mobile", "2", 0.4f, "M1+M3||M2"},
		{"structure", "1", 0.9f, "T1"},
		{"sub", "1", 0.6f, "U1"},
	};
	return c;
}

struct Draws {
	std::vector<std::pair<int32_t, int32_t>> calls;
	static int32_t Range(void* ctx, int32_t lo, int32_t hi) {
		static_cast<Draws*>(ctx)->calls.push_back({lo, hi});
		return hi;
	}
};

std::string Name(const DeathPlan& p, int32_t set) { return (set < 0) ? std::string("-") : p.sets[set]; }

// "wait timed base extra" of one death with sx = sz = legacy = size
std::string Parts(const DeathConfig& c, const DeathPlan& p, DeathKind kind, DeathLayer layer, float size, Draws& d) {
	const DeathParts r = PartsOfDeath(c, p, kind, layer, size, size, size, &Draws::Range, &d);
	return std::to_string(r.waitTicks) + (r.timed ? " timed " : " - ") + Name(p, r.base) + " " + Name(p, r.extra);
}
}

KT1_CASE("FXDEATH-RESOLVE", "set names resolved once, alternatives split, the highest threshold kept") {
	const DeathConfig c = Roster();
	DeathPlan p;
	CHECK(ResolveDeathPlan(c, p), "every row known");
	CHECK(p.selection.size() == 5 && p.selection[2].layer == DeathLayer::Water && p.selection[2].extraBelow == 1.5f, "selection");
	CHECK(p.states[0].size() == 2 && p.states[1].size() == 1 && p.states[2].size() == 1, "states by class");
	const auto& alts = p.states[0][1].alternatives;
	CHECK(alts.size() == 2 && alts[0].size() == 2 && alts[1].size() == 1, "empty pieces are skipped");
	CHECK(Name(p, alts[0][1]) == "M3" && alts[1][0] == p.states[0][0].alternatives[1][0], "one index per name");
	CHECK(Name(p, p.timedSet) == "H1" && Name(p, p.airKillSet) == "H2", "the two single sets");
	CHECK(p.topFraction == 0.9f, "highest threshold of any class");
	for (size_t i = 0; i < p.sets.size(); ++i)
		for (size_t j = i + 1; j < p.sets.size(); ++j)
			CHECK(p.sets[i] != p.sets[j], "no name twice");
}

KT1_CASE("FXDEATH-UNKNOWN", "a row with an unknown layer or class, or one state too many, is left out") {
	DeathConfig c = Roster();
	c.selection.push_back({"space", 0.0f, -1.0f, "ZZ", "", 0.0f});
	DeathPlan p;
	CHECK(!ResolveDeathPlan(c, p) && p.selection.size() == 5, "unknown layer");
	c = Roster();
	c.damage_states.push_back({"ghost", "1", 0.99f, "ZZ"});
	CHECK(!ResolveDeathPlan(c, p) && p.topFraction == 0.9f, "unknown class");
	c = Roster();
	for (int i = 0; i < DEATH_STATES; ++i)
		c.damage_states.push_back({"sub", "x", 0.1f, "U1"});
	CHECK(!ResolveDeathPlan(c, p) && static_cast<int>(p.states[2].size()) == DEATH_STATES, "no more states than the columns hold");
	CHECK(ResolveDeathPlan(Roster(), p) && p.selection.size() == 5 && p.states[2].size() == 1, "a plan is rebuilt from empty");
}

KT1_CASE("FXDEATH-LAYER", "the layer a unit died in, edges as the Lua's") {
	const DeathConfig c;
	CHECK(LayerOf(c, 116.0f, 100.0f) == DeathLayer::Land && LayerOf(c, 116.5f, 100.0f) == DeathLayer::Air, "two units up");
	CHECK(LayerOf(c, 16.0f, -40.0f) == DeathLayer::Water && LayerOf(c, 16.5f, -40.0f) == DeathLayer::Air, "over water");
	CHECK(LayerOf(c, -8.0f, -40.0f) == DeathLayer::Water && LayerOf(c, -8.5f, -40.0f) == DeathLayer::Sub, "one unit deep");
	CHECK(LayerOf(c, -20.0f, 0.0f) == DeathLayer::Land, "ground at sea level is land");
}

KT1_RATE_CASE("FXDEATH-PARTS", "one draw a death, then the sets by kind, layer and bound, in ticks at any rate") {
	const DeathConfig c = Roster();
	DeathPlan p;
	ResolveDeathPlan(c, p);
	Draws d;
	// bound = 0.5 x (sx + sz) = size
	CHECK(Parts(c, p, DeathKind::Mobile, DeathLayer::Land, 1.5f, d) == "6 - LA -", "under the edge");
	CHECK(Parts(c, p, DeathKind::Mobile, DeathLayer::Land, 2.0f, d) == "6 - LB -", "on the edge: the larger class");
	CHECK(Parts(c, p, DeathKind::Sea, DeathLayer::Water, 1.0f, d) == "6 - WA WX", "extra under its bound; mobile's wait");
	CHECK(Parts(c, p, DeathKind::Sea, DeathLayer::Water, 1.5f, d) == "6 - WA -", "no extra on its bound");
	CHECK(Parts(c, p, DeathKind::Sea, DeathLayer::Water, 7.0f, d) == "6 - - -", "no row");
	CHECK(Parts(c, p, DeathKind::Structure, DeathLayer::Land, 3.0f, d) == "7 timed LB -", "timed from the threshold");
	CHECK(Parts(c, p, DeathKind::Structure, DeathLayer::Land, 1.5f, d) == "7 - LA -", "under it");
	CHECK(Parts(c, p, DeathKind::Structure, DeathLayer::Sub, 5.0f, d) == "7 timed - -", "sub-surface: the timed run only");
	CHECK(Parts(c, p, DeathKind::Structure, DeathLayer::Air, 5.0f, d) == "7 - H2 -", "in the air: one hit, no timed run");
	CHECK(Parts(c, p, DeathKind::Air, DeathLayer::Air, 1.0f, d) == "0 - H2 -", "a kill in flight");
	CHECK(Parts(c, p, DeathKind::Command, DeathLayer::Land, 1.0f, d) == "9 - LA -", "an upper bound under the lower");
	CHECK(d.calls.size() == 11, "one draw each, drawn also for an empty wait");
	CHECK(d.calls[0].first == 1 && d.calls[0].second == 6 && d.calls[9].first == 0 && d.calls[9].second == 0
		&& d.calls[10].first == 9 && d.calls[10].second == 9, "the ranges drawn");
}

KT1_CASE("FXDEATH-STATES", "damage states at or under their share, by class") {
	DeathPlan p;
	ResolveDeathPlan(Roster(), p);
	CHECK(DamageClassOf(DeathKind::Structure) == 1 && DamageClassOf(DeathKind::Sub) == 2, "structure and sub");
	for (const DeathKind k: {DeathKind::Mobile, DeathKind::Air, DeathKind::Command, DeathKind::Sea})
		CHECK(DamageClassOf(k) == 0, "the rest are mobile");
	CHECK(ActiveStates(p, 0, 0.81f) == 0u && ActiveStates(p, 0, 0.8f) == 1u && ActiveStates(p, 0, 0.4f) == 3u, "mobile");
	CHECK(ActiveStates(p, 1, 0.85f) == 1u && ActiveStates(p, 2, 0.7f) == 0u, "other classes");
	CHECK(ActiveStates(p, -1, 0.1f) == 0u && ActiveStates(p, 3, 0.1f) == 0u, "no such class");
}

KT1_CASE("FXDEATH-EMITTERS", "emitters by volume: above a row's volume, not on it") {
	const DeathConfig c = Roster();
	CHECK(DamageEmitters(c, 0.0f) == 1 && DamageEmitters(c, 12.0f) == 1, "base row");
	CHECK(DamageEmitters(c, 12.5f) == 4 && DamageEmitters(c, 30.0f) == 4 && DamageEmitters(c, 31.0f) == 6, "larger rows");
	CHECK(DamageEmitters(DeathConfig{}, 100.0f) == 1, "empty roster");
}
