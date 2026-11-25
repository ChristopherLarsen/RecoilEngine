/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: see SupComTargeting.h. A line-for-line port of the per-frame part of the game's cmb_targeting.lua (GameFrame, Scan,
// Acceptable, LayerOK, LineClear), design: game docs/engine/TARGETING-ENGINE-DESIGN.md. Doubles where the Lua computed in doubles;
// floats where the Lua passed a value through a float API (cylinder radius, ground height sample point).

#include "SupComTargeting.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "LuaUtils.h"
#include "Map/Ground.h"
#include "Sim/Misc/LosHandler.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/SupComGroundMoveType.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/CommandAI/FactoryCAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/TimeProfiler.h"
#include "Sim/Weapons/Weapon.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"

namespace {

struct WDef {
	bool used = false;
	int scan = 1;            // frames: max(1, s_to_frames(scan_interval)), from Lua
	double track = 1.0;
	bool retarget = false, direct = false;
	double rangeMin = 0.0;   // elmos
	bool air = false, surface = false, under = false, seafloor = false, above = false, below = false, noGround = false;
};

struct Gun {
	int n = 0;               // weapon index, 0-based
	int wd = 0;
	int target = -1;
	int nextScan = 0;
	int nextBetter = 0;
	int reg = -1;
	bool groundOff = false;
	int avoid0 = -1;
	std::vector<std::pair<int, int>> skip; // (target, until frame)
	bool hasSkip = false;
	int skipEnd = 0;
};

int mode = 0;
double inset = 0.0;
int skipFrames = 0;
bool overkill = false;
std::vector<WDef> wdefs;
std::vector<std::vector<unsigned char>> ranks;           // [wd][ud]; 0 = not a target
std::vector<std::vector<Gun>> guns;                      // [unit]
std::vector<int> nextVisit;                              // [unit]; -1 none
std::map<int, std::vector<int>> due;                     // frame -> units
std::vector<std::vector<int>> shooters;                  // [target] -> shooters (only schedules wakes; order-free)
std::vector<int> focus;                                  // [unit] -> attack-move focus; -1 none
std::set<int> focusUnits;
std::vector<double> inflight;                            // [target] -> damage in flight
std::vector<int> deadNow;
std::vector<char> deadFlag;
std::vector<int> todo;
int stScans = 0, stCand = 0, stHolds = 0;

constexpr int HOLD_CHECK = 3;   // as cmb_targeting.lua (DESIGN, T12)
constexpr int HOLD_REST = 30;   // as cmb_targeting.lua
constexpr int NOGROUND = 64;

void Grow(int u) {
	if (u < 0)
		return;
	const size_t n = std::max<size_t>(u + 1, unitHandler.MaxUnits());
	if (guns.size() >= n)
		return;
	guns.resize(n); nextVisit.resize(n, -1); shooters.resize(n); focus.resize(n, -1); inflight.resize(n, 0.0); deadFlag.resize(n, 0);
}

const CUnit* U(int id) { return (id < 0)? nullptr: unitHandler.GetUnit(id); }

void Wake(int u, int frame) {
	if (u < 0)
		return;
	Grow(u);
	const int nv = nextVisit[u];
	if (nv >= 0 && nv <= frame)
		return;
	nextVisit[u] = frame;
	due[frame].push_back(u);
}

int Rank(const Gun& g, const CUnit* t) {
	const auto& r = ranks[g.wd];
	const int ud = t->unitDef->id;
	return (ud >= 0 && ud < (int)r.size())? r[ud]: 0;
}

double HDist(const CUnit* a, const CUnit* b) {
	if (a == nullptr || b == nullptr)
		return HUGE_VAL;
	const double dx = (double)a->pos.x - (double)b->pos.x;
	const double dz = (double)a->pos.z - (double)b->pos.z;
	return std::sqrt(dx * dx + dz * dz);
}

bool LayerOK(const WDef& w, const CUnit* t) {
	if (t->unitDef->canfly)
		return (!w.below && w.air);
	if (w.noGround)
		return false;
	std::string l;
	if (const auto* scmt = dynamic_cast<const CSupComGroundMoveType*>(t->moveType))
		l = scmt->GetLayerName();
	else
		l = (((double)t->pos.y + (double)t->height) < 0.0)? "sub": "land";
	if (l == "seabed") return (!w.above && (w.seafloor || w.under));
	if (l == "sub") return (!w.above && w.under);
	if (l == "water") return w.surface;
	return (!w.below && w.surface);
}

bool LineClear(const CUnit* u, const CUnit* t) {
	const double ux = u->midPos.x, uy = u->midPos.y, uz = u->midPos.z;
	const double tx = t->midPos.x, ty = t->midPos.y, tz = t->midPos.z;
	const double dx = tx - ux, dz = tz - uz;
	const int n = (int)std::floor(std::sqrt(dx * dx + dz * dz) / 8.0);
	for (int i = 1; i <= n - 1; i++) {
		const double k = (double)i / (double)n;
		const float gx = (float)(ux + dx * k);
		const float gz = (float)(uz + dz * k);
		if ((double)CGround::GetHeightReal(gx, gz, true) > uy + (ty - uy) * k + 1.0)
			return false;
	}
	return true;
}

bool Arriving(const CUnit* t) {
	const auto it = t->modParams.find("cmd_arriving");
	if (it == t->modParams.end())
		return false;
	if (const float* v = std::get_if<float>(&it->second.value))
		return (*v == 1.0f);
	return false;
}

// true when t is acceptable; failed = a check failed (sight, layer, line: skip memory) rather than the target being gone
bool Acceptable(const CUnit* u, const WDef& w, int tid, bool noLine, bool& failed) {
	failed = false;
	const CUnit* t = U(tid);
	if (t == nullptr || t->isDead)
		return false;
	if (teamHandler.AlliedTeams(u->team, t->team))
		return false;
	if (Arriving(t))
		return false;
	failed = true;
	if (!losHandler->InLos(t, u->allyteam))
		return false;
	if (!LayerOK(w, t))
		return false;
	if (noLine || !w.direct || LineClear(u, t)) {
		failed = false;
		return true;
	}
	return false;
}

bool Skipped(Gun& g, int t, int f) {
	if (!g.hasSkip)
		return false;
	if (f >= g.skipEnd) {
		g.skip.clear();
		g.hasSkip = false;
		return false;
	}
	for (size_t i = 0; i < g.skip.size(); i++) {
		if (g.skip[i].first != t)
			continue;
		if (f < g.skip[i].second)
			return true;
		g.skip.erase(g.skip.begin() + i);
		return false;
	}
	return false;
}

bool Doomed(int t) {
	if (t < 0 || t >= (int)inflight.size() || inflight[t] <= 0.0)
		return false;
	const CUnit* tu = U(t);
	return (tu != nullptr && inflight[t] >= (double)tu->health);
}

int Scan(const CUnit* u, Gun& g, const WDef& w, double range, int f, int avoid) {
	stScans++;
	const double reach = range - inset;
	// as Spring.GetUnitsInCylinder(x, z, reach): float radius, candidates by midPos in 2D
	const float fr = (float)reach;
	const float fx = u->pos.x;
	const float fz = u->pos.z;
	const float radSqr = fr * fr;
	QuadFieldQuery qfq;
	quadField.GetUnitsExact(qfq, float3(fx - fr, 0.0f, fz - fr), float3(fx + fr, 0.0f, fz + fr));
	int best = -1, bestR = 0, bestA = 0;
	double bestD = 0.0, bestH = 0.0;
	bool bestDoomed = false;
	for (const CUnit* t: *qfq.units) {
		if (t->midPos.SqDistance2D(float3(fx, 0.0f, fz)) > radSqr)
			continue;
		stCand++;
		if (t->allyteam == u->allyteam)
			continue;
		const int r = Rank(g, t);
		if (r == 0)
			continue;
		const double d = HDist(u, t);
		if (!(d <= reach && d >= w.rangeMin))
			continue;
		if (Skipped(g, t->id, f))
			continue;
		// overkill: a target the shots in flight already kill ranks after every other one
		const bool doomed = (t->id == avoid) || Doomed(t->id);
		const int armed = t->unitDef->HasWeapons()? 0: 1;
		const double hp = t->health;
		bool better = (best < 0) || (bestDoomed && !doomed);
		if (!better && doomed == bestDoomed) {
			if (r != bestR) better = (r < bestR);
			else if (armed != bestA) better = (armed < bestA);
			else if (d != bestD) better = (d < bestD);
			else if (hp != bestH) better = (hp < bestH);
			else better = (t->id < best);
		}
		bool failed;
		if (better && Acceptable(u, w, t->id, false, failed)) {
			best = t->id; bestD = d; bestR = r; bestDoomed = doomed; bestA = armed; bestH = hp;
		}
	}
	return best;
}

// hold fire, or a player (not internal) attack / manual-fire order at the front of the queue
void Overridden(const CUnit* u, bool& over, bool& attack, bool& hold) {
	over = attack = hold = false;
	if (u->fireState == 0) {
		over = true;
		hold = true;
		return;
	}
	const CCommandAI* cai = u->commandAI;
	const CFactoryCAI* fcai = dynamic_cast<const CFactoryCAI*>(cai);
	const CCommandQueue& q = (fcai == nullptr)? cai->commandQue: fcai->newUnitCommands;
	if (q.empty())
		return;
	const Command& c = q.front();
	const bool order = (c.GetID() == CMD_ATTACK || c.GetID() == CMD_MANUALFIRE) && !c.IsInternalOrder();
	over = order;
	attack = order && (c.GetID() == CMD_ATTACK);
}

void FireIntoGround(CWeapon* wp, Gun& g, bool on) {
	g.groundOff = on;
	if (on) {
		const int fl = wp->avoidFlags;
		if ((fl % (2 * NOGROUND)) < NOGROUND) {
			g.avoid0 = fl;
			wp->avoidFlags = fl + NOGROUND;
		}
	} else if (g.avoid0 >= 0) {
		wp->avoidFlags = g.avoid0;
		g.avoid0 = -1;
	}
}

void SetReg(int u, Gun& g) {
	const int t = g.target;
	if (t == g.reg)
		return;
	if (g.reg >= 0 && g.reg < (int)shooters.size()) {
		auto& sh = shooters[g.reg];
		sh.erase(std::remove(sh.begin(), sh.end(), u), sh.end());
	}
	if (t >= 0) {
		Grow(t);
		auto& sh = shooters[t];
		if (std::find(sh.begin(), sh.end(), u) == sh.end())
			sh.push_back(u);
	}
	g.reg = t;
}

void Visit(int uid, int f) {
	auto& list = guns[uid];
	CUnit* u = unitHandler.GetUnit(uid);
	if (list.empty() || u == nullptr)
		return;
	bool haveOver = false, over = false, attack = false, hold = false;
	int fo = focus[uid];
	const CUnit* fu = U(fo);
	if (fu == nullptr)
		fo = -1;
	const bool apply = (mode == 1);
	for (Gun& g: list) {
		if (g.n < 0 || g.n >= (int)u->weapons.size())
			continue;
		CWeapon* wp = u->weapons[g.n];
		const WDef& w = wdefs[g.wd];
		bool failed;
		// attack-move focus: a weapon that can take it and reaches it holds it
		if (fo >= 0 && g.target != fo && (f + uid) % 3 == 0 && Rank(g, fu) != 0 && Acceptable(u, w, fo, false, failed)
				&& HDist(u, fu) <= (double)wp->range)
			g.target = fo;
		const bool dead = (g.target >= 0 && g.target < (int)deadFlag.size() && deadFlag[g.target]);
		if (!((g.target >= 0 && ((f + uid) % HOLD_CHECK == 0 || dead)) || (g.target < 0 && f >= g.nextScan)))
			continue;
		if (!haveOver) {
			Overridden(u, over, attack, hold);
			haveOver = true;
		}
		if (g.target >= 0)
			stHolds++;
		if (apply && w.direct && attack != g.groundOff)
			FireIntoGround(wp, g, attack);   // CMB-016 R2
		const double range = wp->range;
		if (g.target >= 0) {
			const bool ok = Acceptable(u, w, g.target, (f + uid) % 15 != 0, failed);
			if (!ok || HDist(u, U(g.target)) > range * w.track) {   // R3, R5
				if (failed) {   // skip memory
					const int untilF = f + skipFrames;
					bool found = false;
					for (auto& s: g.skip) {
						if (s.first == g.target) { s.second = untilF; found = true; }
					}
					if (!found)
						g.skip.emplace_back(g.target, untilF);
					g.skipEnd = g.hasSkip? std::max(g.skipEnd, untilF): std::max(0, untilF);
					g.hasSkip = true;
				}
				g.target = -1;
				g.nextScan = f;   // R1: scan at once
			}
		}
		// overkill: at each scan a gun whose target is already doomed takes another one if there is one
		if (g.target >= 0 && !over && f >= g.nextScan && Doomed(g.target)) {
			const int c = Scan(u, g, w, range, f, g.target);
			if (c >= 0)
				g.target = c;
			g.nextScan = f + w.scan;
		}
		// CMB-004 v3 R5b
		if (w.retarget && g.target >= 0 && !over && g.target != fo && f >= g.nextBetter) {
			g.nextBetter = f + w.scan;
			const CUnit* cur = U(g.target);
			const int r0 = (cur != nullptr)? Rank(g, cur): 0;
			if (r0 > 1) {
				const int cand = Scan(u, g, w, range, f, -1);
				const CUnit* cu = U(cand);
				const int rc = (cand != g.target && cu != nullptr)? Rank(g, cu): 0;
				if (rc != 0 && rc < r0)
					g.target = cand;
			}
		}
		if (over) {
			g.target = -1;
			if (hold && f >= g.nextScan)
				g.nextScan = f + HOLD_REST;
		} else if (g.target < 0 && f >= g.nextScan) {
			g.target = Scan(u, g, w, range, f, -1);
			g.nextScan = f + w.scan;   // R1
		}
		if (apply && g.target >= 0) {
			const SWeaponTarget& cur = wp->GetCurrentTarget();
			const int curId = (cur.type == Target_Unit && cur.unit != nullptr)? cur.unit->id: -1;
			if (curId != g.target) {
				if (const CUnit* tu = U(g.target))
					wp->Attack(SWeaponTarget(tu, false));
			}
		}
	}
	// the next visit, and the shooters index for the death wake
	int nxt = -1;
	for (Gun& g: list) {
		SetReg(uid, g);
		int at;
		if (g.target >= 0) {
			int m = (-(f + 1 + uid)) % HOLD_CHECK;
			if (m < 0) m += HOLD_CHECK;   // Lua % is floored
			at = f + 1 + m;
		} else {
			at = (g.nextScan > f)? g.nextScan: f + 1;
		}
		if (nxt < 0 || at < nxt)
			nxt = at;
	}
	if (nxt >= 0)
		Wake(uid, nxt);
}

void Reset() {
	guns.clear(); nextVisit.clear(); due.clear(); shooters.clear(); focus.clear(); focusUnits.clear(); inflight.clear();
	deadNow.clear(); deadFlag.clear(); todo.clear();
	stScans = stCand = stHolds = 0;
	Grow(0);
}

bool GetBoolField(lua_State* L, int idx, const char* k) {
	lua_getfield(L, idx, k);
	const bool b = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return b;
}

double GetNumField(lua_State* L, int idx, const char* k, double def) {
	lua_getfield(L, idx, k);
	const double v = lua_isnumber(L, -1)? lua_tonumber(L, -1): def;
	lua_pop(L, 1);
	return v;
}

void ReadRules(lua_State* L, int a) {
	inset = luaL_checknumber(L, a);
	skipFrames = luaL_checkint(L, a + 1);
	overkill = lua_toboolean(L, a + 2);
}

// Spring.SupComTgtRules(insetElmos, skipFrames, overkill)
int Rules(lua_State* L) { ReadRules(L, 1); return 0; }

// Spring.SupComTgtInit(mode, insetElmos, skipFrames, overkill, defs { [wdID] = {...} }, ranks { [wdID] = { [udID] = rank } })
int Init(lua_State* L) {
	const int m = luaL_checkint(L, 1);
	ReadRules(L, 2);
	luaL_checktype(L, 5, LUA_TTABLE);
	luaL_checktype(L, 6, LUA_TTABLE);
	Reset();
	wdefs.assign(weaponDefHandler->NumWeaponDefs() + 1, WDef());
	ranks.assign(wdefs.size(), {});
	for (lua_pushnil(L); lua_next(L, 5) != 0; lua_pop(L, 1)) {
		const int id = lua_tointeger(L, -2);
		if (id < 0 || id >= (int)wdefs.size() || !lua_istable(L, -1))
			continue;
		const int t = lua_gettop(L);
		WDef& w = wdefs[id];
		w.used = true;
		w.scan = std::max(1, (int)GetNumField(L, t, "scan", 1));
		w.track = GetNumField(L, t, "track", 1.0);
		w.rangeMin = GetNumField(L, t, "range_min", 0.0);
		w.retarget = GetBoolField(L, t, "retarget");
		w.direct = GetBoolField(L, t, "direct");
		w.air = GetBoolField(L, t, "air");
		w.surface = GetBoolField(L, t, "surface");
		w.under = GetBoolField(L, t, "under");
		w.seafloor = GetBoolField(L, t, "seafloor");
		w.above = GetBoolField(L, t, "above");
		w.below = GetBoolField(L, t, "below");
		w.noGround = GetBoolField(L, t, "noGround");
	}
	for (lua_pushnil(L); lua_next(L, 6) != 0; lua_pop(L, 1)) {
		const int id = lua_tointeger(L, -2);
		if (id < 0 || id >= (int)ranks.size() || !lua_istable(L, -1))
			continue;
		auto& r = ranks[id];
		for (lua_pushnil(L); lua_next(L, -2) != 0; lua_pop(L, 1)) {
			const int ud = lua_tointeger(L, -2);
			const int rk = lua_tointeger(L, -1);
			if (ud < 0 || ud > 65535)
				continue;
			if ((int)r.size() <= ud)
				r.resize(ud + 1, 0);
			r[ud] = (unsigned char)std::clamp(rk, 0, 255);
		}
	}
	mode = m;
	return 0;
}

// Spring.SupComTgtAdd(unitID, weaponNum (1-based), weaponDefID, firstScanFrame)
int Add(lua_State* L) {
	const int u = luaL_checkint(L, 1);
	const int n = luaL_checkint(L, 2) - 1;
	const int wd = luaL_checkint(L, 3);
	if (u < 0 || wd < 0 || wd >= (int)wdefs.size() || !wdefs[wd].used)
		return 0;
	Grow(u);
	Gun g;
	g.n = n;
	g.wd = wd;
	g.nextScan = luaL_checkint(L, 4);
	auto& list = guns[u];
	list.push_back(g);
	std::stable_sort(list.begin(), list.end(), [](const Gun& a, const Gun& b) { return a.n < b.n; });
	return 0;
}

// Spring.SupComTgtWake(unitID, frame)
int WakeL(lua_State* L) { Wake(luaL_checkint(L, 1), luaL_checkint(L, 2)); return 0; }

// Spring.SupComTgtDestroyed(unitID, nextFrame)
int Destroyed(lua_State* L) {
	const int u = luaL_checkint(L, 1);
	const int nf = luaL_checkint(L, 2);
	if (u < 0)
		return 0;
	Grow(u);
	if (!deadFlag[u]) {
		deadFlag[u] = 1;
		deadNow.push_back(u);
	}
	const std::vector<int> sh = shooters[u];
	shooters[u].clear();
	for (int s: sh)
		Wake(s, nf);
	for (Gun& g: guns[u]) {
		if (g.reg >= 0 && g.reg < (int)shooters.size()) {
			auto& v = shooters[g.reg];
			v.erase(std::remove(v.begin(), v.end(), u), v.end());
		}
	}
	nextVisit[u] = -1;
	guns[u].clear();
	focus[u] = -1;
	focusUnits.erase(u);
	inflight[u] = 0.0;
	return 0;
}

// Spring.SupComTgtFireState(unitID, frame): a fire-state order makes the unit's guns look at once
int FireState(lua_State* L) {
	const int u = luaL_checkint(L, 1);
	const int f = luaL_checkint(L, 2);
	if (u < 0 || u >= (int)guns.size() || guns[u].empty())
		return 0;
	for (Gun& g: guns[u]) {
		if (g.nextScan > f)
			g.nextScan = f;
	}
	Wake(u, f + 1);
	return 0;
}

// Spring.SupComTgtFocus(unitID, targetID | nil)
int Focus(lua_State* L) {
	const int u = luaL_checkint(L, 1);
	if (u < 0)
		return 0;
	Grow(u);
	const int t = lua_isnumber(L, 2)? lua_tointeger(L, 2): -1;
	focus[u] = t;
	if (t >= 0) focusUnits.insert(u); else focusUnits.erase(u);
	return 0;
}

// Spring.SupComTgtInflight(targetID, damage | nil)
int Inflight(lua_State* L) {
	const int t = luaL_checkint(L, 1);
	if (t < 0)
		return 0;
	Grow(t);
	inflight[t] = lua_isnumber(L, 2)? lua_tonumber(L, 2): 0.0;
	return 0;
}

// Spring.SupComTgtGet(unitID, weaponNum (1-based)) -> targetID | nil
int Get(lua_State* L) {
	const int u = luaL_checkint(L, 1);
	const int n = luaL_checkint(L, 2) - 1;
	if (u < 0 || u >= (int)guns.size())
		return 0;
	for (const Gun& g: guns[u]) {
		if (g.n == n && g.target >= 0) {
			lua_pushnumber(L, g.target);
			return 1;
		}
	}
	return 0;
}

// Spring.SupComTgtStats() -> scans, candidates, holds since the last call
int Stats(lua_State* L) {
	lua_pushnumber(L, stScans);
	lua_pushnumber(L, stCand);
	lua_pushnumber(L, stHolds);
	stScans = stCand = stHolds = 0;
	return 3;
}

} // namespace

void SupComTargeting::Update(int f)
{
	SCOPED_SPECIAL_TIMER("Sim::SupComTargeting");
	if (mode == 0)
		return;
	todo.clear();
	const auto it = due.find(f);
	if (it != due.end()) {
		for (int u: it->second) {
			if (u < (int)nextVisit.size() && nextVisit[u] == f) {
				nextVisit[u] = -1;
				todo.push_back(u);
			}
		}
	}
	// as the Lua: focus units are added unless their next visit is this frame (the due list cleared those it took, so a unit in
	// both is visited twice, as in cmb_targeting.lua)
	for (int u: focusUnits) {
		if (nextVisit[u] != f && !guns[u].empty())
			todo.push_back(u);
	}
	std::stable_sort(todo.begin(), todo.end());
	for (size_t i = 0; i < todo.size(); i++)
		Visit(todo[i], f);
	for (int u: deadNow)
		deadFlag[u] = 0;
	deadNow.clear();
	// frames at or before this one are done (a wake for this frame during the visits is dropped, as in the Lua)
	while (!due.empty() && due.begin()->first <= f)
		due.erase(due.begin());
}

bool SupComTargeting::OwnsAutoTarget(const CWeapon* w)
{
	if (mode != 1 || w == nullptr || w->owner == nullptr)
		return false;
	const int u = w->owner->id;
	if (u < 0 || u >= (int)guns.size())
		return false;
	for (const Gun& g: guns[u]) {
		if (g.n == w->weaponNum)
			return true;
	}
	return false;
}

bool SupComTargeting::GunIdle(const CWeapon* w)
{
	if (mode != 1 || w == nullptr || w->owner == nullptr)
		return false;
	const int u = w->owner->id;
	if (u < 0 || u >= (int)guns.size())
		return false;
	for (const Gun& g: guns[u]) {
		if (g.n == w->weaponNum)
			return (g.target < 0);
	}
	return false;
}

// Spring.SupComTgtProf(begin): the A/B section timer for the Lua side (synced Lua has no clock): true starts, false adds the
// elapsed time to the "Lua::SupComTargeting" profiler timer. Timing only, never read by the sim.
static spring_time profStart;
static int Prof(lua_State* L)
{
	static TimerNameRegistrar tnr("Lua::SupComTargeting");
	if (lua_toboolean(L, 1)) {
		profStart = spring_gettime();
	} else {
		const spring_time now = spring_gettime();
		CTimeProfiler::GetInstance().AddTime(hashString("Lua::SupComTargeting"), profStart, now - profStart, false, true);
	}
	return 0;
}

void SupComTargeting::PushSyncedCtrl(lua_State* L)
{
	LuaPushRawNamedCFunc(L, "SupComTgtInit", Init);
	LuaPushRawNamedCFunc(L, "SupComTgtRules", Rules);
	LuaPushRawNamedCFunc(L, "SupComTgtAdd", Add);
	LuaPushRawNamedCFunc(L, "SupComTgtWake", WakeL);
	LuaPushRawNamedCFunc(L, "SupComTgtDestroyed", Destroyed);
	LuaPushRawNamedCFunc(L, "SupComTgtFireState", FireState);
	LuaPushRawNamedCFunc(L, "SupComTgtFocus", Focus);
	LuaPushRawNamedCFunc(L, "SupComTgtInflight", Inflight);
	LuaPushRawNamedCFunc(L, "SupComTgtGet", Get);
	LuaPushRawNamedCFunc(L, "SupComTgtStats", Stats);
	LuaPushRawNamedCFunc(L, "SupComTgtProf", Prof);
}
