/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: Spring.KT1.Trace, the replaced Lua's door to Host/KT1Trace.h (synced handles only; elsewhere On is false and
// the rest does nothing). A registry entry only to get its table: it has no switch of its own and does nothing per step.

#include <vector>

#include "Game/KT1/Host/KT1Trace.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"

namespace KT1 {
namespace {

constexpr int ARG_ID = 3;       // DESIGN: Out(system, column, id1, id2, id3, values...): three ids from here
constexpr int ARG_VALUE = 6;    // DESIGN: values from here

int LOn(lua_State* L) {
	const char* system = luaL_checkstring(L, 1);
	lua_pushboolean(L, CLuaHandle::GetHandleSynced(L) && Trace::On(system));
	return 1;
}

int Row(lua_State* L, uint8_t flags) {
	const char* system = luaL_checkstring(L, 1);
	if (!CLuaHandle::GetHandleSynced(L) || !Trace::On(system)) return 0;
	const char* column = luaL_checkstring(L, 2);
	Core::Trace::Key key{};
	for (size_t i = 0; i < key.size(); ++i) key[i] = luaL_checkint(L, ARG_ID + static_cast<int>(i));
	static std::vector<float> values;   // reused: synced Lua is one thread
	values.clear();
	for (int a = ARG_VALUE; a <= lua_gettop(L); ++a)
		values.push_back(lua_isboolean(L, a) ? static_cast<float>(lua_toboolean(L, a)) : luaL_checkfloat(L, a));
	Trace::Output(system, column, key, values, flags);
	return 0;
}

int LOut(lua_State* L) { return Row(L, 0); }
int LList(lua_State* L) { return Row(L, Core::Trace::ORDERED); }

int LPath(lua_State* L) {
	const char* system = luaL_checkstring(L, 1);
	if (!CLuaHandle::GetHandleSynced(L) || !Trace::On(system)) return 0;
	const std::string column = std::string("path:") + luaL_checkstring(L, 2);
	Trace::Output(system, column.c_str(), {}, {}, Core::Trace::NOT_COMPARED);
	return 0;
}

int LClose(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) Trace::Close();
	return 0;
}

class TraceApi : public Rule {
public:
	TraceApi() : Rule({"Trace", 1, false, "nothing (trace recording)", false}) {}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "On", LOn);
		LuaPushRawNamedCFunc(L, "Out", LOut);
		LuaPushRawNamedCFunc(L, "List", LList);
		LuaPushRawNamedCFunc(L, "Path", LPath);
		LuaPushRawNamedCFunc(L, "Close", LClose);
	}
};

} // namespace

KT1_REGISTER_SYSTEM(Trace, 60, TraceApi)

} // namespace KT1
