/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: OrderRules (version 1). The order rules as rows of command, unit class, condition and action
// (data/kt1/orderrules.json), asked before the Lua gadgets.
//
// Replaces:   ord_orders.lua: AllowCommand, AllowUnitBuildStep, AllowUnitTransportLoad/Unload; ord_refuse.lua;
//             ord_edge; ord_known_target; ord_reclaim_still; ord_transfer; mov_goal_cell; mov_shift_keep;
//             mov_short_order; mov_air_unload_gap; mov_air_idle_water
// Switch:     mod option kt1_orderrules = 0 / 1 / check; unset: the family's kt1_fam_orders
// Config:     data/kt1/orderrules.json (Core/OrderRules/OrderRuleRow.h has the keys, defaults and labels)
// Allow:      AllowCommand, AllowUnitBuildStep, AllowFeatureBuildStep, AllowUnitTransportLoad, AllowUnitTransportUnload
// Phases:     FrameStart 2000 idle aircraft over water, 2400 repeated trims of transferred factories; AfterUnits the
//             events (UnitCreated, UnitDestroyed, UnitUnloaded, UnitGiven)
// State:      per unit (Core/OrderRules/GuardLogic.h), in the sync checksum when live
// Live:       a group whose rows change Orders' state acts only while system Orders holds that state
// Check:      each group's answer and each rewritten order (OrderRulesAdapter.h has the keys). RuleInfo.checkSafe stays
//             false until the replaced gadgets answer those keys
// Lua (synced handles only):
//   Spring.KT1.OrderRules.Handles(gadget)     -> the engine answers that gadget's order calls now (it returns true)
//   Spring.KT1.OrderRules.Passing(row, on)    the Lua gives the order row `row` would give: the row lets it through
//   Spring.KT1.OrderRules.Value(name, number) -> a guard value of the running data set, over the file's
//   Spring.KT1.OrderRules.Accepts(unitDefID, words) -> a def's accepted-order words of the running data set
//   Spring.KT1.OrderRules.PickupReach(unitDefID, elmos) -> a transport def's pickup reach
// Doc:        game-build docs/engine/rules/orderrules.md

#include <string>
#include <string_view>

#include "OrderRulesAdapter.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Host/AllowChain.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/Rules/Orders/OrdersInternal.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Sim/Features/Feature.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "System/float3.h"
#include "System/Log/ILog.h"

namespace KT1::OrderRules {
namespace {

using Rows::CallIn;
using Rows::CompiledRow;
using Rows::Condition;

// a row that writes Orders' state when it acts
bool ChangesState(Condition c) {
	switch (c) {
		case Condition::EngineFireState: case Condition::GameFireState: case Condition::SelfDestruct:
		case Condition::CancelsSelfDestruct: case Condition::DiveToggle: case Condition::FerryRoute:
		case Condition::EndsFerry: case Condition::GoalCell: case Condition::LivingReclaim:
		case Condition::TransportGate: return true;
		default: return false;
	}
}

const char* Suffix(CallIn callIn) {
	switch (callIn) {
		case CallIn::BuildStep: return ".build";
		case CallIn::FeatureStep: return ".feature";
		case CallIn::TransportLoad: return ".load";
		case CallIn::TransportUnload: return ".unload";
		default: return "";
	}
}

bool Natively(const Group& g, bool check) {
	if (Orders::Internal::State() != nullptr) return true;
	return check ? !g.answersFromState : !g.changesState;
}

// the groups of one call-in in order: ask(row) false refuses
// KT1 hot
template<class Ask> bool Run(Table& table, CallIn callIn, int unit, int other, Ask&& ask) {
	const bool check = Check::InCheck(NAME);
	for (const Group& g : table.groups[static_cast<size_t>(callIn)]) {
		if (!Natively(g, check)) continue;
		bool allow = true;
		for (const int32_t i : g.rows) {
			const CompiledRow& row = table.compiled[i];
			if (table.Skipped(row, unit)) continue;
			if (!ask(row)) {
				allow = false;
				break;
			}
		}
		if (check) Check::Native(NAME, g.key.c_str(), unit, other, 0, allow ? 1.0f : 0.0f);
		if (!allow) return false;
	}
	return true;
}

} // namespace

void Table::Compile() {
	compiled.clear();
	for (auto& list : groups) list.clear();
	luaPassing.fill(0);
	for (size_t i = 0; i < config.rows.size() && i < Rows::MAX_ROWS; ++i) {
		const Rows::OrderRuleRow& source = config.rows[i];
		CompiledRow row;
		if (!Rows::Compile(source, static_cast<int32_t>(i), row)) {
			LOG_L(L_ERROR, "[KT1] %s: row %s names something unknown: left out", NAME, source.name.c_str());
			continue;
		}
		std::vector<Group>& list = groups[static_cast<size_t>(row.callIn)];
		if (list.empty() || list.back().gadget != source.gadget)
			list.push_back(Group{source.gadget, source.gadget + Suffix(row.callIn), {}, false, false});
		Group& g = list.back();
		g.rows.push_back(static_cast<int32_t>(compiled.size()));
		g.changesState = g.changesState || ChangesState(row.condition);
		g.answersFromState = g.answersFromState || row.condition == Condition::TransportGate;
		compiled.push_back(row);
	}
	if (config.rows.size() > Rows::MAX_ROWS) LOG_L(L_ERROR, "[KT1] %s: more than %d rows", NAME, static_cast<int>(Rows::MAX_ROWS));
}

bool Table::Skipped(const CompiledRow& row, int unit) const {
	return luaPassing[row.source] > 0 || state.Reissuing(unit, row.source);
}

namespace {

class OrderRulesRule : public Rule {
public:
	OrderRulesRule() : Rule({NAME, 1, true, "ord_orders.lua: AllowCommand, AllowUnitBuildStep, AllowUnitTransportLoad/Unload; ord_refuse.lua; ord_edge; ord_known_target; ord_reclaim_still; ord_transfer; mov_goal_cell; mov_shift_keep; mov_short_order; mov_air_unload_gap; mov_air_idle_water", false}) {}

	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		return Data::ParseInto(text, strictness, table.config);
	}

	void Start() override {
		table.Compile();
		table.state.Init(MAX_UNITS);
		RegisterValidators();
		RegisterFrameWork(table);
		RegisterTransportOrders();
		LOG("[KT1] %s: %d rows", NAME, static_cast<int>(table.compiled.size()));
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Handles", LHandles);
		LuaPushRawNamedCFunc(L, "Passing", LPassing);
		LuaPushRawNamedCFunc(L, "Value", LValue);
		LuaPushRawNamedCFunc(L, "Accepts", LAccepts);
		LuaPushRawNamedCFunc(L, "PickupReach", LPickupReach);
	}

private:
	void RegisterValidators() {
		AllowChain::AddCommand(NAME, 0, [this](const CUnit* u, const ::Command& c, int, bool, bool, Faked) {
			return AskOrder(*u, c);
		});
		AllowChain::AddBuildStep(NAME, 0, [this](const CUnit* builder, const CUnit* u, float part, Faked) {
			return Run(table, CallIn::BuildStep, builder->id, u->id, [&](const CompiledRow& row) {
				return AskStepRow(table, row, *builder, u, part, gs->frameNum);
			});
		});
		AllowChain::AddFeatureBuildStep(NAME, 0, [this](const CUnit* builder, const CFeature* f, float part, Faked) {
			return Run(table, CallIn::FeatureStep, builder->id, f->id, [&](const CompiledRow& row) {
				return AskStepRow(table, row, *builder, nullptr, part, gs->frameNum);
			});
		});
		AllowChain::AddTransportLoad(NAME, 0, [this](const CUnit* t, const CUnit* cargo, const float3& pos, bool allowed, Faked) {
			return Transport(CallIn::TransportLoad, *t, *cargo, pos, allowed);
		});
		AllowChain::AddTransportUnload(NAME, 0, [this](const CUnit* t, const CUnit* cargo, const float3& pos, bool allowed, Faked) {
			return Transport(CallIn::TransportUnload, *t, *cargo, pos, allowed);
		});
	}

	// KT1 hot
	bool AskOrder(const CUnit& u, const ::Command& c) {
		const auto& defs = Orders::Internal::Defs();
		const int def = u.unitDef->id;
		if (def <= 0 || def >= static_cast<int>(defs.size())) return true;
		const CommandAsk ask{u, c, Orders::Internal::KindOf(c.GetID()), defs[def], gs->frameNum};
		return Run(table, CallIn::Command, u.id, c.GetID(), [&](const CompiledRow& row) {
			if (!Rows::Matches(row, ask.kind, ask.def, c.IsInternalOrder())) return true;
			return AskCommandRow(table, row, ask);
		});
	}

	bool Transport(CallIn callIn, const CUnit& t, const CUnit& cargo, const float3& pos, bool allowed) {
		return Run(table, callIn, t.id, cargo.id, [&](const CompiledRow& row) {
			return AskTransportRow(table, row, t, cargo, pos.x, pos.z, allowed, gs->frameNum);
		});
	}

	// the running rule for a synced caller, else nullptr
	static OrderRulesRule* Self(lua_State* L) {
		OrderRulesRule* r = static_cast<OrderRulesRule*>(Registry::Get().Find(NAME));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}

	static int LHandles(lua_State* L) {
		const std::string gadget = luaL_checkstring(L, 1);
		const OrderRulesRule* r = Self(L);
		bool handles = false;
		if (r != nullptr && r->SwitchMode() == Mode::On) {
			// ord_transfer has no row: its work sets the fire state, which is Orders'
			handles = gadget == "ord_transfer" && Orders::Internal::State() != nullptr;
			for (const auto& list : r->table.groups) {
				for (const Group& g : list) handles = handles || (g.gadget == gadget && Natively(g, false));
			}
		}
		lua_pushboolean(L, handles);
		return 1;
	}

	static int LPassing(lua_State* L) {
		const std::string name = luaL_checkstring(L, 1);
		const bool on = lua_toboolean(L, 2);
		OrderRulesRule* r = Self(L);
		if (r == nullptr) return 0;
		for (const CompiledRow& row : r->table.compiled) {
			if (r->table.config.rows[row.source].name != name) continue;
			int32_t& count = r->table.luaPassing[row.source];
			count = on ? count + 1 : (count > 0 ? count - 1 : 0);
		}
		return 0;
	}

	static int LValue(lua_State* L) {
		const std::string name = luaL_checkstring(L, 1);
		const float value = luaL_checkfloat(L, 2);
		OrderRulesRule* r = Self(L);
		bool taken = r != nullptr;
		if (taken) {
			Rows::GuardValues& v = r->table.config.values;
			if (name == "short_order_mu") v.short_order_mu = value;
			else if (name == "goal_cell_mu") v.goal_cell_mu = value;
			else if (name == "rest_short_mu") v.rest_short_mu = value;
			else taken = false;
		}
		lua_pushboolean(L, taken);
		return 1;
	}

	static int LAccepts(lua_State* L) {
		const int def = luaL_checkint(L, 1);
		const char* words = luaL_checkstring(L, 2);
		lua_pushboolean(L, Self(L) != nullptr && Orders::Internal::HandAccepts(def, words));
		return 1;
	}

	static int LPickupReach(lua_State* L) {
		const int def = luaL_checkint(L, 1);
		const float elmos = luaL_checkfloat(L, 2);
		OrderRulesRule* r = Self(L);
		const bool taken = r != nullptr && def > 0 && def <= static_cast<int>(unitDefHandler->NumUnitDefs());
		if (taken) {
			std::vector<float>& reach = r->table.pickupReach;
			if (def >= static_cast<int>(reach.size())) reach.resize(def + 1, 0.0f);
			reach[def] = elmos;
		}
		lua_pushboolean(L, taken);
		return 1;
	}

	Table table;
};

} // namespace

} // namespace KT1::OrderRules

namespace KT1 {

KT1_REGISTER_SYSTEM(OrderRules, 2200, OrderRules::OrderRulesRule)

} // namespace KT1
