/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: Spring.KT1.Check, the Lua door to Host/Check.h: Answer hands the check helper the replaced Lua's answer;
// Report is for a check gadget that compares on the Lua side.
// A registry entry only to get its table: it has no switch of its own and does nothing per step.

#include "Game/KT1/Host/Check.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaUtils.h"

namespace KT1 {
namespace {

// DESIGN: where Answer takes its ids and answers
constexpr int ARG_ID = 3;       // DESIGN: three ids from here
constexpr int ARG_FIRST = 6;    // DESIGN
constexpr int ARG_SECOND = 7;   // DESIGN
constexpr int ARG_INPUT = 8;    // DESIGN

float Number(lua_State* L, int index) {
	if (lua_isboolean(L, index)) return lua_toboolean(L, index) ? 1.0f : 0.0f;
	return luaL_checkfloat(L, index);
}

// Answer(system, what, id1, id2, id3, answer [, answer2 [, input]]); an answer is a number or a boolean
int LAnswer(lua_State* L) {
	const char* system = luaL_checkstring(L, 1);
	const char* what = luaL_checkstring(L, 2);
	const int id1 = luaL_checkint(L, ARG_ID);
	const int id2 = luaL_checkint(L, ARG_ID + 1);
	const int id3 = luaL_checkint(L, ARG_ID + 1 + 1);
	Check::Value v;
	v.v[0] = Number(L, ARG_FIRST);
	if (!lua_isnoneornil(L, ARG_SECOND)) {
		v.v[1] = Number(L, ARG_SECOND);
		v.count = Check::Value::BOTH;
	}
	if (!lua_isnoneornil(L, ARG_INPUT)) {
		v.hasInput = true;
		v.input = Number(L, ARG_INPUT);
	}
	Check::Lua(system, what, id1, id2, id3, v);
	return 0;
}

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
		LuaPushRawNamedCFunc(L, "Answer", LAnswer);
		LuaPushRawNamedCFunc(L, "Report", LReport);
		LuaPushRawNamedCFunc(L, "Total", LTotal);
	}
};

} // namespace

KT1_REGISTER_SYSTEM(Check, 50, CheckApi)

} // namespace KT1
