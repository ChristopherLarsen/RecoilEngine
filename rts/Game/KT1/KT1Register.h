/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: how a system registers itself (LUA-FREE-WORKPACKAGES F01). No shared list names the systems.
//
// In the system's own .cpp, after its class, inside namespace KT1:
//     KT1_REGISTER_SYSTEM(Census, 100, Census)        // name, run order, the Rule subclass (default-constructible)
// and in the system's own folder a sources.cmake fragment:
//     kt1_system(Census KT1Census.cpp)                // the names it registers, then its sources
//
// The build generates one file (kt1_systems.gen.cpp, from every fragment) that calls each system's registrar, so a
// static library cannot drop a system's object file. The registry then runs the systems in ascending run order; two
// systems with one order, or one name twice, stop the engine at start (a build error in all but name).
// Run order (Order): synced systems run in ascending Order in GameFrame, unsynced ones in ascending Order in DrawWorld;
// the two lists are separate, so only the order among synced systems is part of sync (changing it changes NETHASH).
// A new system takes the next free multiple of 100 in its kind's range (or of 10 inside a phase); a clash stops the
// engine at start with both names, so pick from this table, not by feel:
//     1 -    99   host and tooling that must run before every rule (F05 phases, F07 proof hooks)
//   100 -  1999   the rules before LUA-FREE (Census 100 .. UnitAnim 1500, synced and unsynced mixed): fixed, take none
//  2000 -  2999   synced rules, phase "before move" (orders, intents)
//  3000 -  3999   synced rules, phase "move and contact" (movement, passes, plants)
//  4000 -  4999   synced rules, phase "after move" (combat, economy, intel, animation)
//  5000 -  5999   meters and dev counters (F08 LuaLeft 5000)
//  6000 -  7999   draw systems (Draw/)
//  8000 -  8499   interface (UI/)
//  8500 -  8999   audio
//  9000 -  9999   probes and test systems (F06 Draw/Probe 9000)

// One switch belongs to the framework, not to a system: mod option kt1_jsonconfig (0, 1, check; default 0; Data/Check.h)
// picks the config files every system reads. Its check mode is not a system's check mode: nothing runs twice in the game
// and it writes nothing, it only reads both files at start and compares the values.

#ifndef KT1_REGISTER_H
#define KT1_REGISTER_H

#include <memory>

#include "KT1Rule.h"

namespace KT1 {
	using MakeRuleFn = std::unique_ptr<Rule> (*)();
	// Records one system (called by the generated file, before the registry is first used).
	void RegisterSystem(const char* name, int order, MakeRuleFn make);
}

// Used inside namespace KT1 (where every system's .cpp already is); the generated file declares and calls
// KT1::RegisterSystem_<Name>().
#define KT1_REGISTER_SYSTEM(Name, Order, Type) \
	void RegisterSystem_##Name(); \
	void RegisterSystem_##Name() { \
		RegisterSystem(#Name, Order, []() -> std::unique_ptr<Rule> { return std::make_unique<Type>(); }); \
	}

#endif
