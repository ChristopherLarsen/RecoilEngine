/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Orders (version 1). The unit lists of the orders gadget's per-frame passes, read in the engine.
//
// Replaces:   in LuaRules/Gadgets/ord_orders.lua, the two unit walks: TrackMoves' walk (every 3 frames: for every unit with
//             orders, def, transporter and current command read from Lua) and the 8-frame safety net (GetAllUnits, then
//             GetUnitCommandCount per unit). The gadget keeps every rule: the fill, the redirect, the stuck and give-up rules,
//             the order list. With the rule off it walks in Lua as before.
// Switch:     mod option kt1_orders (default off in the engine; the game's option is on). check: the Lua walks and decides;
//             the busy-unit turn and the queue handling run beside it and are compared (no order given, nothing written)
// Config:     gamedata/kt1/orders.lua or, with kt1_jsonconfig, data/kt1/orders.json (no tuning: the lists are exact). The
//             family's tunables are read from data/kt1/orders.json by OrdersShared.cpp (check mode)
// Unit data:  none. A ground mover is a def that can move and cannot fly (as the gadget's groundMover).
// Timer:      none for the lists (built when Lua asks, on the gadget's frames); KT1::Orders times the check's phases
// Lua:        Spring.KT1.Orders.Movers() -> { unitID, ... }  ascending: ground movers not carried whose current command is a
//                                                            MOVE without the internal option (synced only)
//             Spring.KT1.Orders.Busy()   -> { unitID, ... }  ascending: units with a non-empty queue (synced only)
//             Spring.KT1.Orders.Config() -> {}
//             Spring.KT1.Orders.TurnWatch, TurnBegin, TurnRead, TurnEnd: the busy-unit turn (KT1OrdersTurn.h)
// Queue:      the queue GetUnitCurrentCommand and GetUnitCommandCount read: a factory's newUnitCommands, else commandQue.
// Determinism: ids in ascending order; no float, no clock, no hash order.

#include <algorithm>
#include <memory>
#include <vector>

#include "KT1OrdersTurn.h"
#include "OrdersInternal.h"

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/Host/KT1Trace.h"
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

class OrdersRule : public Rule {
public:
	OrdersRule() : Rule({"Orders", 1, true, "LuaRules/Gadgets/ord_orders.lua (TrackMoves walk, 8-frame safety net)", false, true}) {}
	~OrdersRule() override { Orders::Turn::Stop(); }

	// game setup, through whichever loader the game's config files take
	void LoadConfig(const LuaTable&) override { SetUp(); }
	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		SetUp();
		return Rule::ReadConfig(text, strictness);
	}

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
		Orders::Turn::PushLua(L);
	}

private:
	static OrdersRule* Self() { return static_cast<OrdersRule*>(Registry::Get().Find("Orders")); }

	// Once a game (kt1_jsonconfig=check calls both loaders, also for a system that is off). With the switch on and no
	// recording, nothing below the reset runs.
	void SetUp() {
		if (setUp) return;
		setUp = true;
		Orders::Internal::ResetShared();
		if (!Enabled()) return;
		const bool check = SwitchMode() == Mode::Check;
		Orders::Turn::Start(check, Trace::On("Orders"));
		if (check) Orders::Internal::RegisterQueueHandling();   // check-safe; "on" takes it over with the Lua's stand-down
	}

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
		const OrdersRule* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		return PushIds(L, [o](const CUnit* u) { return o->Mover(u); });
	}
	static int LBusy(lua_State* L) {
		const OrdersRule* o = Self();
		if (o == nullptr || !o->Enabled() || !CLuaHandle::GetHandleSynced(L)) return 0;
		return PushIds(L, [](const CUnit* u) { return !Queue(u).empty(); });
	}
	static int LConfig(lua_State* L) {
		lua_newtable(L);
		return 1;
	}

	bool setUp = false;
	std::vector<char> ground;   // per def id: can move and cannot fly
	std::vector<int> scratch;   // reused id list
};

} // namespace

KT1_REGISTER_SYSTEM(Orders, 200, OrdersRule)

} // namespace KT1
