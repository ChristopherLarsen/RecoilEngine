/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: Spring.KT1.Check, the Lua door to Host/Check.h (for a check gadget that compares on the Lua side).
// A registry entry only to get its table: it has no switch of its own and does nothing per step.

#include "Game/KT1/Host/Check.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaUtils.h"

namespace KT1 {
namespace {

// Report(system, compared, differed [, detail])
int LReport(lua_State* L) {
	const char* system = luaL_checkstring(L, 1);
	const long long compared = luaL_checkint(L, 2);
	const long long differed = luaL_checkint(L, 3);
	Check::Add(system, compared, differed, luaL_optstring(L, 4, nullptr));
	return 0;
}

int LTotal(lua_State*) {
	Check::Total();
	return 0;
}

class CheckApi : public Rule {
public:
	CheckApi() : Rule({"Check", 1, false, "nothing (check-mode reporting)", false}) {}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Report", LReport);
		LuaPushRawNamedCFunc(L, "Total", LTotal);
	}
};

} // namespace

KT1_REGISTER_SYSTEM(Check, 50, CheckApi)

} // namespace KT1
