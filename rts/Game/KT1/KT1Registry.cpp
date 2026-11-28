/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the rule registry (game-build docs/engine/rules/INDEX.md). See KT1Rule.h for a rule's life.
//
// Switch: mod option kt1_<key> ("1"/"true" on, "0"/"false" off, "check"; unset: the rule's defaultOn). Off: the engine rule
// does nothing and its Lua gadget runs (the gadget asks Spring.KT1.IsActive("<Name>") and stands down when it is true).
// Check (LUA-FREE §3.2): the engine rule runs and IsActive is false, so its Lua runs too and a proof compares the two.
// Config: read once per game when the rule is on, from the file mod option kt1_jsonconfig names (Data/Check.h):
// gamedata/kt1/<key>.lua ("0", unset) or data/kt1/<key>.json ("1"; then an unsynced rule's file is read again on /kt1
// reload and, in a dev archive, when it changes). "check" reads both for every rule, logs [KT1CHECK] JsonConfig and runs
// on the Lua values.
//
// Lua (every handle, from LuaSyncedRead):
//   Spring.KT1.Rules()        -> { {name, version, synced, replaces, enabled, option, timer}, ... } in registry order
//   Spring.KT1.IsActive(name) -> true when the engine rule exists and is on (not in check mode: its Lua keeps running)
//   Spring.KT1.Enabled(name)  -> true while the engine rule runs (on or check, and not turned off by SetEnabled)
//   Spring.KT1.Mode(name)     -> "off" | "on" | "check", the switch as the game set it; nil when there is no such rule
//   Spring.KT1.SetEnabled(name, on) -> unsynced Lua only, unsynced rules only (capture A/B); true when done
//   Spring.KT1.<Name>         the rule's own table (its PushLua)

#include "KT1Registry.h"
#include "KT1Register.h"
#include "KT1Systems.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "Core/Common/Rate.h"
#include "Data/DataEngine.h"
#include "Data/Number.h"
#include "Game/GameSetup.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Sim/Misc/GlobalConstants.h"
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

Data::Report Rule::ReadConfig(std::string_view text, Data::Strictness strictness) {
	struct NoKeys { void Describe(Data::Schema&) {} } none;
	return Data::ParseInto(text, strictness, none);
}

namespace {
	struct Registration { const char* name; int order; MakeRuleFn make; };
	std::vector<Registration>& Pending() { static std::vector<Registration> v; return v; }
}

void RegisterSystem(const char* name, int order, MakeRuleFn make) {
	Pending().push_back({name, order, make});
}

Registry& Registry::Get() {
	static Registry reg;
	if (!reg.built) {
		reg.built = true;
		RegisterAllSystems();   // generated from every sources.cmake fragment
		auto& p = Pending();
		std::sort(p.begin(), p.end(), [](const Registration& a, const Registration& b) { return a.order < b.order; });
		for (size_t i = 0; i < p.size(); ++i) {
			for (size_t j = 0; j < i; ++j) {
				if (p[j].order == p[i].order || Lower(p[j].name) == Lower(p[i].name)) {
					LOG_L(L_FATAL, "[KT1] systems %s and %s share a run order (%d) or a name: each needs its own (KT1Register.h)", p[j].name, p[i].name, p[i].order);
					std::abort();
				}
			}
			reg.rules.push_back(p[i].make());
		}
		p.clear();
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
	Core::Rate::SetStepsPerSecond(GAME_SPEED);   // before any system turns seconds into steps

	const auto& opts = CGameSetup::GetModOptions();
	const auto source = opts.find("kt1_jsonconfig");
	configSource = Data::ParseSource(source == opts.end() ? std::string() : std::string(source->second.c_str()));
	LOG("[KT1] config files: kt1_jsonconfig=%s", Data::SourceName(configSource));
	Data::ConfigCheck check;
	for (auto& r : rules) {
		Rule::Mode mode = r->info.defaultOn ? Rule::Mode::On : Rule::Mode::Off;
		const auto it = opts.find(r->option);
		if (it != opts.end()) {
			const std::string v = Lower(it->second.c_str());
			mode = (v == "1" || v == "true") ? Rule::Mode::On : (v == "check" ? Rule::Mode::Check : Rule::Mode::Off);
		}
		r->mode = mode;
		const bool on = (mode != Rule::Mode::Off);
		r->enabled = on;

		if (configSource == Data::Source::Check) Data::CheckRule(*r, check);
		if (on) {
			Data::LoadRule(*r, configSource, Rule::ModeName(mode));
		} else {
			LOG("[KT1] %s v%d off (Lua: %s) mode=off", r->info.name, r->info.version, r->info.replaces);
		}
		CTimeProfiler::RegisterTimer(r->timer.c_str());   // the rule outlives the profiler's use of the name
	}
	if (configSource == Data::Source::Check) Data::LogCheck(check);
}

void Registry::EnsureStart() {
	EnsureInit();
	if (started || !inited) return;
	started = true;
	if (configSource == Data::Source::Json) Data::StartReload();
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

bool Registry::SetEnabled(const char* name, bool on) {
	Rule* r = Find(name);
	if (r == nullptr || r->info.synced || r->mode == Rule::Mode::Off)
		return false;   // a synced switch never changes in a run; an off rule never loaded its config or started
	r->enabled = on;
	LOG("[KT1] %s %s at run time (capture)", r->info.name, on ? "enabled" : "disabled");
	return true;
}

// Spring.KT1.IsActive(name): the engine rule replaces its Lua (on; not check, where both run)
static int LuaIsActive(lua_State* L) {
	const Rule* r = Registry::Get().Find(luaL_checkstring(L, 1));
	lua_pushboolean(L, r != nullptr && r->Enabled() && r->SwitchMode() == Rule::Mode::On);
	return 1;
}

// Spring.KT1.Enabled(name): the engine rule runs now
static int LuaEnabled(lua_State* L) {
	const Rule* r = Registry::Get().Find(luaL_checkstring(L, 1));
	lua_pushboolean(L, r != nullptr && r->Enabled());
	return 1;
}

// Spring.KT1.Mode(name)
static int LuaMode(lua_State* L) {
	const Rule* r = Registry::Get().Find(luaL_checkstring(L, 1));
	if (r == nullptr) return 0;
	lua_pushstring(L, Rule::ModeName(r->SwitchMode()));
	return 1;
}

// Spring.KT1.ReadNumber(text): the number the data loader gives a float field for this text; nothing when it refuses it
static int LuaReadNumber(lua_State* L) {
	std::size_t length = 0;
	const char* text = luaL_checklstring(L, 1, &length);
	float value = 0.0f;
	if (Data::ReadNumber(std::string_view(text, length), value) != Data::NumberError::Ok) return 0;
	lua_pushnumber(L, value);
	return 1;
}

// Spring.KT1.SetEnabled(name, on): refused from synced Lua and for synced rules
static int LuaSetEnabled(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) {
		lua_pushboolean(L, false);
		return 1;
	}
	lua_pushboolean(L, Registry::Get().SetEnabled(luaL_checkstring(L, 1), (luaL_checktype(L, 2, LUA_TBOOLEAN), lua_toboolean(L, 2) != 0)));
	return 1;
}

void Registry::PushLua(lua_State* L) {
	Registry& reg = Get();
	reg.EnsureInit();

	lua_pushliteral(L, "KT1");
	lua_newtable(L);
	LuaPushRawNamedCFunc(L, "Rules", LuaRules);
	LuaPushRawNamedCFunc(L, "IsActive", LuaIsActive);
	LuaPushRawNamedCFunc(L, "Enabled", LuaEnabled);
	LuaPushRawNamedCFunc(L, "Mode", LuaMode);
	LuaPushRawNamedCFunc(L, "SetEnabled", LuaSetEnabled);
	LuaPushRawNamedCFunc(L, "ReadNumber", LuaReadNumber);
	for (auto& r : reg.rules) {
		lua_pushstring(L, r->info.name);
		lua_newtable(L);
		r->PushLua(L);   // the rule's functions: present when it is off too, so callers may ask it for state (each checks Enabled)
		lua_rawset(L, -3);
	}
	lua_rawset(L, -3);
}

} // namespace KT1
