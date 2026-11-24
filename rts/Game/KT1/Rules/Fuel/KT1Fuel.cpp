/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Fuel (version 1). The fuel of every aircraft that uses fuel (SPC-012 v3 R3-R6, v6 R10, v9 R8), stepped each frame in the
// engine. The air staging gadget keeps the pads, queues, docking, repair and the empty-tank penalty, fed by this rule's events.
//
// Replaces:   the fuel step of LuaRules/Gadgets/spc_staging.lua (its per-aircraft walk, the height read and the gauge); the gadget
//             stays, reads fuel through this rule and pulls Events() when this is on
// Switch:     mod option kt1_fuel (default off)
// Config:     gamedata/kt1/fuel.lua
//               climb_above   elmos above the ground at which an aircraft counts as flying (burns fuel)                  (DESIGN 4)
//               gauge_step    share of the capacity the gauge must move before it is sent again                         (DESIGN 0.005)
//               low_every     frames between the low-fuel events of one aircraft (units spread by id)                   (DESIGN 5)
//               low_fraction  low-fuel fraction where the unit gives none (the gadget pushes its own with SetLowDefault) (DESIGN 0.19)
// Unit data:  the unit def's customParams spc_fuel_capacity (seconds; > 0: uses fuel), spc_fuel_recharge_rate,
//             spc_fuel_ground_rate, spc_fuel_low_fraction: the same keys the gadget reads
// Timer:      KT1::Fuel
// Lua:        Spring.KT1.Fuel.Events() -> { kind, unitID, value, ... } since the last pull (synced only):
//               1 empty (value 0), 2 fuel again after empty (value: fuel), 3 low and not docked (value: fuel),
//               4 gauge (value: fuel; sent when it moved by gauge_step of the capacity, or crossed empty or full)
//             Spring.KT1.Fuel.Get(unitID) -> fuel (seconds) or nil
//             Spring.KT1.Fuel.Set(unitID, fuel)          synced only: tests and the save check
//             Spring.KT1.Fuel.Dock(unitID, mult or nil)  synced only: docked at a pad or stored in a carrier: refuels at
//                                                         recharge x mult per second; nil: not docked
//             Spring.KT1.Fuel.SetLowDefault(fraction)    synced only
//             Spring.KT1.Fuel.Config()
// Rule, each frame, over unit ids in ascending order; an aircraft is seen first with a full tank (as the gadget's UnitCreated):
//   docked (mult set): fuel = min(cap, fuel + recharge x mult / 30)
//   carried by a transport: unchanged
//   else above the ground by more than climb_above: fuel = max(0, fuel - 1 / 30); else fuel = min(cap, fuel + ground / 30)
//   The gadget stepped each aircraft every 5 frames by the frames since its last step (every frame when docked or near empty):
//   the rates are the same; the values differ in the last bits and the low-fuel look comes up to 4 frames sooner.
// Determinism: ids ascending; float maths on synced state in one fixed order; no thread, clock or hash order.
// Doc:        game-build docs/engine/rules/fuel.md

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/Ground.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

float Param(const UnitDef* ud, const char* key, float def) {
	const auto it = ud->customParams.find(key);
	if (it == ud->customParams.end()) return def;
	char* end = nullptr;
	const float v = std::strtof(it->second.c_str(), &end);
	return (end != it->second.c_str()) ? v : def;
}

class Fuel : public Rule {
public:
	Fuel() : Rule({"Fuel", 1, true, "LuaRules/Gadgets/spc_staging.lua (fuel step)", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		climbAbove = cfg.GetFloat("climb_above", 4.0f);
		gaugeStep = cfg.GetFloat("gauge_step", 0.005f);
		lowEvery = std::max(1, cfg.GetInt("low_every", 5));
		lowDefault = cfg.GetFloat("low_fraction", 0.19f);
	}

	void Start() override {
		const int n = unitDefHandler->NumUnitDefs();
		cap.assign(n + 1, 0.0f);
		recharge.assign(n + 1, 0.0f);
		ground.assign(n + 1, 0.0f);
		low.assign(n + 1, -1.0f);
		for (int d = 1; d <= n; ++d) {
			const UnitDef* ud = unitDefHandler->GetUnitDefByID(d);
			if (ud == nullptr) continue;
			const float c = Param(ud, "spc_fuel_capacity", 0.0f);
			if (!(c > 0.0f)) continue;
			cap[d] = c;
			recharge[d] = Param(ud, "spc_fuel_recharge_rate", 0.0f);
			ground[d] = Param(ud, "spc_fuel_ground_rate", 0.0f);
			low[d] = Param(ud, "spc_fuel_low_fraction", -1.0f);
		}
	}

	void GameFrame(int frame) override {
		const unsigned int maxU = unitHandler.MaxUnits();
		Grow();
		const float step = 1.0f / GAME_SPEED;
		const int lowSlot = frame % lowEvery;
		for (unsigned int id = 0; id < maxU; ++id) {
			const CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr) { owner[id] = nullptr; continue; }
			const int d = u->unitDef->id;
			if (d < 0 || d >= (int)cap.size() || !(cap[d] > 0.0f)) { owner[id] = nullptr; continue; }
			const float c = cap[d];
			if (owner[id] != u) { Begin(id, u, c); continue; }
			float v = fuel[id];
			if (mult[id] >= 0.0f) {
				v = std::min(c, v + recharge[d] * mult[id] * step);
			} else if (u->GetTransporter() == nullptr) {
				if (u->pos.y - CGround::GetHeightReal(u->pos.x, u->pos.z, true) > climbAbove)
					v = std::max(0.0f, v - step);
				else
					v = std::min(c, v + ground[d] * step);
			}
			fuel[id] = v;
			const float s = sent[id];
			if (std::fabs(v - s) >= gaugeStep * c || (v <= 0.0f) != (s <= 0.0f) || (v >= c) != (s >= c)) {
				Push(4, (int)id, v);
				sent[id] = v;
			}
			if (v <= 0.0f && !empty[id]) { empty[id] = 1; Push(1, (int)id, 0.0f); }
			else if (v > 0.0f && empty[id]) { empty[id] = 0; Push(2, (int)id, v); }
			if (mult[id] < 0.0f && (int)(id % (unsigned)lowEvery) == lowSlot) {
				const float lf = low[d] >= 0.0f ? low[d] : lowDefault;
				if (v < lf * c) Push(3, (int)id, v);
			}
		}
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Events", LEvents);
		LuaPushRawNamedCFunc(L, "Get", LGet);
		LuaPushRawNamedCFunc(L, "Set", LSet);
		LuaPushRawNamedCFunc(L, "Dock", LDock);
		LuaPushRawNamedCFunc(L, "SetLowDefault", LSetLowDefault);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static constexpr size_t MAX_EVENTS = 3 * 16384;   // DESIGN: events held between pulls (the gadget pulls every frame)

	void Push(int kind, int id, float v) {
		if (events.size() + 3 > MAX_EVENTS) return;
		events.push_back((float)kind);
		events.push_back((float)id);
		events.push_back(v);
	}

	static Fuel* Self() { return static_cast<Fuel*>(Registry::Get().Find("Fuel")); }
	static Fuel* SyncedSelf(lua_State* L) {
		Fuel* r = Self();
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}
	void Grow() {
		const unsigned int maxU = unitHandler.MaxUnits();
		if (owner.size() >= maxU) return;
		owner.resize(maxU, nullptr);
		fuel.resize(maxU, 0.0f);
		sent.resize(maxU, -1.0f);
		mult.resize(maxU, -1.0f);
		empty.resize(maxU, 0);
	}
	void Begin(int id, const CUnit* u, float c) { owner[id] = u; fuel[id] = c; sent[id] = -1.0f; mult[id] = -1.0f; empty[id] = 0; }
	// A unit that uses fuel is tracked from the first time anyone asks for it (a gadget may set the fuel of an aircraft in the frame
	// it is created, before the rule's next frame sees it): starts full, as the gadget's UnitCreated did.
	bool Tracked(int id) {
		if (id < 0 || cap.empty()) return false;
		const CUnit* u = unitHandler.GetUnit(id);
		if (u == nullptr) return false;
		const int d = u->unitDef->id;
		if (d < 0 || d >= (int)cap.size() || !(cap[d] > 0.0f)) return false;
		Grow();
		if (id >= (int)owner.size()) return false;
		if (owner[id] != u) Begin(id, u, cap[d]);
		return true;
	}

	static int LEvents(lua_State* L) {
		Fuel* r = SyncedSelf(L);
		if (r == nullptr) return 0;
		lua_createtable(L, (int)r->events.size(), 0);
		for (size_t i = 0; i < r->events.size(); ++i) { lua_pushnumber(L, r->events[i]); lua_rawseti(L, -2, (int)i + 1); }
		r->events.clear();
		return 1;
	}
	static int LGet(lua_State* L) {
		Fuel* r = Self();
		const int id = luaL_checkint(L, 1);
		if (r == nullptr || !r->Enabled() || !r->Tracked(id)) return 0;
		lua_pushnumber(L, r->fuel[id]);
		return 1;
	}
	static int LSet(lua_State* L) {
		Fuel* r = SyncedSelf(L);
		const int id = luaL_checkint(L, 1);
		if (r == nullptr || !r->Tracked(id) || lua_isnoneornil(L, 2)) return 0;
		const float c = r->cap[r->owner[id]->unitDef->id];
		r->fuel[id] = std::max(0.0f, std::min(c, luaL_checkfloat(L, 2)));
		return 0;
	}
	static int LDock(lua_State* L) {
		Fuel* r = SyncedSelf(L);
		const int id = luaL_checkint(L, 1);
		if (r == nullptr || !r->Tracked(id)) return 0;
		r->mult[id] = lua_isnoneornil(L, 2) ? -1.0f : std::max(0.0f, luaL_checkfloat(L, 2));
		return 0;
	}
	static int LSetLowDefault(lua_State* L) {
		Fuel* r = SyncedSelf(L);
		if (r != nullptr) r->lowDefault = luaL_checkfloat(L, 1);
		return 0;
	}
	static int LConfig(lua_State* L) {
		const Fuel* r = Self();
		if (r == nullptr) return 0;
		lua_newtable(L);
		LuaPushNamedNumber(L, "climb_above", r->climbAbove);
		LuaPushNamedNumber(L, "gauge_step", r->gaugeStep);
		LuaPushNamedNumber(L, "low_every", r->lowEvery);
		LuaPushNamedNumber(L, "low_fraction", r->lowDefault);
		return 1;
	}

	float climbAbove = 4.0f, gaugeStep = 0.005f, lowDefault = 0.19f;
	int lowEvery = 5;
	std::vector<float> cap, recharge, ground, low;   // per def id
	std::vector<const CUnit*> owner;                 // per unit id: who the state belongs to (an id is reused)
	std::vector<float> fuel, sent, mult;             // per unit id
	std::vector<char> empty;                         // per unit id
	std::vector<float> events;                       // (kind, unit, value) triples since the last pull
};

} // namespace

std::unique_ptr<Rule> MakeFuel() { return std::make_unique<Fuel>(); }

} // namespace KT1
