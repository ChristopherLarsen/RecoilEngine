/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core tests: the minimap's unit layer (LUA-FREE D02): which units show, the silhouette's cell and size, contacts,
// the points and the draw order.

#include <string>
#include <vector>

#include "Game/KT1/Core/IconLexicon/IconMinimapState.h"
#include "Game/KT1/Test/KT1Test.h"

using namespace KT1::Core::IconLexicon;

namespace {

struct Map {
	IconConfig config;
	IconState icons;
	MinimapState state;
	std::vector<IconClass> classes;   // 0: tank, 1: commander, 2: fake, 3: no atlas cell
	std::vector<Paint> paints{{{0.1f, 0.2f, 0.3f}, false}, {{0.4f, 0.5f, 0.6f}, true}};
	MinimapView view{1000.0f, 2000.0f, 100.0f, 200.0f};

	explicit Map(int limit = 8) {
		for (const char* key: {"tank", "boss"}) {
			IconAtlasEntry e;
			e.key = key;
			e.l0[0] = 1.0f, e.l0[1] = 2.0f, e.l0[2] = 18.0f;
			e.l1[0] = 3.0f, e.l1[1] = 4.0f, e.l1[2] = 24.0f;
			config.atlas.icons.push_back(e);
		}
		for (const char* key: {"tank", "boss", "tank", "none"}) {
			IconClass c;
			c.key = key;
			c.command = std::string(key) == "boss";
			c.fake = classes.size() == 2;
			c.canMove = true;
			classes.push_back(c);
		}
		icons.Init(64, limit);
		state.Init(limit);
	}
	IconUnitFacts Own(int id, int def = 0) const {
		IconUnitFacts u;
		u.id = id;
		u.defId = def;
		u.unitSlot = id + 100;
		u.relation = Relation::Self;
		u.inLos = true;
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
	void Build(const std::vector<IconUnitFacts>& facts) { BuildMinimap(state, config, icons, facts, classes, paints, view); }
};

} // namespace

KT1_CASE("ICOMM-SILHOUETTE", "a unit is its atlas cell at 6 px, commanders at 11; team colour, inverted for the enemy") {
	Map m;
	m.Build({m.Own(1), m.Own(2, 1), m.Enemy(3, true, true)});
	KT1_CHECK_EQ(m.state.instances.size(), size_t(3));
	const MinimapInstance& tank = m.state.instances[0];
	KT1_CHECK(tank.cell[0] == 1.0f && tank.cell[1] == 2.0f && tank.cell[2] == 18.0f && tank.cell[3] == 18.0f);
	KT1_CHECK_EQ(tank.size, 18.0f * (6.0f / 10.0f));
	KT1_CHECK(tank.mode == static_cast<float>(MM_NORMAL) && tank.unitSlot == 101.0f && tank.at[3] == 0.0f);
	KT1_CHECK(tank.colour[0] == 0.1f && tank.colour[2] == 0.3f && tank.colour[3] == 1.0f);
	KT1_CHECK_EQ(m.state.instances[1].size, 18.0f * (11.0f / 10.0f));
	KT1_CHECK(m.state.instances[2].mode == static_cast<float>(MM_INVERTED));
}

KT1_CASE("ICOMM-CONTACT", "a radar-only enemy is the grey layer shape at its point; unseen, fake and cell-less units none") {
	Map m;
	IconUnitFacts ground = m.Enemy(1, false, true);
	ground.pos[0] = 250.0f, ground.pos[1] = 5.0f, ground.pos[2] = 500.0f;
	IconUnitFacts air = m.Enemy(2, false, true);
	air.pos[1] = 200.0f;
	m.Build({ground, air, m.Enemy(3, false, false), m.Own(4, 2), m.Own(5, 3)});
	KT1_CHECK_EQ(m.state.instances.size(), size_t(2));
	KT1_CHECK_EQ(m.state.contacts.size(), size_t(2));
	const MinimapInstance& c = m.state.instances[0];
	KT1_CHECK(c.mode == static_cast<float>(MM_CONTACT) && c.size == 6.0f);
	KT1_CHECK(c.cell[0] == static_cast<float>(Shape::Diamond) && c.cell[3] == 1.0f);
	KT1_CHECK(c.at[0] == 25.0f && c.at[1] == 150.0f && c.at[3] == 1.0f);
	KT1_CHECK(m.state.instances[1].cell[0] == static_cast<float>(Shape::Triangle));
}

KT1_CASE("ICOMM-ORDER", "creation order of the world set, ties by id; the limit keeps the oldest") {
	Map m(2);
	IconCreated(m.icons, 9);
	IconCreated(m.icons, 4);
	m.Build({m.Own(4), m.Own(9), m.Own(7)});   // 7 has no number: first
	KT1_CHECK_EQ(m.state.order.size(), size_t(2));
	KT1_CHECK_EQ(m.state.order[0], 7);
	KT1_CHECK_EQ(m.state.order[1], 9);
	m.Build({});
	KT1_CHECK(m.state.order.empty() && m.state.instances.empty() && m.state.contacts.empty());
}
