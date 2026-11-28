/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 LuaLeft: the two hooks CLuaHandle calls (LUA-FREE-WORKPACKAGES F08). The meter itself: KT1LuaLeft.cpp.
// Each is a scope object that does nothing but one flag test when the system is off. Neither reads or writes sim state.

#ifndef KT1_LUALEFT_H
#define KT1_LUALEFT_H

class CLuaHandle;
struct LuaHashString;
struct lua_State;

namespace KT1 { namespace LuaLeft {

// KT1 hook: LuaLeft (CLuaHandle::RunCallInTraceback): times one call into Lua. hs is null for a unit script function.
class CallScope {
public:
	CallScope(const CLuaHandle* handle, lua_State* L, const LuaHashString* hs);
	~CallScope();
	CallScope(const CallScope&) = delete;
	CallScope& operator=(const CallScope&) = delete;
private:
	bool on = false;
	bool endsGame = false;
};

// KT1 hook: LuaLeft (CLuaHandle::CollectGarbage): times one garbage collection of a state.
class GcScope {
public:
	explicit GcScope(const lua_State* L);
	~GcScope();
	GcScope(const GcScope&) = delete;
	GcScope& operator=(const GcScope&) = delete;
private:
	bool on = false;
};

}} // namespace KT1::LuaLeft

#endif
