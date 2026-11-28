/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FrameCount: see KT1FrameCount.h. One "[FRAMECOUNT]" line per window, fields (game-build docs/engine/FRAMECOUNT.md):
//   v=1 tag=<cfg|Start's tag> why=<frames|lua|exit> gf=<first>-<last> wall_s drawn steps fps sps
//   spf=0:a,1:b,2:c,3+:d              drawn frames by the number of sim steps run since the previous drawn frame
//   ft_ms=p50/p95/p99/max             drawn-frame time (draw entry to draw entry), all drawn frames
//   ft_edges=8,12,16.7,25,33,50,100,250 and ft_step=<9 counts> ft_nostep=<9 counts>: the same times in buckets, split into
//                                     frames with at least one step and frames with none
//   step_ms=mean/p50/p95/max          one SimFrame, wall
//   sys=<timer>:<mean ms per step>,...  a fixed list of engine profiler timers (SIM_TIMERS), names verbatim
//   draw=<timer>:<mean ms per drawn frame>,...  a fixed list (DRAW_TIMERS)
//   new_n, new_kb=mean/p95/max        C++ operator new calls and KiB per drawn frame (all threads; includes Lua pool misses)
//   lua_n=mean/p95/max                Lua allocator calls per drawn frame (all handles); frames where the engine clears the count are left out
// The counters force the engine profiler to record (as SupComProfiler=1) so sys= and draw= have data.

#include "KT1FrameCount.h"
#include "KT1FrameCountAlloc.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "Lua/LuaAllocState.h"
#include "lib/lua/include/LuaInclude.h"
#include "Lua/LuaUtils.h"
#include "lib/lua/include/LuaUser.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"
#include "System/SupComProfiler.h"
#include "System/TimeProfiler.h"

CONFIG(int, KT1FrameCount).defaultValue(0).description("KT1: 1 counts steps per drawn frame, step cost per system, allocations per frame and a frame-time histogram, printed as one [FRAMECOUNT] infolog line (dev only; 0 off).");
CONFIG(std::string, KT1FrameCountFrames).defaultValue("").description("KT1: with KT1FrameCount=1, the game-frame window a-b to count; empty counts the whole game.");

namespace KT1 {
namespace FrameCount {

namespace {
	struct State {
		bool cfgRead = false, on = false, autoDone = false, active = false, haveLastDraw = false;
		int from = 0, to = -1, firstFrame = -1, lastFrame = -1;
		int stepsSinceDraw = 0;
		uint64_t steps = 0;
		spring_time t0, lastDraw, simT0;
		uint64_t prevN = 0, prevB = 0, prevLua = 0;
		std::vector<float> frameMs, stepMs;
		std::vector<uint8_t> frameSteps;
		std::vector<uint32_t> allocN, allocKB, luaN;
		std::vector<std::pair<std::string, int64_t>> profStart;
		std::string tag = "cfg";
	} st;

	constexpr float EDGES[] = {8.0f, 12.0f, 16.7f, 25.0f, 33.0f, 50.0f, 100.0f, 250.0f};
	constexpr int NB = 9;

	void ReadConfig() {
		if (st.cfgRead || configHandler == nullptr) return;
		st.cfgRead = true;
		st.on = (configHandler->GetInt("KT1FrameCount") != 0);
		const std::string w = configHandler->GetString("KT1FrameCountFrames");
		int a = 0, b = -1;
		if (!w.empty() && std::sscanf(w.c_str(), "%d-%d", &a, &b) >= 1) { st.from = a; st.to = b; }
		if (st.on) LOG("[FRAMECOUNT] on: window %s", w.empty()? "whole game": w.c_str());
	}

	uint64_t LuaAllocs() {
		SLuaAllocState s = {{0}, {0}, {0}, {0}};
		spring_lua_alloc_get_stats(&s);
		return s.numLuaAllocs.load();
	}

	// fixed lists, profiler names verbatim, in PERF-BENCHMARK / FRAME-BUDGET order (engine-fork-2, 2026-11-27); a timer that never ran prints 0
	const char* const SIM_TIMERS[] = {   // sys=: mean ms per sim step
		"Sim", "Sim::GameFrame", "Lua::Callins::Synced", "Lua::CollectGarbage::Synced", "Sim::Unit::MoveType", "Sim::Los",
		"Sim::Unit::Weapon", "Sim::Projectiles", "Sim::Projectiles::Collisions", "Sim::Unit::SlowUpdate", "Sim::Script",
		"Sim::Unit::Update", "Sim::Features::UpdatePreFrame", "Sim::PathRequests", "Sim::PathUpdates",
	};
	const char* const DRAW_TIMERS[] = {  // draw=: mean ms per drawn frame
		"Update", "Draw", "Draw::World::CreateShadows", "Draw::World::Decals", "Draw::Screen", "Draw::Screen::DrawScreen",
		"Update::WorldDrawer", "CUnitDrawerBase::Update", "Misc::SwapBuffers", "Lua::Callins::Unsynced", "Lua::CollectGarbage::Unsynced",
	};

	std::vector<std::pair<std::string, int64_t>> ProfSnapshot() {
		std::vector<std::pair<std::string, int64_t>> v;
		for (const auto& p: CTimeProfiler::GetInstance().GetSortedProfiles())
			v.emplace_back(p.first, p.second.total.toNanoSecsi());
		return v;
	}

	int64_t Find(const std::vector<std::pair<std::string, int64_t>>& v, const char* n) {
		for (const auto& p: v) if (p.first == n) return p.second;
		return 0;
	}

	void Open(const char* tag) {
		const int f = gs->frameNum;
		const bool on = st.on, autoDone = st.autoDone; const int from = st.from, to = st.to;
		st = State();
		st.cfgRead = true; st.on = on; st.autoDone = autoDone; st.from = from; st.to = to;
		st.active = true; st.firstFrame = f; st.lastFrame = f;
		st.tag = (tag != nullptr && tag[0] != 0)? tag: "cfg";
		for (char& c: st.tag) if (c == ' ' || c == '\t' || c == '\n' || c == '\r') c = '_';
		SupComProfiler::state = 1;   // sys= needs the profiler recording; timers do not touch the simulation
		KT1FrameCountAlloc::on.store(true, std::memory_order_relaxed);
		st.t0 = spring_gettime();
		st.profStart = ProfSnapshot();
		st.frameMs.reserve(1 << 14); st.frameSteps.reserve(1 << 14); st.stepMs.reserve(1 << 14);
	}

	template<typename T> T Pct(std::vector<T> v, float q) {
		if (v.empty()) return T(0);
		const size_t k = std::min(v.size() - 1, size_t(q * (v.size() - 1) + 0.5f));
		std::nth_element(v.begin(), v.begin() + k, v.end());
		return v[k];
	}
	template<typename T> double Mean(const std::vector<T>& v) {
		double s = 0.0; for (T x: v) s += x; return v.empty()? 0.0: s / v.size();
	}
	template<typename T> T Max(const std::vector<T>& v) { return v.empty()? T(0): *std::max_element(v.begin(), v.end()); }

	void Close(const char* why) {
		if (!st.active) return;
		st.active = false;
		KT1FrameCountAlloc::on.store(false, std::memory_order_relaxed);

		const double wall = (spring_gettime() - st.t0).toSecsf();
		const size_t drawn = st.frameMs.size();
		char b[256];
		std::string s = "[FRAMECOUNT] v=1 tag=" + st.tag;
		std::snprintf(b, sizeof(b), " why=%s gf=%d-%d wall_s=%.2f drawn=%u steps=%u fps=%.1f sps=%.1f", why, st.firstFrame, st.lastFrame,
			wall, unsigned(drawn), unsigned(st.steps), wall > 0? drawn / wall: 0.0, wall > 0? st.steps / wall: 0.0);
		s += b;

		unsigned spf[4] = {0, 0, 0, 0};
		unsigned hs[NB] = {}, h0[NB] = {};
		for (size_t i = 0; i < drawn; ++i) {
			const int k = std::min<int>(st.frameSteps[i], 3);
			++spf[k];
			int j = 0; while (j < NB - 1 && st.frameMs[i] >= EDGES[j]) ++j;
			++(st.frameSteps[i] > 0? hs[j]: h0[j]);
		}
		std::snprintf(b, sizeof(b), " spf=0:%u,1:%u,2:%u,3+:%u", spf[0], spf[1], spf[2], spf[3]); s += b;
		std::snprintf(b, sizeof(b), " ft_ms=%.1f/%.1f/%.1f/%.1f ft_edges=8,12,16.7,25,33,50,100,250", Pct(st.frameMs, 0.5f), Pct(st.frameMs, 0.95f), Pct(st.frameMs, 0.99f), Max(st.frameMs)); s += b;
		s += " ft_step=";   for (int j = 0; j < NB; ++j) { std::snprintf(b, sizeof(b), j? ",%u": "%u", hs[j]); s += b; }
		s += " ft_nostep="; for (int j = 0; j < NB; ++j) { std::snprintf(b, sizeof(b), j? ",%u": "%u", h0[j]); s += b; }
		std::snprintf(b, sizeof(b), " step_ms=%.2f/%.2f/%.2f/%.2f", Mean(st.stepMs), Pct(st.stepMs, 0.5f), Pct(st.stepMs, 0.95f), Max(st.stepMs)); s += b;

		// profiler deltas over the window, fixed lists
		const auto now = ProfSnapshot();
		s += " sys=";
		for (size_t i = 0; i < sizeof(SIM_TIMERS) / sizeof(SIM_TIMERS[0]); ++i) {
			const double ms = (Find(now, SIM_TIMERS[i]) - Find(st.profStart, SIM_TIMERS[i])) * 1e-6;
			std::snprintf(b, sizeof(b), "%s%s:%.2f", i? ",": "", SIM_TIMERS[i], st.steps? ms / st.steps: 0.0); s += b;
		}
		s += " draw=";
		for (size_t i = 0; i < sizeof(DRAW_TIMERS) / sizeof(DRAW_TIMERS[0]); ++i) {
			const double ms = (Find(now, DRAW_TIMERS[i]) - Find(st.profStart, DRAW_TIMERS[i])) * 1e-6;
			std::snprintf(b, sizeof(b), "%s%s:%.2f", i? ",": "", DRAW_TIMERS[i], drawn? ms / drawn: 0.0); s += b;
		}

		std::snprintf(b, sizeof(b), " new_n=%.0f/%u/%u new_kb=%.0f/%u/%u lua_n=%.0f/%u/%u",
			Mean(st.allocN), Pct(st.allocN, 0.95f), Max(st.allocN), Mean(st.allocKB), Pct(st.allocKB, 0.95f), Max(st.allocKB),
			Mean(st.luaN), Pct(st.luaN, 0.95f), Max(st.luaN)); s += b;

		LOG("%s", s.c_str());
	}
}

void SimBegin() {
	ReadConfig();
	if (!st.on) return;
	const int f = gs->frameNum;
	if (!st.active && !st.autoDone && f >= st.from && (st.to < 0 || f <= st.to)) { st.autoDone = true; Open("cfg"); }
	if (st.active) st.simT0 = spring_gettime();
}

void SimEnd() {
	if (!st.active) return;
	st.stepMs.push_back((spring_gettime() - st.simT0).toMilliSecsf());
	st.lastFrame = gs->frameNum;
	++st.stepsSinceDraw;
	++st.steps;
	if (st.to >= 0 && gs->frameNum >= st.to) Close("frames");
}

void DrawFrame() {
	if (!st.active) return;
	const spring_time now = spring_gettime();
	uint64_t n, bytes; KT1FrameCountAlloc::Totals(n, bytes);
	const uint64_t lua = LuaAllocs();
	if (st.haveLastDraw) {
		st.frameMs.push_back((now - st.lastDraw).toMilliSecsf());
		st.frameSteps.push_back(uint8_t(std::min(st.stepsSinceDraw, 255)));
		st.allocN.push_back(uint32_t(n - st.prevN));
		st.allocKB.push_back(uint32_t((bytes - st.prevB) >> 10));
		// the global Lua count is cleared now and then (spring_lua_alloc_update_stats); skip the frame it drops
		if (lua >= st.prevLua) st.luaN.push_back(uint32_t(lua - st.prevLua));
	}
	st.prevN = n; st.prevB = bytes; st.prevLua = lua;
	st.stepsSinceDraw = 0;
	st.lastDraw = now;
	st.haveLastDraw = true;
}

void Finish(const char* why) { Close(why); }

namespace {
	int LuaStart(lua_State* L) { ReadConfig(); if (st.on) { Close("lua"); st.autoDone = true; Open(lua_isstring(L, 1)? lua_tostring(L, 1): "lua"); } return 0; }
	int LuaStop(lua_State* L) { Close("lua"); return 0; }
	int LuaIsOn(lua_State* L) { ReadConfig(); lua_pushboolean(L, st.on); return 1; }
}

void PushLua(lua_State* L) {
	lua_pushliteral(L, "FrameCount");
	lua_newtable(L);
	LuaPushRawNamedCFunc(L, "Start", LuaStart);
	LuaPushRawNamedCFunc(L, "Stop", LuaStop);
	LuaPushRawNamedCFunc(L, "IsOn", LuaIsOn);
	lua_rawset(L, -3);
}

} // namespace FrameCount
} // namespace KT1
