/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules: Core test skeleton: the columns and the config reader. The port's cases go in files of their own.
// KT1_SCAFFOLD generated from docs/engine/families/ORDERS.spec.json by game-build tools/engine/kt1_scaffold.py.
// Do not edit: change the spec and run the generator again.

#include "../KT1Test.h"

#include "Game/KT1/Core/OrderRules/OrderRulesColumns.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Core/OrderRules/OrderRuleRow.h"

#include <string>

KT1_CASE("SCAFFOLD-ORDERRULES-COLUMNS", "every column of the family spec is visited once, in its order") {
	KT1::Columns::OrderRulesRow row;
	std::string seen;
	int visited = 0;
	int compared = 0;
	KT1::Columns::OrderRulesRow::Visit(row, [&](const char* name, const auto&, bool compare) {
		seen += name;
		seen += ' ';
		++visited;
		if (compare) ++compared;
	});
	KT1_CHECK(seen == "allow_answer reissued ");
	KT1_CHECK_EQ(visited, KT1::Columns::OrderRulesRow::COUNT);
	KT1_CHECK_EQ(compared, 2);   // DESIGN: the spec's compared columns
}

KT1_CASE("SCAFFOLD-ORDERRULES-CONFIG", "an empty file is accepted and keeps the defaults") {
	KT1::Core::OrderRules::RulesConfig config;
	KT1_CHECK(KT1::Data::ParseInto("{}", KT1::Data::Strictness::Dev, config).accepted);
}
