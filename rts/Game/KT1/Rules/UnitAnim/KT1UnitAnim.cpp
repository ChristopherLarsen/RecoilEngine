/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: UnitAnim (version 1). The per-unit tick of the Lua unit script framework (scripts/pm/anim.lua Loop: Sense,
// pm/loco.lua Tick, pm/aim.lua Tick) for the units it can take, and the piece polls of pm/aim.lua AimWeapon (ANM-001, ANM-002,
// CMB-002). The scripts stay: they register the unit, keep every call-in, and run the whole Lua path when the rule is off.
//
// Replaces:   the Loop thread of scripts/pm/anim.lua for "engine units" (ground treads, wheels, legs or fixed, and rotor or jet aircraft;
//             body static, level, follow_terrain or bank; no work, link, unpack, charge or tilt); the poll of pm/aim.lua AimWeapon for those units
// Switch:     mod option kt1_unitanim (default off)
// Config:     gamedata/kt1/unitanim.lua
//               idle_ticks   still ticks before an idle unit backs off (DESIGN 2)
//               rest_delay_s seconds with no aim before a mount turns to rest (or rest_delay, frames at 30 Hz; DESIGN 90)
//               rest_rate    rad/s back to rest (DESIGN 1.5)
//               roll_eps     degrees: the body Turn is re-issued past this change (DESIGN 0.05)
//               wake_every   ticks between dust events of a moving unit (PLACEHOLDER 3)
//               spin_rate    cycles per second turning on the spot (DESIGN 0.6)
//               full_units, max_ms   the tick length: 100 ms up to full_units units, stretching in proportion, at most max_ms (DESIGN 250, 500; pm_tick.lua)
// Unit data:  none: the script sends the parsed descriptor with Add (Lua API below); C++ never names a unit
// Timer:      KT1::UnitAnim
// Lua:        Spring.KT1.UnitAnim.Add(unitID, spec) -> true | Remove(unitID) | Wake(unitID) | Aimed(unitID, w)
//             Spring.KT1.UnitAnim.AimWait(unitID, w, heading, pitch) -> frames to sleep, 0 = ready
//             Spring.KT1.UnitAnim.State(unitID) -> probes | Events() -> { unitID, x, y, z, ... } dust (a negated unitID: rotor downwash) | Config()
// Rule, each frame, over registered unit ids in ascending order: a unit whose wake frame is due ticks as the Lua Loop did (Sense, cycle,
// body, aim rest, dust), then sleeps as the Lua did (the pm tick length, the idle back-off, the first tick staggered by id).
// Determinism: ascending ids over a sorted list; float maths (the engine Lua number is float) in the Lua's expression order, cast to float at Turn/Move as the Lua
// binding does; per-unit state reset when an id is reused; no clock, thread or hash order.
// Doc:        game-build docs/engine/rules/unitanim.md

#include <limits>
#include "streflop_cond.h"   // math:: as the Lua math library (lmathlib): a pose is the same float either way
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "Game/KT1/KT1Config.h"
#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/Ground.h"
#include "Rendering/Models/3DModel.hpp"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/Scripts/UnitScript.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

constexpr float TAU = 2.0f * 3.14159265358979323846f;   // math.pi in Lua is the same float
constexpr float PI = TAU / 2.0f;

float Wrap(float a) { while (a > PI) a -= TAU; while (a < -PI) a += TAU; return a; }
float LuaMod(float a, float b) { return a - math::floor(a / b) * b; }   // Lua's %
float Rad(float d) { return d * math::DEG_TO_RAD; }                         // math.rad
float Deg(float r) { return r / math::DEG_TO_RAD; }                         // math.deg

enum Loco { L_FIXED, L_TREADS, L_WHEELS, L_LEGS, L_ROTOR, L_JET };
// trail kinds (pm/loco.lua Trails): a bit each, in the order the descriptor lists them is irrelevant (a set)
enum Trail { T_DUST = 1, T_TREADS = 2, T_FEET = 4, T_CONTRAIL = 8, T_EXHAUST = 16, T_GLOW = 32 };
constexpr int N_TRAIL = 6;
enum Body { B_STATIC, B_TERRAIN, B_BANK };

struct Weapon {
	int yaw = -1, pit = -1;                    // aim pieces (script numbers, 0-based; -1 none)
	int turret = -1, barrel = -1, barrelb = -1; // rest pieces
	float rest = 0, yawRate = 0, pitchRate = 0, tol = 0;
	int aimFrame = -9999;
	bool rested = false;
};

struct Unit {
	CUnit* owner = nullptr;
	int loco = L_FIXED, body = B_STATIC;
	bool spin = false, dust = false, quiet = false, canIdle = true;
	bool air = false, pfwd = false, wings = false;   // air: the unit can fly (pm/anim.lua landedOnly idle; layer "air" above 3 elmos)
	int trails = 0, trailOn = 0;                     // kinds the descriptor lists; kinds active at the last tick (probe trail.active)
	std::vector<int> rotors, jets;
	std::vector<float> rotSpun, jetSpun; std::vector<char> rotSet, jetSet;   // loco.lua SpinTo: the last target per piece
	int wingL = -1, wingR = -1;
	float vmax = 0, washAgl = 0;
	int base = -1;
	std::vector<int> wheels, legs, feet;
	std::vector<float> wside, wang, legOff, legAng;
	float stride = 1, wr = 1, swing = 0, lift = 0, bob = 0, h = 4, bobMul = 1, cap = 0, bankRate = 1;
	std::vector<Weapon> W;                     // by weapon number (1-based in Lua; index n-1)
	// state
	int lastFrame = 0, wakeAt = 0, idleTicks = 0, wakeTick = 0;
	bool first = true, sensed = false, sleeping = false, rollSentSet = false;
	float px = 0, py = 0, pz = 0, hd = 0, vs = 0, speed = 0, turn = 0, gh = 0, dt = 0.1f, t = 0;
	bool ghSet = false, land = true, inAir = false;
	float vsPrev = 0, acc = 0, agl = 0, liftSpin = 0;
	float trailTk[N_TRAIL] = {-1e9f, -1e9f, -1e9f, -1e9f, -1e9f, -1e9f};
	float phase = 0, cycle = 0, rate = 0, sl = 0, sr = 0, pitch = 0, roll = 0, rollSent = 0, pitchSent = 0;
	bool tiltSet = false; float tiltX = 0, tiltZ = 0, tiltH = 0, tiltP = 0, tiltR = 0;
};

class UnitAnim : public Rule {
public:
	UnitAnim() : Rule({"UnitAnim", 1, true, "scripts/pm/anim.lua (Loop), pm/loco.lua, pm/aim.lua (Tick, poll)", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		idleTicksN = std::max(1, cfg.GetInt("idle_ticks", 2));
		restDelay = KT1::ConfigFrames(cfg, "rest_delay_s", "rest_delay", 90);
		restRate = cfg.GetFloat("rest_rate", 1.5f);
		rollEps = cfg.GetFloat("roll_eps", 0.05f);
		wakeEvery = std::max(1, cfg.GetInt("wake_every", 3));
		spinRate = cfg.GetFloat("spin_rate", 0.6f);
		fullUnits = std::max(1, cfg.GetInt("full_units", 250));
		maxMs = cfg.GetInt("max_ms", 500);
	}

	// Without Lua driving (an older game), the rule ticks here with its own count. With Run (pm framework 2026-11-27), the ticks run
	// where the Lua Loop threads would wake (the unit script framework's GameFrame) with the Lua's own tick length and unit count.
	void GameFrame(int frame) override {
		if (luaDriven || ids.empty()) return;
		const int n = (int)unitHandler.GetActiveUnits().size();
		tickMs = (n <= fullUnits) ? 100 : std::min(maxMs, (int)math::floor(100.0f * n / fullUnits));
		unitCount = n;
		TickDue(frame);
	}
	void TickDue(int frame) {
		for (size_t i = 0; i < ids.size(); ) {
			const int id = ids[i];
			Unit& U = units[id];
			CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr || u != U.owner || u->script == nullptr) { U.owner = nullptr; ids.erase(ids.begin() + i); continue; }
			++i;
			if (frame < U.wakeAt) continue;
			Tick(U, u, frame);
		}
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Add", LAdd);
		LuaPushRawNamedCFunc(L, "Remove", LRemove);
		LuaPushRawNamedCFunc(L, "Wake", LWake);
		LuaPushRawNamedCFunc(L, "Start", LStart);
		LuaPushRawNamedCFunc(L, "Run", LRun);
		LuaPushRawNamedCFunc(L, "Aimed", LAimed);
		LuaPushRawNamedCFunc(L, "AimWait", LAimWait);
		LuaPushRawNamedCFunc(L, "State", LState);
		LuaPushRawNamedCFunc(L, "Events", LEvents);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
	}

private:
	static constexpr size_t MAX_EVENTS = 4 * 4096;   // DESIGN: dust beyond this between pulls is dropped (look only)

	static constexpr int FRAME_MS = 1000 / GAME_SPEED;   // base unit_script.lua Sleep: floor(1000 / gameSpeed), 33 at 30 Hz
	static constexpr double FRAME_LEN = double(int(1000000.0 / GAME_SPEED)) / 1000.0;   // the stagger frame length, ms to 3 decimals: 33.333 at 30 Hz (the old literal), 100 at 10 (anim.lua: floor(1000000 / Game.gameSpeed) / 1000)
	static int SleepFrames(float ms) { return std::max(1, (int)math::floor(ms / float(FRAME_MS))); }   // base unit_script.lua Sleep

	// ---- the tick (anim.lua Loop body) ----------------------------------------------------------------------------------------
	void Tick(Unit& U, CUnit* u, int f) {
		CUnitScript* S = u->script;
		Sense(U, u, f);
		Loco(U, S);
		AimTick(U, S, f);
		// sleep (anim.lua Loop tail)
		float ms = U.quiet ? 1000.0f : (float)tickMs;
		if (U.canIdle && ms > 100) {
			if (math::fabs(U.vs) <= 0.3f && math::fabs(U.turn) <= 0.05f && AimIdle(U) && !u->beingBuilt
				&& (!U.air || (U.land && U.agl <= 1 && U.liftSpin < 0.02f && math::fabs(U.pitch) < 0.05f && math::fabs(U.roll) < 0.05f)))   // an aircraft idles landed, spun down
				++U.idleTicks; else U.idleTicks = 0;
			if (U.idleTicks >= idleTicksN) {
				ms = std::min(8000.0f, std::max(2000.0f, 2.0f * unitCount));
				U.sleeping = true;
			}
		}
		if (U.first) {
			U.first = false;
			const int frames = (int)math::floor(ms / float(FRAME_LEN));
			if (frames > 1) ms = std::max(1.0f, ms - (U.owner->id % frames) * float(FRAME_LEN));
		}
		U.wakeAt = f + SleepFrames(ms);
	}

	void Sense(Unit& U, CUnit* u, int f) {
		const float dt = std::max(1, f - U.lastFrame) / float(GAME_SPEED);
		U.dt = dt;
		U.lastFrame = f;
		const float x = u->pos.x, y = u->pos.y, z = u->pos.z;
		const float hd = (float)u->heading * TAU / 65536.0f;
		if (!U.sensed) { U.sensed = true; U.px = x; U.py = y; U.pz = z; U.hd = hd; U.vs = 0; }
		const float dx = x - U.px, dz = z - U.pz;
		const float dist = math::sqrt(dx * dx + dz * dz);
		const float along = dx * math::sin(hd) + dz * math::cos(hd);
		const float vsPrev = U.vs;
		U.vsPrev = vsPrev;
		U.vs = (along >= 0 ? dist : -dist) / dt;
		U.speed = dist / dt;
		U.turn = Wrap(hd - U.hd) / dt;
		U.acc = (U.vs - vsPrev) / dt;
		U.px = x; U.py = y; U.pz = z; U.hd = hd;
		if (!U.ghSet || dx != 0 || dz != 0) { U.gh = (float)CGround::GetHeightReal((float)x, (float)z, true); U.ghSet = true; }
		U.agl = y - U.gh;
		U.inAir = U.air && U.agl > 3;                // anim.lua Sense: canFly and agl > 3 is "air", else gh >= 0 is "land"
		U.land = !U.inAir && U.gh >= 0;
	}

	void GroundTilt(Unit& U, float& p, float& r) {
		if (U.tiltSet && U.tiltX == U.px && U.tiltZ == U.pz && U.tiltH == U.hd) { p = U.tiltP; r = U.tiltR; return; }
		const float3 n = CGround::GetSmoothNormal((float)U.px, (float)U.pz, true);
		const float nx = n.x, ny = n.y, nz = n.z;
		if (ny < 0.01f) { p = 0; r = 0; }
		else {
			const float fx = math::sin(U.hd), fz = math::cos(U.hd);
			const float rx = -math::cos(U.hd), rz = math::sin(U.hd);
			const float up = -(nx * fx + nz * fz) / ny;
			const float right = -(nx * rx + nz * rz) / ny;
			p = Deg(math::atan(up)); r = Deg(math::atan(-right));
		}
		U.tiltSet = true; U.tiltX = U.px; U.tiltZ = U.pz; U.tiltH = U.hd; U.tiltP = p; U.tiltR = r;
	}

	// loco.lua SpinTo: a Spin only when the target moved by SPIN_EPS (0.5 rad/s), or to stop
	static void SpinTo(CUnitScript* S, int p, int axis, float speed, float acc, float& last, char& set) {
		if (p < 0) return;
		if (set && math::fabs(last - speed) < 0.5f && (speed != 0 || last == 0)) return;
		if (speed == 0) S->StopSpin(p, axis, acc); else S->Spin(p, axis, speed, acc);
		last = speed; set = 1;
	}

	// pm/loco.lua Tick, the parts an engine unit has (Turn(p, axis, dest, speed) in Lua is S->Turn(p, axis, speed, dest))
	void Loco(Unit& U, CUnitScript* S) {
		const float dt = U.dt;
		U.t += dt;
		const bool moving = math::fabs(U.vs) > 0.3f;
		const bool turning = math::fabs(U.turn) > 0.05f;
		const bool rolling = U.loco == L_TREADS || U.loco == L_WHEELS;
		if (rolling || U.loco == L_LEGS) {
			float rate = 0, sl = 0, sr = 0;
			if (moving) { rate = U.vs / U.stride; sl = rate; sr = rate; }
			else if (U.spin && turning) {
				const float s = U.turn > 0 ? 1 : -1;
				rate = s * spinRate; sl = -s * spinRate; sr = s * spinRate;
			}
			U.phase = LuaMod(U.phase + rate * dt, 1.0f);
			U.cycle = U.cycle + rate * dt;
			U.rate = rate; U.sl = sl; U.sr = sr;
			if (rolling) {
				for (size_t i = 0; i < U.wheels.size(); ++i) {
					const float side = U.wside[i] > 0 ? sl : sr;
					if (side == 0) continue;
					U.wang[i] = U.wang[i] + side * dt * (U.stride / U.wr);
					S->Turn(U.wheels[i], 0, 40.0f, (float)(-U.wang[i]));
				}
			} else {
				float bobAmp = 0;
				if (rate != 0) {
					for (size_t i = 0; i < U.legs.size(); ++i) {
						const float ph = LuaMod(U.phase + U.legOff[i], 1.0f);
						const float a = math::sin(ph * TAU) * U.swing;
						S->Turn(U.legs[i], 0, (float)std::max(1.0f, math::fabs(a - U.legAng[i]) / dt * 1.2f), (float)a);
						U.legAng[i] = a;
						if (i < U.feet.size() && U.feet[i] >= 0) {
							const float l = std::max(0.0f, math::cos(ph * 2.0f * PI)) * U.lift;   // Gait.Lift: cos(ph * 2 * math.pi), two roundings
							S->Turn(U.feet[i], 0, (float)std::max(1.0f, math::fabs(l) / dt * 1.5f), (float)l);
						}
					}
					bobAmp = U.bob * U.h * math::fabs(math::sin(U.phase * TAU * U.bobMul));
				}
				if (U.base >= 0) S->Move(U.base, 1, 20.0f, (float)bobAmp);
			}
		}
		// the body (R8)
		float tp = 0, tr = 0;
		if (U.body == B_TERRAIN && U.land) GroundTilt(U, tp, tr);
		else if (U.body == B_BANK) {
			const float share = std::min(1.0f, math::fabs(U.turn) / U.bankRate);
			tr = (U.turn > 0 ? -1 : 1) * U.cap * share;
			if (math::fabs(U.turn) < 0.01f) tr = 0;
		}
		if (U.pfwd && U.loco == L_ROTOR) {                // nose down while speeding up
			const float pit = (U.acc > 0.5f) ? -std::min(U.cap > 0 ? U.cap : 10.0f, U.acc * 1.2f) : 0.0f;
			tp = tp + pit;
		}
		const float k = std::min(1.0f, dt / 0.25f);
		U.pitch = U.pitch + (tp - U.pitch) * k;
		U.roll = U.roll + (tr - U.roll) * k;
		if (U.base >= 0) {
			if (!(U.rollSentSet && math::fabs(U.roll - U.rollSent) < rollEps && math::fabs(U.pitch - U.pitchSent) < rollEps)) {
				U.rollSentSet = true; U.rollSent = U.roll; U.pitchSent = U.pitch;
				S->Turn(U.base, 2, 6.0f, (float)Rad(U.roll));
				S->Turn(U.base, 0, 6.0f, (float)(-Rad(U.pitch)));
			}
		}
		// rotor and jet aircraft (loco.lua R6; tilt-rotor units stay with the Lua)
		if (U.loco == L_ROTOR || U.loco == L_JET) {
			const bool up = U.inAir || U.agl > 1;
			const float target = up ? 1.0f : 0.0f;
			U.liftSpin = U.liftSpin + (target - U.liftSpin) * std::min(1.0f, dt / (up ? 0.4f : 1.5f));
			for (size_t i = 0; i < U.rotors.size(); ++i)
				SpinTo(S, U.rotors[i], 1, U.liftSpin > 0.02f ? 38 * U.liftSpin : 0.0f, U.liftSpin > 0.02f ? 8.0f : 6.0f, U.rotSpun[i], U.rotSet[i]);
			for (size_t i = 0; i < U.jets.size(); ++i)
				SpinTo(S, U.jets[i], 2, moving ? 60.0f : 8.0f, 20.0f, U.jetSpun[i], U.jetSet[i]);
			if (U.wings && up) {
				const float a = Rad(28) * math::sin(U.t * (moving ? 9 : 6));
				S->Turn(U.wingL, 2, 8.0f, (float)a);
				S->Turn(U.wingR, 2, 8.0f, (float)(-a));
			}
		}
		// trails (loco.lua Trails): the active kinds, then dust and rotor downwash every wake_every moving ticks
		int on = 0;
		for (int b = 0; b < N_TRAIL; ++b) {
			const int kind = 1 << b;
			if (!(U.trails & kind)) continue;
			bool ok = false;
			if (kind == T_DUST || kind == T_TREADS || kind == T_FEET) ok = U.land;
			else if (kind == T_CONTRAIL) ok = U.inAir && U.speed > 0.5f * std::max(U.vmax, 10.0f);
			else ok = U.inAir;   // exhaust, thruster_glow
			if (ok) {
				if (moving) U.trailTk[b] = U.t;
				if (U.t - U.trailTk[b] < 1.0f) on |= kind;
			}
		}
		U.trailOn = on;
		if (moving) {
			++U.wakeTick;
			if (U.wakeTick % wakeEvery == 0) {
				if ((on & T_DUST) && events.size() + 4 <= MAX_EVENTS) {
					events.push_back(U.owner->id); events.push_back(U.px); events.push_back(U.gh); events.push_back(U.pz);
				}
				if (U.loco == L_ROTOR && U.inAir && U.agl < U.washAgl && events.size() + 4 <= MAX_EVENTS) {   // downwash: the id negated
					events.push_back(-U.owner->id); events.push_back(U.px); events.push_back(U.gh); events.push_back(U.pz);
				}
			}
		}
	}

	// pm/aim.lua Tick: the rest pose (R9)
	void AimTick(Unit& U, CUnitScript* S, int f) {
		for (Weapon& w : U.W) {
			if (f - w.aimFrame > restDelay && !w.rested && w.aimFrame > -9000) {
				w.rested = true;
				if (w.turret >= 0) S->Turn(w.turret, 1, restRate, 0.0f);
				if (w.barrel >= 0) S->Turn(w.barrel, 0, restRate, 0.0f);
				if (w.barrelb >= 0) S->Turn(w.barrelb, 0, restRate, 0.0f);
			} else if (f - w.aimFrame <= restDelay) w.rested = false;
		}
	}
	static bool AimIdle(const Unit& U) {
		for (const Weapon& w : U.W) if (!(w.rested || w.aimFrame <= -9000)) return false;
		return true;
	}

	// a wake (Lua Wake: Signal, then StartThread(Loop)) ticks at once, as the new Loop thread runs to its first Sleep inside the call
	// (exactness 2026-11-27: waiting for the rule's next GameFrame put the tick a frame late when the wake came after it)
	void WakeUnit(Unit& U) {
		if (!U.sleeping) return;
		U.sleeping = false; U.idleTicks = 0;
		U.first = true;   // the new Lua Loop thread starts with first = true: its first sleep is staggered by id again
		CUnit* u = U.owner;
		if (u != nullptr && u->script != nullptr) Tick(U, u, gs->frameNum);
		else U.wakeAt = gs->frameNum;
	}

	// ---- Lua ---------------------------------------------------------------------------------------------------------------
	static UnitAnim* Self() { return static_cast<UnitAnim*>(Registry::Get().Find("UnitAnim")); }
	static UnitAnim* Synced(lua_State* L) {
		UnitAnim* r = Self();
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}
	Unit* Get(int id) {
		if (id < 0 || id >= (int)units.size()) return nullptr;
		Unit& U = units[id];
		if (U.owner == nullptr || unitHandler.GetUnit(id) != U.owner) return nullptr;
		return &U;
	}

	static float Num(lua_State* L, int t, const char* k, float d) {
		lua_getfield(L, t, k);
		const float v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : d;
		lua_pop(L, 1);
		return v;
	}
	static bool Flag(lua_State* L, int t, const char* k) {
		lua_getfield(L, t, k);
		const bool v = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
		return v;
	}
	static int Str(lua_State* L, int t, const char* k, const char* const* names, int n, int d) {
		lua_getfield(L, t, k);
		int v = d;
		if (lua_isstring(L, -1)) { const char* s = lua_tostring(L, -1); for (int i = 0; i < n; ++i) if (strcmp(s, names[i]) == 0) v = i; }
		lua_pop(L, 1);
		return v;
	}
	static void List(lua_State* L, int t, const char* k, std::vector<float>& out, float sub) {
		out.clear();
		lua_getfield(L, t, k);
		if (lua_istable(L, -1)) {
			const int n = (int)lua_objlen(L, -1);
			for (int i = 1; i <= n; ++i) { lua_rawgeti(L, -1, i); out.push_back(lua_isnumber(L, -1) ? lua_tonumber(L, -1) - sub : -1 - sub); lua_pop(L, 1); }
		}
		lua_pop(L, 1);
	}
	static void Pieces(lua_State* L, int t, const char* k, std::vector<int>& out) {
		std::vector<float> d; List(L, t, k, d, 1.0f); out.assign(d.begin(), d.end());
		for (int& p : out) if (p < 0) p = -1;
	}

	static int LAdd(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		const int id = luaL_checkint(L, 1);
		luaL_checktype(L, 2, LUA_TTABLE);
		CUnit* u = unitHandler.GetUnit(id);
		if (u == nullptr) return 0;
		if (id >= (int)r->units.size()) r->units.resize(std::max<size_t>(id + 1, unitHandler.MaxUnits()));
		Unit& U = r->units[id];
		U = Unit();
		U.owner = u;
		static const char* LOCO[] = {"fixed", "treads", "wheels", "legs", "rotor", "jet"};
		static const char* BODY[] = {"static", "follow_terrain", "bank"};
		U.loco = Str(L, 2, "loco", LOCO, 6, L_FIXED);
		U.body = Str(L, 2, "body", BODY, 3, B_STATIC);
		U.spin = Flag(L, 2, "spin"); U.dust = Flag(L, 2, "dust"); U.quiet = Flag(L, 2, "quiet");
		U.air = Flag(L, 2, "air"); U.pfwd = Flag(L, 2, "pfwd"); U.wings = Flag(L, 2, "wings");
		U.canIdle = U.air || U.body != B_BANK;       // anim.lua: a banking body idles only on an aircraft
		U.trails = (int)Num(L, 2, "trail", 0);
		if (U.dust) U.trails |= T_DUST;             // an older script sends only the dust flag
		U.vmax = Num(L, 2, "vmax", 0); U.washAgl = Num(L, 2, "wash_agl", 0);
		U.wingL = (int)Num(L, 2, "wing_l", 0) - 1; U.wingR = (int)Num(L, 2, "wing_r", 0) - 1;
		Pieces(L, 2, "rotors", U.rotors); Pieces(L, 2, "jets", U.jets);
		U.rotSpun.assign(U.rotors.size(), 0); U.rotSet.assign(U.rotors.size(), 0);
		U.jetSpun.assign(U.jets.size(), 0); U.jetSet.assign(U.jets.size(), 0);
		U.base = (int)Num(L, 2, "base", 0) - 1;
		Pieces(L, 2, "wheels", U.wheels); Pieces(L, 2, "legs", U.legs); Pieces(L, 2, "feet", U.feet);
		List(L, 2, "wside", U.wside, 0); List(L, 2, "leg_off", U.legOff, 0);
		U.wside.resize(U.wheels.size(), 1); U.wang.assign(U.wheels.size(), 0);
		U.legOff.resize(U.legs.size(), 0); U.legAng.assign(U.legs.size(), 0);
		U.stride = Num(L, 2, "stride", 1); U.wr = Num(L, 2, "wr", 1); U.swing = Num(L, 2, "swing", 0); U.lift = Num(L, 2, "lift", 0);
		U.bob = Num(L, 2, "bob", 0); U.h = Num(L, 2, "h", 4); U.bobMul = Num(L, 2, "bob_mul", 1);
		U.cap = Num(L, 2, "cap", 0); U.bankRate = Num(L, 2, "bank_rate", 1);
		U.t = Num(L, 2, "t", 0);
		U.lastFrame = gs->frameNum;
		U.wakeAt = std::numeric_limits<int>::max();   // Start (from script.Create) runs the first tick, as the Lua Create starts the Loop
		lua_getfield(L, 2, "weapons");
		if (lua_istable(L, -1)) {
			const int w = lua_gettop(L);
			const int n = (int)lua_objlen(L, w);
			U.W.resize(n);
			for (int i = 1; i <= n; ++i) {
				lua_rawgeti(L, w, i);
				if (lua_istable(L, -1)) {
					const int e = lua_gettop(L);
					Weapon& W = U.W[i - 1];
					W.yaw = (int)Num(L, e, "yaw", 0) - 1; W.pit = (int)Num(L, e, "pit", 0) - 1;
					W.turret = (int)Num(L, e, "turret", 0) - 1; W.barrel = (int)Num(L, e, "barrel", 0) - 1; W.barrelb = (int)Num(L, e, "barrelb", 0) - 1;
					W.rest = Num(L, e, "rest", 0); W.yawRate = Num(L, e, "yaw_rate", 1); W.pitchRate = Num(L, e, "pitch_rate", 1); W.tol = Num(L, e, "tol", 0);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);
		auto it = std::lower_bound(r->ids.begin(), r->ids.end(), id);
		if (it == r->ids.end() || *it != id) r->ids.insert(it, id);
		lua_pushboolean(L, 1);
		return 1;
	}
	static int LRemove(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		const int id = luaL_checkint(L, 1);
		auto it = std::lower_bound(r->ids.begin(), r->ids.end(), id);
		if (it != r->ids.end() && *it == id) r->ids.erase(it);
		if (id >= 0 && id < (int)r->units.size()) r->units[id].owner = nullptr;
		return 0;
	}
	// script.Create: the first tick now (the Lua Loop thread started in Create runs its first tick before Create returns)
	static int LStart(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		Unit* U = r->Get(luaL_checkint(L, 1));
		if (U == nullptr || U->owner->script == nullptr) return 0;
		r->Tick(*U, U->owner, gs->frameNum);
		return 0;
	}
	// GG.pmTick's ms and n, from the unit script framework's GameFrame: the due ticks now
	static int LRun(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		r->luaDriven = true;
		r->tickMs = (int)luaL_checknumber(L, 1);
		r->unitCount = (int)luaL_checknumber(L, 2);
		if (!r->ids.empty()) r->TickDue(gs->frameNum);
		return 0;
	}
	static int LWake(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		if (Unit* U = r->Get(luaL_checkint(L, 1))) r->WakeUnit(*U);
		return 0;
	}
	static int LAimed(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		Unit* U = r->Get(luaL_checkint(L, 1));
		const int n = luaL_checkint(L, 2);
		if (U == nullptr || n < 1 || n > (int)U->W.size()) return 0;
		U->W[n - 1].aimFrame = gs->frameNum;
		r->WakeUnit(*U);
		return 0;
	}
	// pm/aim.lua AimWeapon poll with the turn bound (aim_bound): 0 = within tolerance, else the frames to sleep
	static int LAimWait(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		CUnit* u = unitHandler.GetUnit(luaL_checkint(L, 1));
		Unit* U = r->Get(luaL_checkint(L, 1));
		const int n = luaL_checkint(L, 2);
		if (u == nullptr || U == nullptr || n < 1 || n > (int)U->W.size()) return 0;
		const float heading = luaL_checknumber(L, 3), pitch = luaL_checknumber(L, 4);
		const Weapon& w = U->W[n - 1];
		CUnitScript* S = u->script;
		const float ptol = std::max(w.tol, 0.02f);
		float ey = 0, ep = 0;
		bool ok = true;
		if (w.yaw >= 0) { const auto* p = S->SafeGetPiece(w.yaw); ey = math::fabs(Wrap((p ? (float)p->GetRotation().y : 0.0f) - heading)); if (ey > w.tol) ok = false; }
		if (w.pit >= 0) { const auto* p = S->SafeGetPiece(w.pit); ep = math::fabs(Wrap(-(p ? (float)p->GetRotation().x : 0.0f) + w.rest - pitch)); if (ep > ptol) ok = false; }
		if (ok) { lua_pushnumber(L, 0); return 1; }
		float lb = 0;
		if (w.yaw >= 0) lb = std::max(lb, (ey - w.tol) / (w.yawRate / float(GAME_SPEED)));
		if (w.pit >= 0) lb = std::max(lb, (ep - ptol) / (w.pitchRate / float(GAME_SPEED)));
		const float skip = math::floor(lb) - 1;
		lua_pushnumber(L, (skip >= 2 && skip < 1e6f) ? SleepFrames(skip * FRAME_MS) : 1);
		return 1;
	}
	static int LState(lua_State* L) {
		UnitAnim* r = Self();
		if (r == nullptr || !r->Enabled()) return 0;
		const Unit* U = r->Get(luaL_checkint(L, 1));
		if (U == nullptr) return 0;
		lua_newtable(L);
		lua_pushnumber(L, U->phase); lua_setfield(L, -2, "phase");
		lua_pushnumber(L, U->rate);  lua_setfield(L, -2, "rate");
		lua_pushnumber(L, U->cycle); lua_setfield(L, -2, "cycle");
		lua_pushnumber(L, U->sl);    lua_setfield(L, -2, "side_l");
		lua_pushnumber(L, U->sr);    lua_setfield(L, -2, "side_r");
		lua_pushnumber(L, U->pitch); lua_setfield(L, -2, "pitch");
		lua_pushnumber(L, U->roll);  lua_setfield(L, -2, "roll");
		lua_pushnumber(L, U->vs);    lua_setfield(L, -2, "vs");
		lua_pushnumber(L, U->turn);  lua_setfield(L, -2, "turn");
		lua_pushboolean(L, U->sleeping); lua_setfield(L, -2, "idle");
		lua_pushnumber(L, U->idleTicks); lua_setfield(L, -2, "idle_ticks");
		lua_pushnumber(L, U->trailOn);   lua_setfield(L, -2, "trail");
		lua_pushnumber(L, U->liftSpin);  lua_setfield(L, -2, "lift_spin");
		return 1;
	}
	static int LEvents(lua_State* L) {
		UnitAnim* r = Synced(L);
		if (r == nullptr) return 0;
		lua_createtable(L, (int)r->events.size(), 0);
		for (size_t i = 0; i < r->events.size(); ++i) { lua_pushnumber(L, r->events[i]); lua_rawseti(L, -2, (int)i + 1); }
		r->events.clear();
		return 1;
	}
	static int LConfig(lua_State* L) {
		const UnitAnim* r = Self();
		if (r == nullptr) return 0;
		lua_newtable(L);
		lua_pushnumber(L, r->idleTicksN); lua_setfield(L, -2, "idle_ticks");
		lua_pushnumber(L, r->restDelay);  lua_setfield(L, -2, "rest_delay");
		lua_pushnumber(L, r->restRate);   lua_setfield(L, -2, "rest_rate");
		lua_pushnumber(L, r->rollEps);    lua_setfield(L, -2, "roll_eps");
		lua_pushnumber(L, r->wakeEvery);  lua_setfield(L, -2, "wake_every");
		lua_pushnumber(L, r->spinRate);   lua_setfield(L, -2, "spin_rate");
		lua_pushnumber(L, r->fullUnits);  lua_setfield(L, -2, "full_units");
		lua_pushnumber(L, r->maxMs);      lua_setfield(L, -2, "max_ms");
		lua_pushboolean(L, 1);            lua_setfield(L, -2, "air");   // rotor and jet units (2026-11-27)
		return 1;
	}

	bool luaDriven = false;
	int idleTicksN = 2, restDelay = 90, wakeEvery = 3, fullUnits = 250, maxMs = 500;
	float restRate = 1.5f;
	float rollEps = 0.05f, spinRate = 0.6f;
	int tickMs = 100, unitCount = 0;
	std::vector<Unit> units;      // per unit id
	std::vector<int> ids;         // registered ids, ascending
	std::vector<float> events;   // unitID (negated: rotor downwash, else dust), x, ground height, z
};

} // namespace

std::unique_ptr<Rule> MakeUnitAnim() { return std::make_unique<UnitAnim>(); }

} // namespace KT1
