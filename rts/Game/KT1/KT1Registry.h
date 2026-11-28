/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the engine rules (each registers itself: KT1Register.h), their switches, configs, timers and the Lua namespace Spring.KT1.
// The only KT1 header stock files include. Each call below is one hook line in a stock file.

#ifndef KT1_REGISTRY_H
#define KT1_REGISTRY_H

#include <memory>
#include <vector>

#include "KT1Rule.h"
#include "Core/SimHost/StartGate.h"
#include "Prof/KT1FrameCount.h" // KT1 hook: FrameCount (dev frame counters; stock files call KT1::FrameCount::*)

namespace KT1 {
	class Registry {
	public:
		static Registry& Get();

		// KT1 hook: Setup (CGame::Load, after the defs and the simulation are loaded, before LuaRules): switches and
		// configs, then every enabled synced rule's Start. Same moment on every machine, before any unit and any Allow call.
		static void Setup();
		// KT1 hook: registry (Game.cpp, before eventHandler.GameFrame): every enabled synced rule's frame, timed.
		static void GameFrame(int frame);
		// KT1 hook: registry (WorldDrawer, after the alpha particles above water): every enabled unsynced rule's draw, timed.
		static void DrawWorld(bool drawAboveWater);
		// KT1 hook: registry (LuaSyncedRead::PushEntries): Spring.KT1 = { Rules, IsActive, <Name> = {...} }.
		static void PushLua(lua_State* L);

		// KT1 hook: registry (CGame::CGame): the sim host is reset and every rule destroyed; the next Get() makes fresh
		// rules, and the next game reads its own switches and configs.
		static void Reset();

		Rule* Find(const char* name) const;         // by name, case-insensitive; nullptr when there is none
		// Run-time switch for an unsynced (draw-side) rule only, for the in-run draw A/B (F07 capture): a rule the game
		// switched on or to check may be turned off and on again; a synced rule, or one the game switched off, is refused.
		bool SetEnabled(const char* name, bool on);
		const std::vector<std::unique_ptr<Rule>>& All() const { return rules; }

	private:
		void EnsureInit();     // switches and configs, once a game is set up
		// once: every enabled synced rule's Start (Setup; a step only when Setup never ran); true when this call did it
		bool StartSynced();

		std::vector<std::unique_ptr<Rule>> rules;
		bool built = false, inited = false;
		Core::SimHost::StartGate starts;
		Data::Source configSource = Data::Source::Lua;   // mod option kt1_jsonconfig
	};
}

#endif
