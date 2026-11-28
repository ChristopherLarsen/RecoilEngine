/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules (LUA-FREE S11, S14): the table that answers the order Allow chain, and what its rows keep between calls.
// The tests are Core/OrderRules; this folder gathers their facts from the engine and gives the orders.
//
// Rows are asked in list order, which is the replaced gadgets' layer order. The rows of one gadget are one group: the
// first refusing row ends the group and the call, as a refusing gadget ends the Lua chain. In check mode every group
// reports its answer under the gadget's name (see GroupKey), and the replaced handler answers the same key:
//   Spring.KT1.Check.Answer("OrderRules", "<gadget>", unitID, cmdID, 0, allow)                       AllowCommand
//   Spring.KT1.Check.Answer("OrderRules", "<gadget>.build" | ".feature", builderID, targetID, 0, allow)
//   Spring.KT1.Check.Answer("OrderRules", "<gadget>.load" | ".unload", transporterID, cargoID, 0, allow)
// A rewritten order is reported by both sides as "reissued", unitID, the new order's id, floor(first parameter), first
// and third parameter.

#ifndef KT1_RULES_ORDER_RULES_ADAPTER_H
#define KT1_RULES_ORDER_RULES_ADAPTER_H

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/OrderRules/GuardLogic.h"
#include "Game/KT1/Core/OrderRules/OrderRuleRow.h"
#include "Game/KT1/Core/Orders/DefFacts.h"
#include "Game/KT1/Core/Orders/OrderTypes.h"

struct Command;
class CUnit;
class CFeature;

namespace KT1::OrderRules {

namespace Rows = Core::OrderRules;

constexpr const char* NAME = "OrderRules";

// the rows of one gadget in one call-in
struct Group {
	std::string gadget;              // as the config names it
	std::string key;                 // the check key: the gadget, with the call-in's suffix
	std::vector<int32_t> rows;       // indexes into Table::compiled
	bool changesState = false;       // acting live needs Orders' state (system Orders)
	bool answersFromState = false;   // even the answer needs it
};

struct Table {
	Rows::RulesConfig config;
	std::vector<Rows::CompiledRow> compiled;
	std::array<std::vector<Group>, static_cast<size_t>(Rows::CallIn::Count)> groups;
	std::array<int32_t, Rows::MAX_ROWS> luaPassing{};   // by source row: the replaced Lua is giving that row's own order
	std::vector<float> pickupReach;  // elmos by unit def id (0: none), handed over by the game (ORD-013 v3 R3)
	Rows::State state;

	// config.rows to compiled and groups; a row with an unknown name is logged and left out
	void Compile();
	// the row is skipped: it is giving this unit its own order, here or in Lua
	bool Skipped(const Rows::CompiledRow& row, int unit) const;
};

// one asked order
struct CommandAsk {
	const CUnit& unit;
	const ::Command& command;
	Core::Orders::CommandKind kind;
	const Core::Orders::DefFacts& def;
	int frame;
};

// a row's condition and its work; false: the row refuses (OrderRulesCommand.cpp)
bool AskCommandRow(Table& table, const Rows::CompiledRow& row, const CommandAsk& ask);
// a build or feature step of `builder` on a unit or (target null) a feature; part < 0 takes it apart (OrderRulesSteps.cpp)
bool AskStepRow(Table& table, const Rows::CompiledRow& row, const CUnit& builder, const CUnit* target, float part, int frame);
// the transport gates (OrderRulesSteps.cpp)
bool AskTransportRow(const Table& table, const Rows::CompiledRow& row, const CUnit& transporter, const CUnit& cargo,
                     float x, float z, bool engineAllows, int frame);

// gives a row's rewritten order with the row's own guard up, and reports it in check mode (OrderRulesCommand.cpp)
void Reissue(Table& table, const Rows::CompiledRow& row, int unit, int commandID, std::span<const float> params,
             std::uint8_t options);

// hears the transports' load and unload orders for their gates (OrderRulesSteps.cpp)
void RegisterTransportOrders();
// the work between orders: phases, events and sync columns (OrderRulesFrame.cpp)
void RegisterFrameWork(Table& table);

} // namespace KT1::OrderRules

#endif
