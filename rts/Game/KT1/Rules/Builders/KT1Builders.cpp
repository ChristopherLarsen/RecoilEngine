/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Builders (version 1). The construction economy's builder walk (ECO-004, ECO-006, ECO-007, ECO-012): which builders have
// work this frame, read in the engine in one call instead of two engine reads per builder (about 370 builders, most of them idle).
//
// Replaces:   the idle-builder test of LuaRules/Gadgets/game_eco.lua's GameFrame (Spring.GetUnitIsBuilding and
//             Spring.GetUnitCommands(b, 0) for every builder, in SortedPairs order); the gadget stays and walks Busy()'s list
//             instead; everything it does with a busy builder (targets, set-up, holds, requests, settlement) is unchanged
// Switch:     mod option kt1_builders (default off)
// Config:     gamedata/kt1/builders.lua (no tuning values; the file says so)
// Unit data:  which unit defs count as factories: pushed once by the gadget (SetFactoryDefs), from its own isFactoryDef
// Timer:      KT1::Builders (each Busy() call)
// Lua:        Spring.KT1.Builders.Busy() -> { unitID, ... } ascending: every builder or factory unit (a CBuilder or CFactory) that is
//               building something, is a factory def, or has a command in its queue; read at the call (synced only)
//             Spring.KT1.Builders.SetFactoryDefs({ defID, ... })   synced only
// Same as the Lua: the gadget visited every builder in ascending id (SortedPairs) and skipped one with no target, not a factory and an
// empty queue; Busy() is that test, made at the same moment (the gadget calls it where its loop starts), in the same order. A
// builder that only becomes busy during the gadget's own loop (an order given by a callback inside it) is seen next frame.
// Determinism: ids ascending; reads synced state only; no thread, clock or hash order.
// Doc:        game-build docs/engine/rules/builders.md

#include <memory>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Units/UnitTypes/Builder.h"
#include "Sim/Units/UnitTypes/Factory.h"
#include "System/StringHash.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace {

class Builders : public Rule {
public:
	Builders() : Rule({"Builders", 1, true, "LuaRules/Gadgets/game_eco.lua (idle-builder test of the builder walk)", false}) {}

	void Start() override { factoryDef.assign(unitDefHandler->NumUnitDefs() + 1, 0); }

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Busy", LBusy);
		LuaPushRawNamedCFunc(L, "SetFactoryDefs", LSetFactoryDefs);
	}

private:
	static Builders* SyncedSelf(lua_State* L) {
		Builders* r = static_cast<Builders*>(Registry::Get().Find("Builders"));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}

	static int LBusy(lua_State* L) {
		Builders* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		ScopedTimer t(hashString("KT1::Builders"));
		const unsigned int maxU = unitHandler.MaxUnits();
		lua_createtable(L, 256, 0);
		int n = 0;
		for (unsigned int id = 0; id < maxU; ++id) {
			const CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) continue;
			const CBuilder* b = dynamic_cast<const CBuilder*>(u);
			const CFactory* f = (b == nullptr) ? dynamic_cast<const CFactory*>(u) : nullptr;
			if (b == nullptr && f == nullptr) continue;
			const int d = u->unitDef->id;
			bool busy = (b != nullptr && b->curBuild != nullptr) || (f != nullptr && f->curBuild != nullptr);
			if (!busy && d >= 0 && d < (int)r->factoryDef.size() && r->factoryDef[d]) busy = true;
			if (!busy && u->commandAI != nullptr && !u->commandAI->commandQue.empty()) busy = true;
			if (!busy) continue;
			lua_pushnumber(L, id);
			lua_rawseti(L, -2, ++n);
		}
		return 1;
	}

	static int LSetFactoryDefs(lua_State* L) {
		Builders* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		luaL_checktype(L, 1, LUA_TTABLE);
		std::fill(r->factoryDef.begin(), r->factoryDef.end(), 0);
		for (int i = 1; ; ++i) {
			lua_rawgeti(L, 1, i);
			if (lua_isnil(L, -1)) { lua_pop(L, 1); break; }
			const int d = lua_toint(L, -1);
			lua_pop(L, 1);
			if (d >= 0 && d < (int)r->factoryDef.size()) r->factoryDef[d] = 1;
		}
		return 0;
	}

	std::vector<char> factoryDef;   // per def id: the gadget's isFactoryDef
};

} // namespace

std::unique_ptr<Rule> MakeBuilders() { return std::make_unique<Builders>(); }

} // namespace KT1
