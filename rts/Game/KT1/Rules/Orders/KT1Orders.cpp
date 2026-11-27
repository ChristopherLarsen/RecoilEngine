/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Orders (version 2). The unit lists of the orders gadget's per-frame passes, read in the engine.
//
// Replaces:   in LuaRules/Gadgets/ord_orders.lua, the two unit walks: TrackMoves' walk (every 3 frames: for every unit with
//             orders, def, transporter and current command read from Lua) and the 8-frame safety net (GetAllUnits, then
//             GetUnitCommandCount per unit). The gadget keeps every rule: the fill, the redirect, the stuck and give-up rules,
//             the order list. With the rule off it walks in Lua as before.
// Switch:     mod option kt1_orders (default off)
// Config:     gamedata/kt1/orders.lua (no tuning: the lists are exact)
// Unit data:  none. A ground mover is a def that can move and cannot fly (as the gadget's groundMover).
// Timer:      none (the lists are built when Lua asks, on the gadget's frames; KT1::Orders stays at 0)
// Lua:        Spring.KT1.Orders.Movers() -> { unitID, ... }  ascending: ground movers not carried whose current command is a
//                                                            MOVE without the internal option (synced only)
//             Spring.KT1.Orders.Busy()   -> { unitID, ... }  ascending: units with a non-empty queue (synced only)
//             Spring.KT1.Orders.Movers(true) -> { u, cmdID, opts, tag, gx, gz, ux, uz, speed, stunned, carried, ... }
//                                                            (v2: 11 per mover; gx, gz the front command's goal, else ux, uz;
//                                                            speed the velocity length; stunned and carried 0 or 1)
//             Spring.KT1.Orders.Busy(true)   -> { u, cmdID, opts, tag, count, ... }  (v2: 5 per unit; count the queue length)
//             Spring.KT1.Orders.FrontChanged() -> { unitID, ... }  ascending: units whose front command (id or tag) changed,
//                                                            or whose queue emptied, or that became busy, since the last call
//             Spring.KT1.Orders.BusyDelta()  -> { u, cmdID, opts, tag, count, ... }  (v3: Busy(true)'s rows only for units whose
//                                                            front changed since the last call, or new; count 0: queue emptied)
//             Spring.KT1.Orders.Config() -> {}
// Queue:      the queue GetUnitCurrentCommand and GetUnitCommandCount read: a factory's newUnitCommands, else commandQue.
// Determinism: ids in ascending order; no float, no clock, no hash order.

#include <algorithm>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

class Orders : public Rule {
public:
	Orders() : Rule({"Orders", 3, true, "LuaRules/Gadgets/ord_orders.lua (TrackMoves walk, 8-frame safety net)", false}) {}

	void Start() override {
		const int n = unitDefHandler->NumUnitDefs();
		ground.assign(n + 1, 0);
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			ground[d] = (ud != nullptr && ud->canmove && !ud->canfly) ? 1 : 0;
		}
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Movers", LMovers);
		LuaPushRawNamedCFunc(L, "Busy", LBusy);
		LuaPushRawNamedCFunc(L, "FrontChanged", LFrontChanged);
		LuaPushRawNamedCFunc(L, "BusyDelta", LBusyDelta);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static Orders* Self() { return static_cast<Orders*>(Registry::Get().Find("Orders")); }

	static const CCommandQueue& Queue(const CUnit* u) {
		const CCommandAI* cai = u->commandAI;
		const CFactoryCAI* fac = dynamic_cast<const CFactoryCAI*>(cai);
		return (fac == nullptr) ? cai->commandQue : fac->newUnitCommands;
	}

	bool Mover(const CUnit* u) const {
		const int d = u->unitDef->id;
		if (d < 0 || d >= (int)ground.size() || ground[d] == 0) return false;
		if (u->GetTransporter() != nullptr) return false;
		const CCommandQueue& q = Queue(u);
		if (q.empty()) return false;
		const Command& c = q.front();
		return c.GetID() == CMD_MOVE && !c.IsInternalOrder();
	}

	template<typename Pred> static int PushIds(lua_State* L, Pred keep) {
		std::vector<int>& ids = Self()->scratch;
		ids.clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (keep(u)) ids.push_back(u->id);
		}
		std::sort(ids.begin(), ids.end());
		lua_createtable(L, (int)ids.size(), 0);
		for (size_t i = 0; i < ids.size(); ++i) { lua_pushnumber(L, ids[i]); lua_rawseti(L, -2, (int)i + 1); }
		return 1;
	}

	static std::vector<const CUnit*>& Sorted(Orders* o, bool (*keep)(const Orders*, const CUnit*)) {
		std::vector<const CUnit*>& us = o->units;
		us.clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (keep(o, u)) us.push_back(u);
		}
		std::sort(us.begin(), us.end(), [](const CUnit* a, const CUnit* b) { return a->id < b->id; });
		return us;
	}
	static void Put(lua_State* L, int& i, double v) { lua_pushnumber(L, v); lua_rawseti(L, -2, ++i); }

	static int LMovers(lua_State* L) {
		Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		if (!lua_toboolean(L, 1)) return PushIds(L, [o](const CUnit* u) { return o->Mover(u); });
		const auto& us = Sorted(o, [](const Orders* s, const CUnit* u) { return s->Mover(u); });
		lua_createtable(L, (int)us.size() * 11, 0);
		int i = 0;
		for (const CUnit* u : us) {
			const Command& c = Queue(u).front();
			const bool goal = c.GetNumParams() >= 3;
			Put(L, i, u->id); Put(L, i, c.GetID()); Put(L, i, c.GetOpts()); Put(L, i, c.GetTag());
			Put(L, i, goal ? c.GetParam(0) : u->pos.x); Put(L, i, goal ? c.GetParam(2) : u->pos.z);
			Put(L, i, u->pos.x); Put(L, i, u->pos.z); Put(L, i, u->speed.w);
			Put(L, i, u->IsStunned() ? 1 : 0); Put(L, i, (u->GetTransporter() != nullptr) ? 1 : 0);
		}
		return 1;
	}
	static int LBusy(lua_State* L) {
		Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		if (!lua_toboolean(L, 1)) return PushIds(L, [](const CUnit* u) { return !Queue(u).empty(); });
		const auto& us = Sorted(o, [](const Orders*, const CUnit* u) { return !Queue(u).empty(); });
		lua_createtable(L, (int)us.size() * 5, 0);
		int i = 0;
		for (const CUnit* u : us) {
			const CCommandQueue& q = Queue(u);
			const Command& c = q.front();
			Put(L, i, u->id); Put(L, i, c.GetID()); Put(L, i, c.GetOpts()); Put(L, i, c.GetTag()); Put(L, i, (double)q.size());
		}
		return 1;
	}
	// per unit id: the front seen at the last call (busy, id, tag) and the call it was seen in; a unit gone since is dropped
	struct Front { int id = 0; unsigned int tag = 0; unsigned int seen = 0; bool busy = false; };
	static int LFrontChanged(lua_State* L) {
		Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		const unsigned int call = ++o->calls;
		std::vector<int>& ids = o->scratch;
		ids.clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (u->id < 0) continue;
			if ((size_t)u->id >= o->fronts.size()) o->fronts.resize(u->id + 1);
			Front& f = o->fronts[u->id];
			const CCommandQueue& q = Queue(u);
			const bool busy = !q.empty();
			const int cid = busy ? q.front().GetID() : 0;
			const unsigned int tag = busy ? q.front().GetTag() : 0;
			const bool fresh = (f.seen != call - 1);       // not seen last call: a new unit (or id reuse)
			if (fresh ? busy : (busy != f.busy || (busy && (cid != f.id || tag != f.tag)))) ids.push_back(u->id);
			f.busy = busy; f.id = cid; f.tag = tag; f.seen = call;
		}
		std::sort(ids.begin(), ids.end());
		lua_createtable(L, (int)ids.size(), 0);
		for (size_t i = 0; i < ids.size(); ++i) { lua_pushnumber(L, ids[i]); lua_rawseti(L, -2, (int)i + 1); }
		return 1;
	}
	// v3: Busy(true)'s row (u, cmdID, opts, tag, count) only for the units whose front (busy, id, tag) changed since the last
	// BusyDelta call, or that are new since it; count 0 for a unit whose queue emptied. Its own state (FrontChanged's is apart).
	static int LBusyDelta(lua_State* L) {
		Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		const unsigned int call = ++o->dcalls;
		std::vector<const CUnit*>& us = o->units;
		us.clear();
		for (const CUnit* u : unitHandler.GetActiveUnits()) {
			if (u->id < 0) continue;
			if ((size_t)u->id >= o->dfronts.size()) o->dfronts.resize(u->id + 1);
			Front& f = o->dfronts[u->id];
			const CCommandQueue& q = Queue(u);
			const bool busy = !q.empty();
			const int cid = busy ? q.front().GetID() : 0;
			const unsigned int tag = busy ? q.front().GetTag() : 0;
			const bool fresh = (f.seen != call - 1);
			if (fresh ? busy : (busy != f.busy || (busy && (cid != f.id || tag != f.tag)))) us.push_back(u);
			f.busy = busy; f.id = cid; f.tag = tag; f.seen = call;
		}
		std::sort(us.begin(), us.end(), [](const CUnit* a, const CUnit* b) { return a->id < b->id; });
		lua_createtable(L, (int)us.size() * 5, 0);
		int i = 0;
		for (const CUnit* u : us) {
			const CCommandQueue& q = Queue(u);
			if (q.empty()) { Put(L, i, u->id); Put(L, i, 0); Put(L, i, 0); Put(L, i, 0); Put(L, i, 0); continue; }
			const Command& c = q.front();
			Put(L, i, u->id); Put(L, i, c.GetID()); Put(L, i, c.GetOpts()); Put(L, i, c.GetTag()); Put(L, i, (double)q.size());
		}
		return 1;
	}
	static int LConfig(lua_State* L) {
		lua_newtable(L);
		return 1;
	}

	std::vector<char> ground;   // per def id: can move and cannot fly
	std::vector<int> scratch;   // reused id list
	std::vector<const CUnit*> units;   // reused unit list (packed rows)
	std::vector<Front> fronts;  // FrontChanged state, by unit id
	unsigned int calls = 0;     // FrontChanged calls
	std::vector<Front> dfronts; // BusyDelta state, by unit id
	unsigned int dcalls = 0;    // BusyDelta calls
};

} // namespace

std::unique_ptr<Rule> MakeOrders() { return std::make_unique<Orders>(); }

} // namespace KT1
