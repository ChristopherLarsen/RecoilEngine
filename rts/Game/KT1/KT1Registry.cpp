/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the rule registry (game-build docs/engine/rules/INDEX.md). See KT1Rule.h for a rule's life.
//
// Switch: mod option kt1_<key> ("1"/"true" on, "0"/"false" off; unset: the rule's defaultOn). Off: the engine rule does
// nothing and its Lua gadget runs (the gadget asks Spring.KT1.IsActive("<Name>") and stands down when it is true).
// Config: gamedata/kt1/<key>.lua from the game archive, read once per game when the rule is on.
//
// Lua (every handle, from LuaSyncedRead):
//   Spring.KT1.Rules()        -> { {name, version, synced, replaces, enabled, option, timer}, ... } in registry order
//   Spring.KT1.IsActive(name) -> true when the engine rule exists and is on
//   Spring.KT1.<Name>         the rule's own table (its PushLua)

#include "KT1Registry.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "Game/GameSetup.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "System/FileSystem/VFSModes.h"
#include "System/Log/ILog.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {

static std::string Lower(const char* s) {
	std::string r(s);
	std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return r;
}

Rule::Rule(const RuleInfo& i) : info(i), key(Lower(i.name)), option("kt1_" + Lower(i.name)), timer(std::string("KT1::") + i.name) {}

Registry& Registry::Get() {
	static Registry reg;
	if (!reg.built) {
		reg.built = true;
		AddAllRules(reg);
	}
	return reg;
}

Rule* Registry::Find(const char* name) const {
	if (name == nullptr) return nullptr;
	const std::string k = Lower(name);
	for (const auto& r : rules)
		if (r->Key() == k) return r.get();
	return nullptr;
}

void Registry::EnsureInit() {
	if (inited || gameSetup == nullptr) return;   // no game yet (menu, LuaIntro): every rule reads as off
	inited = true;

	const auto& opts = CGameSetup::GetModOptions();
	for (auto& r : rules) {
		bool on = r->info.defaultOn;
		const auto it = opts.find(r->option);
		if (it != opts.end()) {
			const std::string v = Lower(it->second.c_str());
			on = (v == "1" || v == "true");
		}
		r->enabled = on;

		if (on) {
			const std::string path = "gamedata/kt1/" + r->key + ".lua";
			LuaParser p(path, SPRING_VFS_MOD_BASE, SPRING_VFS_ZIP, {r->info.synced}, {true});   // setup: the parser needs its own Lua state (setup false leaves it invalid)
			const bool ok = p.Execute();
			r->LoadConfig(p.GetRoot());   // an invalid root reads every field as its default
			LOG("[KT1] %s v%d %s, config %s%s%s", r->info.name, r->info.version, r->info.synced ? "synced" : "unsynced",
			    path.c_str(), ok ? "" : " (none: defaults): ", ok ? "" : p.GetErrorLog().c_str());
		} else {
			LOG("[KT1] %s v%d off (Lua: %s)", r->info.name, r->info.version, r->info.replaces);
		}
		CTimeProfiler::RegisterTimer(r->timer.c_str());   // the rule outlives the profiler's use of the name
	}
}

void Registry::EnsureStart() {
	EnsureInit();
	if (started || !inited) return;
	started = true;
	for (auto& r : rules)
		if (r->enabled) r->Start();
}

void Registry::GameFrame(int frame) {
	Registry& reg = Get();
	reg.EnsureStart();
	for (auto& r : reg.rules) {
		if (!r->enabled || !r->info.synced) continue;
		ScopedTimer t(hashString(r->timer.c_str()));
		r->GameFrame(frame);
	}
}

void Registry::DrawWorld(bool drawAboveWater) {
	if (!drawAboveWater) return;   // the below-water call of a water pass: the rules draw once, in the last (above-water) pass
	Registry& reg = Get();
	reg.EnsureStart();
	for (auto& r : reg.rules) {
		if (!r->enabled || r->info.synced) continue;
		ScopedTimer t(hashString(r->timer.c_str()));
		r->DrawWorld();
	}
}

// Spring.KT1.Rules()
static int LuaRules(lua_State* L) {
	Registry& reg = Registry::Get();
	lua_newtable(L);
	int i = 0;
	for (const auto& r : reg.All()) {
		lua_newtable(L);
		lua_pushstring(L, r->Info().name);       lua_setfield(L, -2, "name");
		lua_pushnumber(L, r->Info().version);    lua_setfield(L, -2, "version");
		lua_pushboolean(L, r->Info().synced);    lua_setfield(L, -2, "synced");
		lua_pushstring(L, r->Info().replaces);   lua_setfield(L, -2, "replaces");
		lua_pushboolean(L, r->Enabled());        lua_setfield(L, -2, "enabled");
		lua_pushstring(L, r->OptionKey().c_str()); lua_setfield(L, -2, "option");
		lua_pushstring(L, r->TimerName().c_str()); lua_setfield(L, -2, "timer");
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

// Spring.KT1.IsActive(name)
static int LuaIsActive(lua_State* L) {
	const Rule* r = Registry::Get().Find(luaL_checkstring(L, 1));
	lua_pushboolean(L, r != nullptr && r->Enabled());
	return 1;
}

void Registry::PushLua(lua_State* L) {
	Registry& reg = Get();
	reg.EnsureInit();

	lua_pushliteral(L, "KT1");
	lua_newtable(L);
	LuaPushRawNamedCFunc(L, "Rules", LuaRules);
	LuaPushRawNamedCFunc(L, "IsActive", LuaIsActive);
	for (auto& r : reg.rules) {
		lua_pushstring(L, r->info.name);
		lua_newtable(L);
		r->PushLua(L);   // the rule's functions: present when it is off too, so callers may ask it for state (each checks Enabled)
		lua_rawset(L, -3);
	}
	FrameCount::PushLua(L);   // Spring.KT1.FrameCount (dev counters, unsynced)
	lua_rawset(L, -3);
}

} // namespace KT1
