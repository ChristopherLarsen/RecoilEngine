/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ORDERS, step S05: see KT1OrdersTurn.h.

#include "KT1OrdersTurn.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "OrdersInternal.h"

#include "Game/KT1/Core/Orders/TurnTrace.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/Fakes.h"
#include "Game/KT1/Host/KT1Trace.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SimContext.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"

namespace KT1::Orders {

namespace CO = Core::Orders;

namespace {

constexpr const char* SYSTEM = "Orders";
// DESIGN: directly after LuaRules, so a join or leave that an earlier gadget causes from inside its own handler of the
// same call-in is collected before the outer one, the order in which ord_orders.lua's list sees them
constexpr int CLIENT_ORDER = LUA_HANDLE_ORDER_RULES + 1;
constexpr int ARG_STEP = 1;          // DESIGN: TurnBegin's arguments
constexpr int ARG_NET = 2;           // DESIGN
constexpr int ARG_GUARD_S = 3;       // DESIGN
constexpr int ARG_PATROL_S = 4;      // DESIGN
constexpr int ARG_FIGHT_S = 5;       // DESIGN
constexpr int ARG_UNIT = 1;          // DESIGN: TurnRead's arguments
constexpr int ARG_OWN_KIND = 2;      // DESIGN
constexpr int ARG_OWN_TAG = 3;       // DESIGN
constexpr int ARG_OWN_TARGET = 4;    // DESIGN
constexpr int ARG_TASK_TAG = 5;      // DESIGN
constexpr int ARG_WALK = 6;          // DESIGN
constexpr int ARG_GIVEN = 7;         // DESIGN
constexpr int GIVEN_FIELDS = 2;      // DESIGN: one given order is { cmdID, target }

struct Adapter {
	bool active = false;             // joins and leaves are collected and the Lua calls answer
	bool check = false;
	bool walking = false;            // between TurnBegin and TurnEnd
	bool initSent = false;           // the first walk carried the init
	std::unique_ptr<CO::State> state;    // check mode, from the first sim step
	CO::OrdersConfig config;         // the family's config with the Lua's three intervals over it
	CO::TurnParams params;
	CO::TurnInput in;                // the walk being collected; units in visit order until TurnEnd
	CO::TurnOutput out;
	std::vector<CO::TurnEvent> events;   // since the last walk
	std::vector<CO::OrderView> queue;    // reused
};

Adapter turn;

float MapX() { return static_cast<float>(mapDims.mapx * SQUARE_SIZE); }
float MapZ() { return static_cast<float>(mapDims.mapy * SQUARE_SIZE); }

// at the first sim step, never at Start: the Lua hands defs over while it loads (Internal::HandAccepts)
void EnsureState() {
	if (!turn.check || turn.state != nullptr) return;
	const std::vector<CO::DefFacts>& defs = Internal::Defs();
	turn.state = std::make_unique<CO::State>();
	turn.state->Init(unitHandler.MaxUnits(), static_cast<int32_t>(defs.size()) - 1, MapX(), MapZ(), Internal::Config());
	turn.state->defs = defs;
}

// Check mode only: the two passes return at once under c.check
void PreTurn(const SimContext& c) {
	EnsureState();
	Internal::RunSelfDestructs(c);
	Internal::RunFerries(c);
}

// ---- joins and leaves -----------------------------------------------------------------------------------------------

// a fake's call-ins never reach ord_orders.lua (gamedata/fake_units.lua)
void Note(CO::TurnEventKind kind, const CUnit* u) {
	if (!turn.active || Fakes::Is(u)) return;
	std::vector<CO::TurnEvent>* into = &turn.events;
	if (turn.walking) into = turn.in.units.empty() ? &turn.in.events : &turn.in.units.back().during;
	into->push_back({kind, u->id});
}

class TurnClient final : public CEventClient {
public:
	TurnClient() : CEventClient("[KT1OrdersTurn]", CLIENT_ORDER, true) {}
	bool WantsEvent(const std::string& name) override { return name == "UnitCommand" || name == "UnitDestroyed"; }
	int GetReadAllyTeam() const override { return AllAccessTeam; }
	void UnitCommand(const CUnit* u, const Command&, int, bool, bool) override { Note(CO::TurnEventKind::Join, u); }
	void UnitDestroyed(const CUnit* u, const CUnit*, int) override { Note(CO::TurnEventKind::Leave, u); }
};

// Not a static object: CEventClient's destructor calls the event handler, which may be gone by process exit.
TurnClient* client = nullptr;

// ---- one walk -------------------------------------------------------------------------------------------------------

void QueueOf(int unit) { Internal::ReadQueue(unit, -1, turn.queue); }

// the queue length after the last visit's turn: nothing happens between that turn and the next call
void CloseVisit() {
	if (turn.in.units.empty()) return;
	CO::TurnUnit& last = turn.in.units.back();
	QueueOf(last.unit);
	last.queueLenAfter = static_cast<int32_t>(turn.queue.size());
}

void ReadEngine(CO::TurnUnit& unit) {
	const CUnit* u = unitHandler.GetUnit(unit.unit);
	if (u == nullptr) return;
	unit.defId = u->unitDef->id;
	QueueOf(unit.unit);
	unit.queueLen = static_cast<int32_t>(turn.queue.size());
	CO::SetTurnQueue(unit, turn.queue);
}

float NumberAt(lua_State* L, int table, int i) {
	lua_rawgeti(L, table, i);
	const float v = lua_tofloat(L, -1);
	lua_pop(L, 1);
	return v;
}

void ReadOwn(lua_State* L, CO::TurnUnit& unit) {
	const int kind = luaL_checkint(L, ARG_OWN_KIND);
	if (kind > 0 && kind <= static_cast<int>(CO::OwnKind::Follow)) {
		unit.own.kind = static_cast<CO::OwnKind>(kind);
		unit.own.tag = static_cast<uint32_t>(luaL_checkfloat(L, ARG_OWN_TAG));
		unit.own.target = static_cast<int32_t>(luaL_checkfloat(L, ARG_OWN_TARGET));
	}
	if (lua_isnoneornil(L, ARG_TASK_TAG)) return;
	unit.hasTask = true;
	unit.taskTag = static_cast<uint32_t>(luaL_checkfloat(L, ARG_TASK_TAG));
}

void ReadWalk(lua_State* L, CO::TurnUnit& unit) {
	if (!lua_istable(L, ARG_WALK)) return;
	unit.walking = true;
	const int n = static_cast<int>(lua_objlen(L, ARG_WALK));
	if (n > static_cast<int>(CO::WALK_ORDERS))
		LOG_L(L_WARNING, "[KT1] Orders: unit %d walks %d steps, more than are kept", unit.unit, n);
	unit.walk.count = std::min(n, static_cast<int>(CO::WALK_ORDERS));
	for (int i = 0; i < unit.walk.count; ++i)
		unit.walk.tags[i] = static_cast<uint32_t>(NumberAt(L, ARG_WALK, i + 1));
}

void ReadGiven(lua_State* L, CO::TurnUnit& unit) {
	if (!lua_istable(L, ARG_GIVEN)) return;
	const int n = static_cast<int>(lua_objlen(L, ARG_GIVEN));
	for (int i = 1; i < n; i += GIVEN_FIELDS) {
		const int command = static_cast<int>(NumberAt(L, ARG_GIVEN, i));
		unit.given.push_back({Internal::KindOf(command), NumberAt(L, ARG_GIVEN, i + 1)});
	}
}

// the intervals the Lua runs on (a data set's values, or a fixture's) over the family's config; the first walk's go
// into the init
void TakeIntervals(float guard, float patrol, float fight) {
	if (turn.initSent) {
		const bool same = guard == turn.config.guard.check_s && patrol == turn.config.patrol.patrol_check_s
		                  && fight == turn.config.patrol.fight_check_s;
		if (same) return;
		LOG_L(L_WARNING, "[KT1] Orders: the turn intervals changed during the run; a trace across the change does not replay");
	} else {
		turn.config = Internal::Config();
	}
	turn.config.guard.check_s = guard;
	turn.config.patrol.patrol_check_s = patrol;
	turn.config.patrol.fight_check_s = fight;
	turn.params = CO::MakeTurnParams(turn.config);
	if (turn.initSent) return;
	turn.in.hasInit = true;
	turn.in.init = CO::MakeTurnInit(turn.config, Internal::Defs(), unitHandler.MaxUnits(), MapX(), MapZ());
	turn.initSent = true;
}

// The Lua has answered already and a key's answers pair newest first, so a unit visited twice is given last visit first.
void Report(const CO::TurnOutput& out) {
	for (size_t i = out.turnList.size(); i-- > 0;) {
		const int unit = out.turnList[i];
		Check::Native(SYSTEM, "front_class", unit, 0, 0, static_cast<float>(out.frontClass[i]));
		Check::Native(SYSTEM, "next_check", unit, 0, 0, static_cast<float>(out.nextCheck[i]));
	}
	const int size = static_cast<int>(out.turnList.size());
	Check::Native(SYSTEM, "turn_list", 0, 0, 1, static_cast<float>(size));
	for (int i = 0; i < size; ++i)
		Check::Native(SYSTEM, "turn_list", 0, i, 0, static_cast<float>(out.turnList[i]));
}

void FinishWalk() {
	CloseVisit();
	turn.walking = false;
	std::stable_sort(turn.in.units.begin(), turn.in.units.end(),
	                 [](const CO::TurnUnit& a, const CO::TurnUnit& b) { return a.unit < b.unit; });
	if (Trace::On(SYSTEM)) {
		Core::Trace::Bytes bytes;
		CO::Put(bytes, turn.in);
		Trace::Input(SYSTEM, "turn", bytes.View());
	}
	if (!turn.check || turn.state == nullptr) return;
	turn.in.hasInit = false;   // the state is set up already, with the whole config (EnsureState)
	CO::RunTurnStep(*turn.state, turn.params, turn.in, turn.out);
	Report(turn.out);
}

// ---- Lua ------------------------------------------------------------------------------------------------------------

bool Caller(lua_State* L) { return turn.active && CLuaHandle::GetHandleSynced(L); }

int LWatch(lua_State* L) {
	if (!Caller(L)) return 0;
	lua_pushstring(L, turn.check ? "check" : "trace");
	return 1;
}

int LBegin(lua_State* L) {
	if (!Caller(L)) return 0;
	EnsureState();
	turn.in = CO::TurnInput{};
	turn.in.now = luaL_checkint(L, ARG_STEP);
	turn.in.events.swap(turn.events);
	if (lua_istable(L, ARG_NET)) {
		const int n = static_cast<int>(lua_objlen(L, ARG_NET));
		for (int i = 1; i <= n; ++i)
			turn.in.netUnits.push_back(static_cast<int32_t>(NumberAt(L, ARG_NET, i)));
	}
	TakeIntervals(luaL_checkfloat(L, ARG_GUARD_S), luaL_checkfloat(L, ARG_PATROL_S), luaL_checkfloat(L, ARG_FIGHT_S));
	turn.walking = true;
	return 0;
}

int LRead(lua_State* L) {
	if (!Caller(L) || !turn.walking) return 0;
	CloseVisit();
	CO::TurnUnit unit;
	unit.unit = luaL_checkint(L, ARG_UNIT);
	ReadEngine(unit);
	ReadOwn(L, unit);
	ReadWalk(L, unit);
	ReadGiven(L, unit);
	turn.in.units.push_back(std::move(unit));
	return 0;
}

int LEnd(lua_State* L) {
	if (Caller(L) && turn.walking) FinishWalk();
	return 0;
}

} // namespace

namespace Internal {

Core::Orders::State* State() { return turn.state.get(); }

} // namespace Internal

namespace Turn {

void Start(bool check, bool record) {
	turn = Adapter{};
	if (!check && !record) return;
	turn.active = true;
	turn.check = check;
	if (check) Phases::Add(SYSTEM, Core::SimHost::Phase::FrameStart, PHASE_PRE_TURN, 0, PreTurn);
	if (client == nullptr) client = new TurnClient();
	else eventHandler.RemoveClient(client);
	eventHandler.AddClient(client);
}

void Stop() { turn = Adapter{}; }

void PushLua(lua_State* L) {
	LuaPushRawNamedCFunc(L, "TurnWatch", LWatch);
	LuaPushRawNamedCFunc(L, "TurnBegin", LBegin);
	LuaPushRawNamedCFunc(L, "TurnRead", LRead);
	LuaPushRawNamedCFunc(L, "TurnEnd", LEnd);
}

} // namespace Turn

} // namespace KT1::Orders
