/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: a row's compared columns to the check helper (Host/CheckColumns.h has the keys).
// KT1_SCAFFOLD generated from docs/engine/families/ORDERS.spec.json by game-build tools/engine/kt1_scaffold.py.
// Do not edit: change the spec and run the generator again.

#ifndef KT1_SCAFFOLD_ORDERRULES_CHECK_H
#define KT1_SCAFFOLD_ORDERRULES_CHECK_H

#include "Game/KT1/Core/OrderRules/OrderRulesColumns.h"

namespace KT1 {

void ReportOrderRulesColumns(int id, const Columns::OrderRulesRow& row);

} // namespace KT1

#endif
