/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: every engine rule, one line each, in the order they run each frame (game-build docs/engine/rules/INDEX.md).
// Adding a rule: its folder under Rules/, one line here, its source in rts/Game/CMakeLists.txt, its row in INDEX.md.

#include "KT1Registry.h"

namespace KT1 {
	std::unique_ptr<Rule> MakeCensus();   // Rules/Census: the demo rule (framework proof)

	void AddAllRules(Registry& reg) {
		reg.Add(MakeCensus());
	}
}
