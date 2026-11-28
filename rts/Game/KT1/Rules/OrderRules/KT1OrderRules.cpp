/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: OrderRules (version 1). Skeleton of the orders family (game-build tools/engine/kt1_scaffold.py):
// no logic yet. This file is the port's from here: the generator never rewrites it.
//
// Replaces:   ord_orders.lua: AllowCommand, AllowUnitBuildStep, AllowUnitTransportLoad/Unload; ord_refuse.lua;
//             ord_edge; ord_known_target; ord_reclaim_still; ord_transfer; mov_goal_cell; mov_shift_keep;
//             mov_short_order; mov_air_unload_gap; mov_air_idle_water
// Switch:     mod option kt1_orderrules = 0 / 1 / check; unset: the family's kt1_fam_orders
// Config:     data/kt1/orderrules.json (Core/OrderRules/OrderRuleRow.h has the keys, defaults and labels)
// Phase:      allow_chain (spec)
// Columns:    Core/OrderRules/OrderRulesColumns.h (check, trace, replay)
// Check:      spec check_safe: yes. RuleInfo.checkSafe stays false until the port computes the columns and reports
//             them (ReportOrderRulesColumns)
// Doc:        game-build docs/engine/rules/orderrules.md

#include "Game/KT1/KT1Register.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Core/OrderRules/OrderRuleRow.h"

namespace KT1 {
namespace {

class OrderRulesRule : public Rule {
public:
	OrderRulesRule() : Rule({"OrderRules", 1, true, "ord_orders.lua: AllowCommand, AllowUnitBuildStep, AllowUnitTransportLoad/Unload; ord_refuse.lua; ord_edge; ord_known_target; ord_reclaim_still; ord_transfer; mov_goal_cell; mov_shift_keep; mov_short_order; mov_air_unload_gap; mov_air_idle_water", false}) {}

	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		return Data::ParseInto(text, strictness, config);
	}

private:
	KT1::Core::OrderRules::RulesConfig config;
};

} // namespace

KT1_REGISTER_SYSTEM(OrderRules, 2200, OrderRulesRule)

} // namespace KT1
