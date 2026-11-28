/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the list of engine rules, their switches, configs, timers and the Lua namespace Spring.KT1.
// The only KT1 header stock files include. Each call below is one hook line in a stock file.

#ifndef KT1_REGISTRY_H
#define KT1_REGISTRY_H

#include <memory>
#include <vector>

#include "KT1Rule.h"
#include "Prof/KT1FrameCount.h" // KT1 hook: FrameCount (dev frame counters; stock files call KT1::FrameCount::*)

namespace KT1 {
	class Registry {
	public:
		static Registry& Get();

		// KT1 hook: registry (Game.cpp, before eventHandler.GameFrame): every enabled synced rule's frame, timed.
		static void GameFrame(int frame);
		// KT1 hook: registry (WorldDrawer, after the alpha particles above water): every enabled unsynced rule's draw, timed.
		static void DrawWorld(bool drawAboveWater);
		// KT1 hook: registry (LuaSyncedRead::PushEntries): Spring.KT1 = { Rules, IsActive, <Name> = {...} }.
		static void PushLua(lua_State* L);

		Rule* Find(const char* name) const;         // by name, case-insensitive; nullptr when there is none
		const std::vector<std::unique_ptr<Rule>>& All() const { return rules; }

		// Used by KT1RuleList.cpp only.
		void Add(std::unique_ptr<Rule> r) { rules.push_back(std::move(r)); }

	private:
		void EnsureInit();     // switches and configs, once a game is set up
		void EnsureStart();    // per-def data, at the first frame

		std::vector<std::unique_ptr<Rule>> rules;
		bool built = false, inited = false, started = false;
	};

	void AddAllRules(Registry& reg);   // KT1RuleList.cpp: one line per rule
}

#endif
