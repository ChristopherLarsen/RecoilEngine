/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: a row's compared columns to the check helper.
// KT1_SCAFFOLD generated from docs/engine/families/ORDERS.spec.json by game-build tools/engine/kt1_scaffold.py.
// Do not edit: change the spec and run the generator again.

#include "OrderRulesCheck.h"

#include "Game/KT1/Host/CheckColumns.h"

namespace KT1 {

void ReportOrderRulesColumns(int id, const Columns::OrderRulesRow& row) {
	Columns::OrderRulesRow::Visit(row, CheckColumns{Columns::OrderRulesRow::SYSTEM, id});
}

} // namespace KT1
