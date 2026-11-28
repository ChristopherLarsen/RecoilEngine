/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule SimProbe: what its Lua twin reads (every function synced handles only).
//   Spring.KT1.SimProbe.Take()        { type, id, def, team, other, arg, arg2, value, flags, x, y, z, ... } the events
//                                     since the last call, as raised (check mode), then forgotten
//   Spring.KT1.SimProbe.Commands()    { unitID, cmdID, ... } the command validator's calls since the last call
//   Spring.KT1.SimProbe.State()       { id, damage, events, ... } every tracked unit, ascending id
//   Spring.KT1.SimProbe.Counts()      { phases = {5 numbers}, vetoes = {8 numbers}, dropped = n }
//   Spring.KT1.SimProbe.DenyCommand() the refused command id

#include "KT1SimProbe.h"

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"

namespace KT1 {
namespace SimProbe {
namespace {

// The running probe, for a synced caller; nullptr otherwise.
SimProbeRule* Self(lua_State* L) {
	SimProbeRule* p = static_cast<SimProbeRule*>(Registry::Get().Find("SimProbe"));
	return (p != nullptr && p->Enabled() && CLuaHandle::GetHandleSynced(L)) ? p : nullptr;
}

void Put(lua_State* L, int& n, float v) {
	lua_pushnumber(L, v);
	lua_rawseti(L, -2, ++n);
}

int LTake(lua_State* L) {
	SimProbeRule* p = Self(L);
	if (p == nullptr) return 0;
	constexpr int STRIDE = 12;
	lua_createtable(L, static_cast<int>(p->stream.size()) * STRIDE, 0);
	int n = 0;
	for (const Core::SimHost::Event& e : p->stream) {
		Put(L, n, static_cast<float>(e.type));
		Put(L, n, e.id);
		Put(L, n, e.def);
		Put(L, n, e.team);
		Put(L, n, e.other);
		Put(L, n, e.arg);
		Put(L, n, e.arg2);
		Put(L, n, e.value);
		Put(L, n, e.flags);
		Put(L, n, e.pos[0]);
		Put(L, n, e.pos[1]);
		Put(L, n, e.pos[2]);
	}
	p->stream.clear();
	return 1;
}

int LCommands(lua_State* L) {
	SimProbeRule* p = Self(L);
	if (p == nullptr) return 0;
	lua_createtable(L, static_cast<int>(p->commands.size()), 0);
	int n = 0;
	for (const int32_t v : p->commands) Put(L, n, v);
	p->commands.clear();
	return 1;
}

int LState(lua_State* L) {
	SimProbeRule* p = Self(L);
	if (p == nullptr) return 0;
	Core::SimProbe::State& s = p->state;
	lua_createtable(L, s.units.index.Size() * 3, 0);
	int n = 0;
	for (const int32_t id : s.units.index.AscendingIds()) {
		const Core::DenseIndex::Slot slot = s.units.index.SlotOf(id);
		Put(L, n, id);
		Put(L, n, s.damage[slot]);
		Put(L, n, s.events[slot]);
	}
	return 1;
}

template<class A> void PushArray(lua_State* L, const char* key, const A& values) {
	lua_createtable(L, static_cast<int>(values.size()), 0);
	int n = 0;
	for (const auto v : values) Put(L, n, static_cast<float>(v));
	lua_setfield(L, -2, key);
}

int LCounts(lua_State* L) {
	const SimProbeRule* p = Self(L);
	if (p == nullptr) return 0;
	lua_createtable(L, 0, 3);
	PushArray(L, "phases", p->seen);
	PushArray(L, "vetoes", p->asked);
	lua_pushnumber(L, static_cast<float>(p->dropped));
	lua_setfield(L, -2, "dropped");
	return 1;
}

int LDenyCommand(lua_State* L) {
	const SimProbeRule* p = Self(L);
	if (p == nullptr) return 0;
	lua_pushnumber(L, p->denyCommand);
	return 1;
}

} // namespace

void SimProbeRule::PushLua(lua_State* L) {
	LuaPushRawNamedCFunc(L, "Take", LTake);
	LuaPushRawNamedCFunc(L, "Commands", LCommands);
	LuaPushRawNamedCFunc(L, "State", LState);
	LuaPushRawNamedCFunc(L, "Counts", LCounts);
	LuaPushRawNamedCFunc(L, "DenyCommand", LDenyCommand);
}

} // namespace SimProbe
} // namespace KT1
