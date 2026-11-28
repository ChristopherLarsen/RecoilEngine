/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Census (version 1). The framework's demo rule: it proves data, config, API, switch and timer end to end.
//
// Replaces:   LuaRules/Gadgets/kt1_census.lua (same numbers; it runs when this rule is off)
// Switch:     mod option kt1_census (default off)
// Config:     gamedata/kt1/census.lua or, with kt1_jsonconfig, data/kt1/census.json (CensusConfig.h)
//               period_s          seconds between tallies (DESIGN 1)
//               default_weight    a unit's weight when its def sets none (DESIGN 1)
//               count_unfinished  count units still being built (DESIGN false)
// Unit data:  kt1 = { census = { weight = <integer> } }   (customParams kt1_census_weight); count = false leaves the def out
// Timer:      KT1::Census
// Lua:        Spring.KT1.Census.Get(team)  -> units, weight, frame     the last tally (any handle)
//             Spring.KT1.Census.Events()   -> { team, old, new, ... }  weight changes since the last call, then cleared (synced only)
//             Spring.KT1.Census.Config()   -> { period, default_weight, count_unfinished }
// Rule:       every `period` frames (frame % period == 0), for each team: units = finished units (and unfinished when
//             count_unfinished), weight = sum of their weights. A team whose weight changed queues one event.
// Determinism: integer sums (order-free); teams in id order; events in team order. No float state.
// Low-rate call-in pattern: the rule never calls Lua. It queues events; the gadget pulls them (Events) in its own GameFrame,
// at the rule's rate. No new call-in, no per-unit Lua call, and the order is the engine's.

#include <algorithm>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Rules/Census/CensusConfig.h"
#include "Game/KT1/KT1UnitData.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Game/KT1/KT1Config.h"
#include "Lua/LuaUtils.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

class Census : public Rule {
public:
	Census() : Rule({"Census", 1, true, "LuaRules/Gadgets/kt1_census.lua", false}) {}

	void LoadConfig(const LuaTable& t) override {
		period = KT1::ConfigFrames(t, "period_s", "period", 30);   // KT1 lint: allow frame-literal the default of the old key, frames at 30 Hz, which KT1::ConfigFrames converts
		defaultWeight = t.GetInt("default_weight", 1);
		countUnfinished = t.GetBool("count_unfinished", false);
	}

	void ConfigValues(Data::Values& v) const override {
		v.Add("period", period);
		v.Add("default_weight", defaultWeight);
		v.Add("count_unfinished", countUnfinished);
	}

	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		const Data::Report report = Data::ParseInto(text, strictness, cfg);
		period = Core::Rate::Every(cfg.periodS);
		defaultWeight = cfg.defaultWeight;
		countUnfinished = cfg.countUnfinished;
		return report;
	}

	void Start() override {
		const int n = unitDefHandler->NumUnitDefs();
		weight.assign(n + 1, 0);
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			weight[d] = DefBool(ud, Key(), "count", true) ? (int)DefNum(ud, Key(), "weight", (float)defaultWeight) : 0;
		}
	}

	void GameFrame(int frame) override {
		if (frame % period != 0) return;
		const int nt = teamHandler.ActiveTeams();
		units.resize(nt, 0);
		sums.resize(nt, 0);
		for (int t = 0; t < nt; ++t) {
			int nu = 0, w = 0;
			for (const CUnit* u : unitHandler.GetUnitsByTeam(t)) {
				if (u->beingBuilt && !countUnfinished) continue;
				const int d = u->unitDef->id;
				nu += 1;
				w += (d >= 0 && d < (int)weight.size()) ? weight[d] : defaultWeight;
			}
			if (w != sums[t] && events.size() < MAX_EVENTS) { events.push_back(t); events.push_back(sums[t]); events.push_back(w); }
			units[t] = nu;
			sums[t] = w;
		}
		lastFrame = frame;
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Get", LGet);
		LuaPushRawNamedCFunc(L, "Events", LEvents);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static constexpr size_t MAX_EVENTS = 3 * 1024;   // DESIGN: drop beyond this many queued (team, old, new) until pulled

	static Census* Self() { return static_cast<Census*>(Registry::Get().Find("Census")); }

	static int LGet(lua_State* L) {
		const Census* c = Self();
		const int t = luaL_checkint(L, 1);
		if (c == nullptr || !c->Enabled() || t < 0 || t >= (int)c->sums.size()) return 0;
		lua_pushnumber(L, c->units[t]);
		lua_pushnumber(L, c->sums[t]);
		lua_pushnumber(L, c->lastFrame);
		return 3;
	}
	static int LEvents(lua_State* L) {
		Census* c = Self();
		if (c == nullptr || !c->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		lua_createtable(L, (int)c->events.size(), 0);
		for (size_t i = 0; i < c->events.size(); ++i) { lua_pushnumber(L, c->events[i]); lua_rawseti(L, -2, (int)i + 1); }
		c->events.clear();
		return 1;
	}
	static int LConfig(lua_State* L) {
		const Census* c = Self();
		if (c == nullptr) return 0;
		lua_newtable(L);
		lua_pushnumber(L, c->period);          lua_setfield(L, -2, "period");
		lua_pushnumber(L, c->defaultWeight);   lua_setfield(L, -2, "default_weight");
		lua_pushboolean(L, c->countUnfinished); lua_setfield(L, -2, "count_unfinished");
		return 1;
	}

	CensusConfig cfg;
	int period = 30, defaultWeight = 1;   // period in frames; KT1 lint: allow frame-literal the value before LoadConfig replaces it
	bool countUnfinished = false;
	std::vector<int> weight;          // per def id
	std::vector<int> units, sums;     // per team, the last tally
	std::vector<int> events;          // (team, old, new) triples since the last pull
	int lastFrame = -1;
};

} // namespace

KT1_REGISTER_SYSTEM(Census, 100, Census)

} // namespace KT1
