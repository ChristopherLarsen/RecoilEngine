/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: its columns, declared once for the check, the trace recorder and the replay. No engine include.
// KT1_SCAFFOLD generated from docs/engine/families/ORDERS.spec.json by game-build tools/engine/kt1_scaffold.py.
// Do not edit: change the spec and run the generator again.

#ifndef KT1_SCAFFOLD_ORDERRULES_COLUMNS_H
#define KT1_SCAFFOLD_ORDERRULES_COLUMNS_H

#include <array>
#include <cstdint>
#include <vector>

namespace KT1::Columns {

// one row: what the system computed for one id (the caller says what the id is: a unit, a call, a step)
struct OrderRulesRow {
	static constexpr const char* SYSTEM = "OrderRules";
	static constexpr int COUNT = 2;

	// each Allow answer per validator, fakes included
	bool allow_answer{};
	// the rewritten order a refuse-and-re-issue row gives
	std::vector<float> reissued{};

	// every column once, in the spec's order: visit(name, value, compare)
	template<class R, class V> static void Visit(R& row, V&& visit) {
		visit("allow_answer", row.allow_answer, true);
		visit("reissued", row.reissued, true);
	}
};

} // namespace KT1::Columns

#endif
