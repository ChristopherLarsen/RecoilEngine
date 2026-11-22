/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComUnitPass.h.

#include "SupComUnitPass.h"

#include <string>
#include <vector>

#include "Game/GameSetup.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/TimeProfiler.h"

namespace {
	struct Step {
		std::string name;
		SupComUnitPass::FilterFn filter;
		SupComUnitPass::StepFn fn;
		int rate;
		int on = -1;               // -1 not read yet, then 0 or 1 (mod options are fixed for the game)
	};
	std::vector<Step>& Steps() { static std::vector<Step> s; return s; }

	struct Event { int id, kind; float a, b; };
	std::vector<Event> events;
	int eventFrame = -1;

	bool On(Step& s) {
		if (s.on < 0) {
			const auto& mo = CGameSetup::GetModOptions();
			const auto it = mo.find("unitpass_" + s.name);
			s.on = (it != mo.end() && (it->second == "1" || (s.name == "intel" && it->second == "check"))) ? 1 : 0;   // intel: "check" runs it beside Lua
		}
		return s.on == 1;
	}
}

int SupComUnitPass::Register(const char* name, FilterFn filter, StepFn fn, int rate)
{
	Steps().push_back({ name, filter, fn, rate < 1 ? 1 : rate });
	return int(Steps().size()) - 1;
}

void SupComUnitPass::Emit(int unitID, int kind, float a, float b)
{
	events.push_back({ unitID, kind, a, b });
}

void SupComUnitPass::Update()
{
	static bool registered = false;
	if (!registered) { registered = true; RegisterMoveStep(); RegisterIntelStep(); }
	const int f = gs->frameNum;
	if (eventFrame != f) { events.clear(); eventFrame = f; }

	std::vector<Step*> live;
	for (Step& s: Steps()) {
		if (On(s))
			live.push_back(&s);
	}
	if (live.empty())
		return;                    // the zero-cost path: no step on

	SCOPED_TIMER("Sim::SupComUnitPass");
	const Facts facts{ f };
	const int maxID = int(unitHandler.MaxUnits());
	for (int id = 0; id < maxID; ++id) {              // ascending id: the same order on every client
		CUnit* u = unitHandler.GetUnit(id);
		if (u == nullptr || u->isDead)
			continue;
		for (Step* s: live) {
			if (s->rate > 1 && (id % s->rate) != (f % s->rate))   // the Lua phase rule: id % rate == frame % rate
				continue;
			if (s->filter != nullptr && !s->filter(u))
				continue;
			s->fn(u, facts);
		}
	}
}

namespace {
	// Spring.SupComUnitPassEvents() -> frame, ids, kinds, as, bs   (this frame's events, in emission order)
	int PassEvents(lua_State* L) {
		if (!CLuaHandle::GetHandleSynced(L)) return 0;
		lua_pushnumber(L, eventFrame);
		const int n = int(events.size());
		lua_createtable(L, n, 0); lua_createtable(L, n, 0); lua_createtable(L, n, 0); lua_createtable(L, n, 0);
		for (int i = 0; i < n; ++i) {
			lua_pushnumber(L, events[i].id);   lua_rawseti(L, -5, i + 1);
			lua_pushnumber(L, events[i].kind); lua_rawseti(L, -4, i + 1);
			lua_pushnumber(L, events[i].a);    lua_rawseti(L, -3, i + 1);
			lua_pushnumber(L, events[i].b);    lua_rawseti(L, -2, i + 1);
		}
		return 5;
	}
}

void SupComUnitPass::PushSyncedRead(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SupComUnitPassEvents", PassEvents);
	LuaPushRawNamedCFunc(L, "SupComUnitPassSetMoveSpeed", SetMoveSpeed);
	PushIntel(L);
}
