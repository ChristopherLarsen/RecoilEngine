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
// Run order: the frame order of docs/engine/rules/INDEX.md, in steps of 100 so a new system fits between two.

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
