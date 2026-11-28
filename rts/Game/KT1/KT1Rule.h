/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the base class of a game rule moved from Lua into the engine (game-build docs/engine/rules/INDEX.md).
//
// Every KT1 rule is one folder rts/Game/KT1/Rules/<Name>/ with its .cpp file, a sources.cmake fragment (KT1Register.h)
// and a header only if another KT1 file needs it.
// Stock Recoil files never include a rule; they call the registry (KT1Registry.h) from one-line hooks marked "// KT1 hook: <rule>".
//
// A rule's life, in this order:
//   1. Construct        the registry makes every rule once (each registers itself: KT1Register.h), before anything else is known.
//   2. LoadConfig       on first use in a game: gamedata/kt1/<name>.lua (the game's archive; the table it returns), or an empty table.
//                       Only for an enabled rule. Tuning lives there: no number that a designer may change is a C++ constant.
//   3. Start            at the first sim frame: unit defs are loaded; read per-def data here (KT1UnitData.h).
//   4. GameFrame(f)     every synced frame, before the Lua GameFrame, under the profiler timer "KT1::<Name>".
//   4b. DrawWorld       unsynced rules that draw: once per drawn frame, in the world's alpha pass (after the engine's own particles),
//                       under the timer "KT1::<Name>". Never changes sim state. Not called in a headless engine (no draw).
//   5. PushLua          fills the table Spring.KT1.<Name> (in every Lua handle: check CLuaHandle::GetHandleSynced for writes).
//
// Determinism (synced rules): no thread-dependent state, no unordered iteration that reaches a result, no wall clock,
// float maths in the same order on every machine. Results go to Lua through the rule's API (Spring.KT1.<Name>.*), at the rule's rate.

#ifndef KT1_RULE_H
#define KT1_RULE_H

#include <string>

class LuaTable;
struct lua_State;

namespace KT1 {
	struct RuleInfo {
		const char* name;        // "Census": the API table Spring.KT1.Census, the config gamedata/kt1/census.lua, the timer KT1::Census
		int version;             // bump when the rule's behaviour changes (logged; listed by Spring.KT1.Rules)
		bool synced;             // a synced rule runs in GameFrame and may change sim state; an unsynced one may not
		const char* replaces;    // the Lua gadget it replaced ("LuaRules/Gadgets/kt1_census.lua"); that gadget stands down when this is on
		bool defaultOn;          // the mod option's value when the game does not set it
		// the rule honours the check-mode contract (KT1Register.h); kt1_<key>=check on a rule without it stops the engine
		bool checkSafe = false;
		// the sim host shows this system intel's fake units too (Host/Fakes.h); false: it sees real units only, as Lua gadgets do
		bool seesFakes = false;
	};

	class Rule {
	public:
		explicit Rule(const RuleInfo& i);
		virtual ~Rule() = default;

		const RuleInfo& Info() const { return info; }
		const std::string& Key() const { return key; }             // "census": lower case of name
		const std::string& OptionKey() const { return option; }    // "kt1_census": the mod option that switches it
		const std::string& TimerName() const { return timer; }     // "KT1::Census"
		bool Enabled() const { return enabled; }
		// the switch as the game set it: off, on, or check (LUA-FREE §3.2: the rule runs and its Lua is not stood down,
		// so both paths run and the proof compares them). Fixed for the run; Enabled() is what runs now.
		enum class Mode { Off, On, Check };
		Mode SwitchMode() const { return mode; }
		static const char* ModeName(Mode m) { return m == Mode::On ? "on" : (m == Mode::Check ? "check" : "off"); }

		virtual void LoadConfig(const LuaTable& cfg) {}
		virtual void Start() {}
		virtual void GameFrame(int frame) {}
		virtual void DrawWorld() {}
		virtual void PushLua(lua_State* L) {}

	private:
		friend class Registry;
		RuleInfo info;
		std::string key, option, timer;
		bool enabled = false;
		Mode mode = Mode::Off;
	};
}

#endif
