/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Orders (version 1). The unit lists of the orders gadget's per-frame passes, read in the engine.
//
// Replaces:   in LuaRules/Gadgets/ord_orders.lua, the two unit walks: TrackMoves' walk (every 3 frames: for every unit with
//             orders, def, transporter and current command read from Lua) and the 8-frame safety net (GetAllUnits, then
//             GetUnitCommandCount per unit). The gadget keeps every rule: the fill, the redirect, the stuck and give-up rules,
//             the order list. With the rule off it walks in Lua as before.
// Switch:     mod option kt1_orders (default off)
// Config:     data/kt1/orders.json (no tuning: the lists are exact)
// Unit data:  none. A ground mover is a def that can move and cannot fly (as the gadget's groundMover).
// Timer:      none (the lists are built when Lua asks, on the gadget's frames; KT1::Orders stays at 0)
// Lua:        Spring.KT1.Orders.Movers() -> { unitID, ... }  ascending: ground movers not carried whose current command is a
//                                                            MOVE without the internal option (synced only)
//             Spring.KT1.Orders.Busy()   -> { unitID, ... }  ascending: units with a non-empty queue (synced only)
//             Spring.KT1.Orders.Config() -> {}
// Queue:      the queue GetUnitCurrentCommand and GetUnitCommandCount read: a factory's newUnitCommands, else commandQue.
// Determinism: ids in ascending order; no float, no clock, no hash order.

#include <algorithm>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
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
	Orders() : Rule({"Orders", 1, true, "LuaRules/Gadgets/ord_orders.lua (TrackMoves walk, 8-frame safety net)", false}) {}

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

	static int LMovers(lua_State* L) {
		const Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		return PushIds(L, [o](const CUnit* u) { return o->Mover(u); });
	}
	static int LBusy(lua_State* L) {
		const Orders* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		return PushIds(L, [](const CUnit* u) { return !Queue(u).empty(); });
	}
	static int LConfig(lua_State* L) {
		lua_newtable(L);
		return 1;
	}

	std::vector<char> ground;   // per def id: can move and cannot fly
	std::vector<int> scratch;   // reused id list
};

} // namespace

KT1_REGISTER_SYSTEM(Orders, 200, Orders)

} // namespace KT1
