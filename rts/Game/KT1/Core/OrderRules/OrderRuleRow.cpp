/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: stubs behind OrderRuleRow.h (LUA-FREE H01; steps S11 and S14 port them).

#include "OrderRuleRow.h"

namespace KT1::Core::OrderRules {

bool InClass(const Orders::DefFacts&, UnitClass) { return false; }
bool ParseCallIn(std::string_view, CallIn&) { return false; }
bool ParseAction(std::string_view, Action&) { return false; }
bool ParseCondition(std::string_view, Condition&) { return false; }
bool ParseUnitClass(std::string_view, UnitClass&) { return false; }
bool Compile(const OrderRuleRow&, int32_t, CompiledRow&) { return false; }
bool Matches(const CompiledRow&, Orders::CommandKind, const Orders::DefFacts&, bool) { return false; }

} // namespace KT1::Core::OrderRules
