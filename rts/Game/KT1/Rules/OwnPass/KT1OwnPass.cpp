/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: OwnPass (version 1). A heavy own unit drives through a standing light own unit without pushing it (mov_own_pass.lua;
// WINDOW-2026-11-12-A, WINDOW-2026-11-12-C). The engine does the per-frame walk and the light-unit search; Lua keeps the
// hold (MoveCtrl, blocking flags) and the release (apart test, order replay) of the few open pairs.
//
// Replaces:   LuaRules/Gadgets/mov_own_pass.lua (the walk and search only: the gadget stays and pulls Events when this is on)
// Switch:     mod option kt1_ownpass (default off)
// Config:     gamedata/kt1/ownpass.lua
//               slice          frames between the speed reads of one unit (DESIGN 3)
//               ahead          elmos beyond the two sizes at which the light unit stops blocking (DESIGN 24)
//               search_margin  elmos added to the search radius for the heavy's cylinder (DESIGN 64)
//               idle_speed     speed (elmos per frame) below which a unit counts as standing (DESIGN 0.05)
// Unit data:  none (mass, radius and footprint come from the unit def, as in the Lua)
// Timer:      KT1::OwnPass
// Lua:        Spring.KT1.OwnPass.Events()      -> { light, heavy, ... }   pairs opened since the last pull, then cleared (synced only)
//             Spring.KT1.OwnPass.Closed(light) the Lua released the light unit: the rule may open it again (synced only)
//             Spring.KT1.OwnPass.Config()      -> { slice, ahead, search_margin, idle_speed }
// Rule, each frame f, over unit ids in ascending order (the Lua walked in order of appearing; ids are reused, so a tie between two
// heavy candidates goes to the lower id, not the earlier arrival):
//   1. every ground mover (canmove, not canfly) whose id % slice == f % slice re-reads whether it moves (speed above idle_speed);
//      the last read stays between. Moving units set the heaviest mass per team and the largest size.
//   2. a ground mover on its slice frame that is not open, not moving, lighter than its team's heaviest mover and standing
//      (speed below idle_speed, empty command queue) looks for a moving unit of its team, heavier, within the heavy's size + ahead
//      + search_margin and within the two sizes + ahead of it; the lowest id found opens the pair: an event (light, heavy).
// Determinism: the walk is over ids 0..MaxUnits in order; float maths on synced state (position, speed, def mass and size) in one fixed
// expression order; the search result is the lowest id, so the order the quad field returns units in never reaches a result;
// no thread, clock or hash-ordered container is read; per-team maxima are order-free.
// Events:     pairs, in light-id order; the gadget pulls them in its GameFrame (after this rule), as the Lua did in the same frame.
// Doc:        game-build docs/engine/rules/ownpass.md

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1UnitData.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Rendering/Models/3DModel.hpp"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

class OwnPass : public Rule {
public:
	OwnPass() : Rule({"OwnPass", 1, true, "LuaRules/Gadgets/mov_own_pass.lua", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		slice = std::max(1, cfg.GetInt("slice", 3));
		ahead = cfg.GetFloat("ahead", 24.0f);
		margin = cfg.GetFloat("search_margin", 64.0f);
		idleSpeed = cfg.GetFloat("idle_speed", 0.05f);
	}

	void Start() override {
		const int n = unitDefHandler->NumUnitDefs();
		ground.assign(n + 1, 0);
		mass.assign(n + 1, 0.0f);
		size.assign(n + 1, 0.0f);
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			if (ud == nullptr || !ud->canmove || ud->canfly) continue;
			ground[d] = 1;
			mass[d] = ud->mass;
			const float foot = std::sqrt(float(ud->xsize) * float(ud->xsize) + float(ud->zsize) * float(ud->zsize)) * 4.0f;
			const S3DModel* model = ud->LoadModel();   // the Lua unit def radius is the model radius
			size[d] = std::max(model != nullptr ? model->radius : 0.0f, foot);   // the engine collides by footprint as well as by radius
		}
	}

	void GameFrame(int frame) override {
		const unsigned int maxU = unitHandler.MaxUnits();
		if (owner.size() < maxU) {
			owner.resize(maxU, nullptr);
			moveRead.resize(maxU, 0);
			isOpen.resize(maxU, 0);
			movingAt.resize(maxU, -1);
		}
		const int nt = teamHandler.ActiveTeams();
		heaviest.assign(nt, -1.0f);
		float maxSize = 0.0f;
		const int slot = frame % slice;

		// pass 1: moving flags, heaviest mover per team, largest mover size
		for (unsigned int id = 0; id < maxU; ++id) {
			CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) { owner[id] = nullptr; continue; }
			if (owner[id] != u) { owner[id] = u; moveRead[id] = 0; isOpen[id] = 0; movingAt[id] = -1; }
			const int d = u->unitDef->id;
			if (d < 0 || d >= (int)ground.size() || !ground[d]) continue;
			if ((int)(id % (unsigned)slice) == slot)
				moveRead[id] = (u->speed.w > idleSpeed) ? 1 : 0;
			if (!moveRead[id]) continue;
			movingAt[id] = frame;
			const int t = u->team;
			if (t >= 0 && t < nt && mass[d] > heaviest[t]) heaviest[t] = mass[d];
			maxSize = std::max(maxSize, size[d]);
		}

		// pass 2: standing light units on their slice frame look for a heavier moving unit of their team
		const float radius = maxSize + ahead + margin;
		for (unsigned int id = 0; id < maxU; ++id) {
			CUnit* light = owner[id];
			if (light == nullptr || (int)(id % (unsigned)slice) != slot) continue;
			const int d = light->unitDef->id;
			if (d < 0 || d >= (int)ground.size() || !ground[d]) continue;
			if (isOpen[id] || movingAt[id] == frame) continue;
			const int t = light->team;
			if (t < 0 || t >= nt || heaviest[t] < 0.0f || !(mass[d] < heaviest[t])) continue;
			if (!(light->speed.w < idleSpeed)) continue;
			if (light->commandAI != nullptr && !light->commandAI->commandQue.empty()) continue;

			QuadFieldQuery q;
			q.threadOwner = 0;
			quadField.GetUnitsExact(q, light->pos, radius, false);
			const float ls = size[d];
			int first = -1;
			for (CUnit* h : *q.units) {
				if (h->team != t || h->id == light->id || movingAt[h->id] != frame) continue;
				if (first >= 0 && (int)h->id > first) continue;
				const int hd = h->unitDef->id;
				if (hd < 0 || hd >= (int)ground.size() || !ground[hd] || !(mass[hd] > mass[d])) continue;
				const float r = size[hd];
				const float dx = light->pos.x - h->pos.x, dz = light->pos.z - h->pos.z;
				const float dd = dx * dx + dz * dz;
				const float rr = r + ls + ahead;
				const float rh = r + ahead + margin;
				if (dd <= rh * rh && dd <= rr * rr) first = (int)h->id;
			}
			if (first >= 0 && events.size() < MAX_EVENTS) {
				isOpen[id] = 1;
				events.push_back((int)id);
				events.push_back(first);
			}
		}
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Events", LEvents);
		LuaPushRawNamedCFunc(L, "Closed", LClosed);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static constexpr size_t MAX_EVENTS = 2 * 4096;   // DESIGN: pairs queued beyond this wait: the light stays unopened until a pull makes room

	static OwnPass* Self() { return static_cast<OwnPass*>(Registry::Get().Find("OwnPass")); }

	static int LEvents(lua_State* L) {
		OwnPass* r = Self();
		if (r == nullptr || !r->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		lua_createtable(L, (int)r->events.size(), 0);
		for (size_t i = 0; i < r->events.size(); ++i) { lua_pushnumber(L, r->events[i]); lua_rawseti(L, -2, (int)i + 1); }
		r->events.clear();
		return 1;
	}
	static int LClosed(lua_State* L) {
		OwnPass* r = Self();
		if (r == nullptr || !r->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		const int id = luaL_checkint(L, 1);
		if (id >= 0 && id < (int)r->isOpen.size()) r->isOpen[id] = 0;
		return 0;
	}
	static int LConfig(lua_State* L) {
		const OwnPass* r = Self();
		if (r == nullptr) return 0;
		lua_newtable(L);
		lua_pushnumber(L, r->slice);     lua_setfield(L, -2, "slice");
		lua_pushnumber(L, r->ahead);     lua_setfield(L, -2, "ahead");
		lua_pushnumber(L, r->margin);    lua_setfield(L, -2, "search_margin");
		lua_pushnumber(L, r->idleSpeed); lua_setfield(L, -2, "idle_speed");
		return 1;
	}

	int slice = 3;
	float ahead = 24.0f, margin = 64.0f, idleSpeed = 0.05f;
	std::vector<char> ground;             // per def id
	std::vector<float> mass, size;        // per def id
	std::vector<CUnit*> owner;            // per unit id: who the state below belongs to (an id is reused)
	std::vector<char> moveRead, isOpen;   // per unit id
	std::vector<int> movingAt;            // per unit id: the last frame the unit counted as moving
	std::vector<float> heaviest;          // per team, this frame
	std::vector<int> events;              // (light, heavy) pairs since the last pull
};

} // namespace

std::unique_ptr<Rule> MakeOwnPass() { return std::make_unique<OwnPass>(); }

} // namespace KT1
