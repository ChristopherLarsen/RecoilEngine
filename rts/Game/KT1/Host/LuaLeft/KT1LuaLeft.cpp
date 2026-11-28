/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 system: LuaLeft (version 1). How much Lua is left, in every run's infolog (LUA-FREE-WORKPACKAGES F08).
//
// Replaces:   nothing stands down; it gives the totals of perf_sprof / perf_wprof without their cost.
// Switch:     mod option kt1_lualeft (default off until the pin flips it, plan 3.3; 0 = no timing, no lines)
// Config:     none read yet; the report period is Core Config::reportPeriodS (DESIGN 10 s of game time).
//             A file comes with F04's ReadConfig, so this file builds with or without F04.
// Core:       Core/LuaLeft (the adding up, engine-free; tests Test/LuaLeft)
// Hooks:      CLuaHandle::RunCallInTraceback (every call into Lua) and CLuaHandle::CollectGarbage
// Timer:      KT1::LuaLeft (the report only)
// Output:     two [LUALEFT] lines per window and a last, shorter window at game end
// Counts:     self time (a nested call-in is not counted twice); main thread, from sim step 1 on.
//             Not counted: Lua run outside CLuaHandle call-ins (RmlUi event listeners, LuaParser, file loading).
// Determinism: unsynced. Reads wall clock, handle names, allocation counters and the length of two Lua tables
//             (rawget only: no metamethod, no Lua code runs); writes nothing the sim reads.

#include "KT1LuaLeft.h"

#include <algorithm>
#include <cstring>

#include "Game/KT1/Core/LuaLeft/LuaLeftCore.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaHashString.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"
#include "System/Platform/Threading.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"
#include "System/UnorderedSet.hpp"

extern const spring::unsynced_set<const luaContextData*>* LUAHANDLE_CONTEXTS[2];   // [0] unsynced, [1] synced (LuaHandle.cpp)

namespace KT1 { namespace LuaLeft {

namespace {
	constexpr const char* UNIT_SCRIPT = "UnitScript";   // a unit script function: the engine passes no name
	constexpr int LINE_SIZE = 2048;                     // DESIGN: holds ten rows of two 63-character names

	Meter meter;
	const Rule* self = nullptr;   // the registry's one LuaLeft rule: its switch may change in a run (Spring.KT1.SetEnabled)
	int periodSteps = 0;

	// seconds of game time as sim steps (becomes Core::Rate::Every with F02)
	int StepsFor(float seconds) { return std::max(1, int(seconds * GAME_SPEED + 0.5f)); }   // DESIGN: to the nearest step

	int64_t NowNs() { return spring_gettime().toNanoSecsi(); }

	// Counting starts at sim step 1 (loading is not "Lua left") and only on the main thread (the meter has no lock).
	bool Counting() {
		if (self == nullptr || gs == nullptr || gs->frameNum <= 0 || !Threading::IsMainThread()) return false;
		if (self->Enabled()) return true;
		if (meter.Idle()) meter.Stop();   // a window must not span the time the meter was off
		return false;
	}

	int DrawFrame() { return (globalRendering != nullptr) ? int(globalRendering->drawFrame) : 0; }

	void SampleAllContexts() {
		for (const Side side : {UNSYNCED, SYNCED}) {
			for (const luaContextData* lcd : *LUAHANDLE_CONTEXTS[side])
				meter.SampleAllocs(lcd, side, lcd->allocState.numLuaAllocs.load(std::memory_order_relaxed));
		}
	}

	void WriteReport() {
		static const unsigned timer = hashString("KT1::LuaLeft");
		ScopedTimer t(timer);
		SampleAllContexts();
		const Report r = meter.Close(gs->frameNum, DrawFrame(), periodSteps);
		char line[LINE_SIZE];
		FormatSummary(r, line, sizeof(line));
		LOG("%s", line);
		FormatTop(r, line, sizeof(line));
		LOG("%s", line);
	}

	// Between call-ins only: opens the first window, or reports a finished one.
	void Turn() {
		if (!meter.Idle()) return;
		if (meter.NeedsStart(gs->frameNum)) {
			SampleAllContexts();
			meter.StartWindow(gs->frameNum, DrawFrame());
		} else if (meter.Due(gs->frameNum, periodSteps)) {
			WriteReport();
		}
	}

	// #_G[table][field], read raw: 0 when either is not a table.
	int ListLength(lua_State* L, const char* table, const char* field) {
		int n = 0;
		lua_pushstring(L, table);
		lua_rawget(L, LUA_GLOBALSINDEX);
		if (lua_istable(L, -1)) {
			lua_pushstring(L, field);
			lua_rawget(L, -2);
			if (lua_istable(L, -1)) n = int(lua_objlen(L, -1));
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		return n;
	}

	void ReadCounts(int state, lua_State* L) {
		if (!lua_checkstack(L, 2)) return;
		meter.SetCounts(state, ListLength(L, "gadgetHandler", "gadgets"), ListLength(L, "widgetHandler", "widgets"));
	}
}

CallScope::CallScope(const CLuaHandle* handle, lua_State* L, const LuaHashString* hs) {
	if (!Counting()) return;
	on = true;
	Turn();
	const bool synced = CLuaHandle::GetHandleSynced(L);
	const Side side = synced ? SYNCED : UNSYNCED;
	const char* callIn = (hs != nullptr) ? hs->GetString() : UNIT_SCRIPT;
	const uint32_t hash = (hs != nullptr) ? hs->GetHash() : hashString(UNIT_SCRIPT);
	const int state = meter.State(handle->GetName().c_str(), side);
	if (meter.CountsWanted(state)) ReadCounts(state, L);
	// game over, or the synced state going away: report what the window holds
	endsGame = (std::strcmp(callIn, "GameOver") == 0) || (synced && std::strcmp(callIn, "Shutdown") == 0);
	meter.Enter(meter.Row(state, hash, callIn), side, NowNs(), gs->frameNum);   // last: the work above is not timed
}

CallScope::~CallScope() {
	if (!on) return;
	meter.Leave(NowNs());
	if (endsGame && meter.Idle() && meter.StepsInWindow(gs->frameNum) > 0) WriteReport();
}

GcScope::GcScope(const lua_State* L) {
	if (!Counting()) return;
	on = true;
	meter.EnterGc(CLuaHandle::GetHandleSynced(L) ? SYNCED : UNSYNCED, NowNs());
}

GcScope::~GcScope() {
	if (on) meter.Leave(NowNs());
}

namespace {
	class LuaLeftRule : public Rule {
	public:
		LuaLeftRule() : Rule({"LuaLeft", 1, false, "perf_sprof / perf_wprof totals (they stay)", false, true}) {   // check-safe: it only measures
			self = this;
			periodSteps = StepsFor(float(Config{}.reportPeriodS));
		}
	};
}

}} // namespace KT1::LuaLeft

namespace KT1 {
	KT1_REGISTER_SYSTEM(LuaLeft, 5000, LuaLeft::LuaLeftRule)
}
