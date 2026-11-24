/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Plants (version 1). The contact search of the two plant gadgets: which moving ground unit touches which knockable
// plant (feature def customParams prop_class knockable_single or knockable_cluster). The gadgets keep what a contact does.
//
// Replaces:   the per-unit search of LuaRules/Gadgets/mov_trees.lua (MV-5: a tree knocked over) and LuaRules/Gadgets/map_plants.lua
//             (MAP-002 v5: a plant falls, a cluster breaks); both stay and pull Events() when this is on
// Switch:     mod option kt1_plants (default off)
// Config:     gamedata/kt1/plants.lua
//               trees_every       frames between the checks of one unit (units spread by id)       (DESIGN 3)
//               trees_reach       share of the footprint half-width at which a tree is reached     (DESIGN 0.6)
//               trees_min_speed   speed (elmos per frame) above which a unit knocks trees          (DESIGN 0.01)
//               plants_every      frames between the checks of one unit                            (DESIGN 3)
//               plants_margin     elmos added to the footprint half-width for contact              (PLACEHOLDER 4)
//               plants_min_speed  horizontal speed above which a unit touches plants               (DESIGN 0.05)
// Unit data:  none: footprint, speed and kind from the unit def, as in the Lua
// Timer:      KT1::Plants
// Lua:        Spring.KT1.Plants.Events(kind) -> { unitID, featureID, ... }  the contacts of one probe since its last pull (kind 1:
//                                         the tree probe, mov_trees; 2: the plant probe, map_plants), then cleared (synced only)
//             Spring.KT1.Plants.Config() -> the numbers in use
// Rule, each frame f, over unit ids in ascending order:
//   trees:  a unit whose def is not a flyer and has speed > 0, id % trees_every == f % trees_every, speed above trees_min_speed,
//           finished (build progress 1), at or above the ground - 1: each knockable feature within max(xsize, zsize) * 4 * trees_reach
//   plants: a unit whose def can move, is not a flyer and not a building, id % plants_every == f % plants_every, horizontal speed at
//           least plants_min_speed: each knockable feature within max(xsize, zsize) * 4 + plants_margin
//   The features of one unit in ascending feature id (the Lua took the quad field order).
//   Units in ascending id (the Lua walked its own list, in swap-remove order): two units reaching one tree in the same check
//   frame: the lower id knocks it over.
// Determinism: ids ascending, features sorted by id, float maths on synced state in one fixed order; no thread, clock or hash order.
// Doc:        game-build docs/engine/rules/plants.md

#include <algorithm>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/Ground.h"
#include "Sim/Features/Feature.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

class Plants : public Rule {
public:
	Plants() : Rule({"Plants", 1, true, "LuaRules/Gadgets/mov_trees.lua, map_plants.lua (contact search)", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		treesEvery = std::max(1, cfg.GetInt("trees_every", 3));
		treesReach = cfg.GetFloat("trees_reach", 0.6f);
		treesMin = cfg.GetFloat("trees_min_speed", 0.01f);
		plantsEvery = std::max(1, cfg.GetInt("plants_every", 3));
		plantsMargin = cfg.GetFloat("plants_margin", 4.0f);
		plantsMin = cfg.GetFloat("plants_min_speed", 0.05f);
	}

	void Start() override {
		const int nf = (int)featureDefHandler->NumFeatureDefs();
		knock.assign(nf + 1, 0);
		for (int d = 1; d <= nf; ++d) {
			const FeatureDef* fd = featureDefHandler->GetFeatureDefByID(d);
			if (fd == nullptr) continue;
			const auto it = fd->customParams.find("prop_class");
			if (it != fd->customParams.end() && (it->second == "knockable_single" || it->second == "knockable_cluster")) knock[d] = 1;
		}
		const int n = unitDefHandler->NumUnitDefs();
		treeR.assign(n + 1, -1.0f);
		plantR.assign(n + 1, -1.0f);
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			if (ud == nullptr) continue;
			const float half = float(std::max(ud->xsize, ud->zsize)) * 4.0f;
			if (!ud->canfly && ud->speed > 0.0f) treeR[d] = half * treesReach;
			if (ud->canmove && !ud->canfly && !ud->IsBuildingUnit()) plantR[d] = half + plantsMargin;
		}
	}

	void GameFrame(int frame) override {
		const unsigned int maxU = unitHandler.MaxUnits();
		const int ts = frame % treesEvery, ps = frame % plantsEvery;
		for (unsigned int id = 0; id < maxU; ++id) {
			const bool tSlot = (int)(id % (unsigned)treesEvery) == ts, pSlot = (int)(id % (unsigned)plantsEvery) == ps;
			if (!tSlot && !pSlot) continue;
			const CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) continue;
			const int d = u->unitDef->id;
			if (d < 0 || d >= (int)treeR.size()) continue;
			if (tSlot && treeR[d] >= 0.0f && u->speed.w > treesMin && u->buildProgress >= 1.0f
				&& u->pos.y >= CGround::GetHeightReal(u->pos.x, u->pos.z, true) - 1.0f)
				Probe(u, treeR[d], 1);
			if (pSlot && plantR[d] >= 0.0f && u->speed.x * u->speed.x + u->speed.z * u->speed.z >= plantsMin * plantsMin)
				Probe(u, plantR[d], 2);
		}
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Events", LEvents);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static constexpr size_t MAX_EVENTS = 2 * 8192;   // DESIGN: contacts held between pulls; more wait for the unit's next check

	void Probe(const CUnit* u, float r, int kind) {
		QuadFieldQuery q;
		q.threadOwner = 0;
		quadField.GetFeaturesExact(q, float3(u->pos.x, 0.0f, u->pos.z), r, false);
		hits.clear();
		for (const CFeature* f : *q.features) {
			const int fd = f->def->id;
			if (fd > 0 && fd < (int)knock.size() && knock[fd]) hits.push_back(f->id);
		}
		std::sort(hits.begin(), hits.end());
		std::vector<int>& ev = events[kind - 1];
		for (int fid : hits) {
			if (ev.size() + 2 > MAX_EVENTS) return;
			ev.push_back(u->id);
			ev.push_back(fid);
		}
	}

	static Plants* Self() { return static_cast<Plants*>(Registry::Get().Find("Plants")); }

	static int LEvents(lua_State* L) {
		Plants* r = Self();
		const int kind = luaL_checkint(L, 1);
		if (r == nullptr || !r->Enabled() || !CLuaHandle::GetHandleSynced(L) || kind < 1 || kind > 2) return 0;
		std::vector<int>& ev = r->events[kind - 1];
		lua_createtable(L, (int)ev.size(), 0);
		for (size_t i = 0; i < ev.size(); ++i) { lua_pushnumber(L, ev[i]); lua_rawseti(L, -2, (int)i + 1); }
		ev.clear();
		return 1;
	}
	static int LConfig(lua_State* L) {
		const Plants* r = Self();
		if (r == nullptr) return 0;
		lua_newtable(L);
		LuaPushNamedNumber(L, "trees_every", r->treesEvery);
		LuaPushNamedNumber(L, "trees_reach", r->treesReach);
		LuaPushNamedNumber(L, "trees_min_speed", r->treesMin);
		LuaPushNamedNumber(L, "plants_every", r->plantsEvery);
		LuaPushNamedNumber(L, "plants_margin", r->plantsMargin);
		LuaPushNamedNumber(L, "plants_min_speed", r->plantsMin);
		return 1;
	}

	int treesEvery = 3, plantsEvery = 3;
	float treesReach = 0.6f, treesMin = 0.01f, plantsMargin = 4.0f, plantsMin = 0.05f;
	std::vector<char> knock;              // per feature def id
	std::vector<float> treeR, plantR;     // per unit def id: probe radius, -1 = not probed
	std::vector<int> hits;                // scratch
	std::vector<int> events[2];           // per probe: (unit, feature) pairs since the last pull
};

} // namespace

std::unique_ptr<Rule> MakePlants() { return std::make_unique<Plants>(); }

} // namespace KT1
