/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: every engine rule, one line each, in the order they run each frame (game-build docs/engine/rules/INDEX.md).
// Adding a rule: its folder under Rules/, one line here, its source in rts/Game/CMakeLists.txt, its row in INDEX.md.

#include "KT1Registry.h"

namespace KT1 {
	std::unique_ptr<Rule> MakeCensus();   // Rules/Census: the demo rule (framework proof)
	std::unique_ptr<Rule> MakeOrders();   // Rules/Orders: the orders gadget's unit lists (TrackMoves walk, safety net)
	std::unique_ptr<Rule> MakeRibbons();  // UI/Ribbons: shot ribbons drawn by the engine (unsynced)
	std::unique_ptr<Rule> MakeOwnPass();  // Rules/OwnPass: mov_own_pass walk and light-unit search (synced)
	std::unique_ptr<Rule> MakePlants();   // Rules/Plants: the contact search of mov_trees and map_plants (synced)
	std::unique_ptr<Rule> MakeFuel();     // Rules/Fuel: the air staging fuel step (synced)
	std::unique_ptr<Rule> MakeBuilders(); // Rules/Builders: the construction economy busy-builder read (synced)
	std::unique_ptr<Rule> MakeIconCull();  // UI/IconCull: engine icon passes skip icons that draw nothing (unsynced)
	std::unique_ptr<Rule> MakeFarView();   // UI/FarView: far above the map, a baked map mesh instead of the full world pass (unsynced)

	void AddAllRules(Registry& reg) {
		reg.Add(MakeCensus());
		reg.Add(MakeOrders());
		reg.Add(MakeRibbons());
		reg.Add(MakeOwnPass());
		reg.Add(MakePlants());
		reg.Add(MakeFuel());
		reg.Add(MakeBuilders());
		reg.Add(MakeIconCull());
		reg.Add(MakeFarView());
	}
}
