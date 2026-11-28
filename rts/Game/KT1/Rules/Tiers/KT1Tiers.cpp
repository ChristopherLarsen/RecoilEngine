/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Tiers (version 1). Idle units far from action sleep between tier visits (rate tiers, PRF-002 v3 R7).
//
// Replaces:   nothing (an engine-only saving; Lua gadgets may read IsAsleep to skip their own per-unit work)
// Switch:     mod option kt1_tiers (default off)
// Config:     gamedata/kt1/tiers.lua
//               far_period_s   seconds between tier visits; frames = round(s x GAME_SPEED), at least 2 (DESIGN, 0.5: 15 at 30 Hz, 5 at 10 Hz)
//               margin_elmos   action distance = max(longest weapon range, sight) + this (DESIGN rules.lua sim.action_distance_margin 10 MU x 8 = 80)
//               cell_elmos     occupancy grid cell (build choice, 512)
// Timer:      KT1::Tiers
// Lua:        Spring.KT1.Tiers.IsAsleep(unitID) -> bool;  Spring.KT1.Tiers.Stats() -> asleep, idleNear, total (last frame)
// Rule:       at frame f, before Lua GameFrame and the unit handler update (Game.cpp order), the byte for f is set when the unit is idle (R2: empty queue, not
//             built, stunned, skidding, transported, carrying or targeting, no speed, move type not active, no self-d, not a
//             builder), unchanged since the previous frame end (position, heading, health, build and capture progress, team),
//             far from action (no enemy ally team in any grid cell its action square touches) and f is not its visit
//             ((f+id) % period != 0). Hooks skip the tiered work while the byte is set and no live wake holds (KT1Tiers.h).
// Determinism: main thread, active-unit order, synced state only; the grid ORs ally-team bits (order-free); integer cells.
//             Not bit-identical with the rule off: a skipped weapon SlowUpdate draws no gsRNG.
// Doc:        game-build docs/engine/RATE-TIERS-PLAN.md

#include <memory>
#include <vector>

#include "KT1Tiers.h"
#include "KT1TiersCore.h"

#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/ReadMap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/MoveTypes/MoveType.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "Sim/Weapons/Weapon.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace Tiers {
	bool on = false;
	std::vector<uint8_t> asleep;

	static TiersCore::Live ReadLive(const CUnit* u) {
		TiersCore::Live l;
		l.queueEmpty = (u->commandAI == nullptr || u->commandAI->commandQue.empty());
		l.beingBuilt = u->beingBuilt;
		l.transported = (u->GetTransporter() != nullptr);
		l.carrying = !u->transportedUnits.empty();
		l.stunned = u->IsStunned();
		l.hasTarget = u->HaveTarget();
		l.moving = (u->speed.x != 0.0f || u->speed.y != 0.0f || u->speed.z != 0.0f)
			|| (u->moveType != nullptr && u->moveType->progressState == AMoveType::Active);
		l.lastAttackFrame = u->lastAttackFrame;
		l.lastNanoAdd = u->lastNanoAdd;
		return l;
	}

	bool LiveWake(const CUnit* u) { return TiersCore::Wake(ReadLive(u), gs->frameNum); }

	bool Asleep(const CUnit* u) {
		if (!on) return false;
		const unsigned id = u->id;
		return id < asleep.size() && asleep[id] != 0 && !u->isDead && !LiveWake(u);
	}
}

namespace {

struct Snap {
	const CUnit* unit = nullptr;
	float3 pos;
	short heading = 0;
	float health = 0.0f, build = 0.0f, capture = 0.0f;
	int team = -1;
};

class TiersRule : public Rule {
public:
	TiersRule() : Rule({"Tiers", 1, true, "", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		periodSeconds = cfg.GetFloat("far_period_s", 0.5f);
		marginElmos = cfg.GetFloat("margin_elmos", 80.0f);
		cellElmos = std::max(64.0f, cfg.GetFloat("cell_elmos", 512.0f));
	}

	void Start() override {
		period = TiersCore::Period(periodSeconds, GAME_SPEED);
		grid.Init(mapDims.mapx * SQUARE_SIZE, mapDims.mapy * SQUARE_SIZE, cellElmos);
		Tiers::asleep.assign(unitHandler.MaxUnits(), 0);
		snaps.assign(unitHandler.MaxUnits(), Snap{});
		Tiers::on = true;
		LOG("[KT1] Tiers: period %d frames, margin %.0f elmos, cell %.0f", period, marginElmos, cellElmos);
	}

	void GameFrame(int frame) override {
		const auto& units = unitHandler.GetActiveUnits();
		const int nAlly = teamHandler.ActiveAllyTeams();

		// enemy masks per ally team; more than 64 ally teams: every unit counts as near (never sleeps)
		enemyMask.assign(std::max(nAlly, 1), 0);
		for (int a = 0; a < nAlly && a < 64; ++a)
			for (int b = 0; b < nAlly && b < 64; ++b)
				if (!teamHandler.Ally(a, b)) enemyMask[a] |= (1ull << b);

		grid.Clear();
		for (const CUnit* u : units)
			if (!u->isDead) grid.Add(u->pos.x, u->pos.z, u->allyteam);

		const int next = frame;   // the rule runs before the unit update of the same frame
		nAsleep = nIdleNear = 0;
		nTotal = static_cast<int>(units.size());
		for (const CUnit* u : units) {
			const unsigned id = u->id;
			Snap& s = snaps[id];

			TiersCore::Facts f;
			f.changed = (s.unit != u || s.pos != u->pos || s.heading != u->heading || s.health != u->health
				|| s.build != u->buildProgress || s.capture != u->captureProgress || s.team != u->team);
			s.unit = u; s.pos = u->pos; s.heading = u->heading; s.health = u->health;
			s.build = u->buildProgress; s.capture = u->captureProgress; s.team = u->team;

			f.idle = Idle(u);
			if (f.idle && !f.changed) {
				const float reach = std::max(u->maxRange, u->unitDef->losRadius) + marginElmos;
				f.far = (nAlly <= 64) && !grid.EnemyWithin(u->pos.x, u->pos.z, reach, enemyMask[u->allyteam]);
				nIdleNear += !f.far;
			}
			const bool sleep = TiersCore::Decide(f, next, id, period);
			Tiers::asleep[id] = sleep;
			nAsleep += sleep;
		}
		if (frame % 300 == 0)
			LOG("[KT1TIERS] f=%d asleep %d idle-near %d of %d", frame, nAsleep, nIdleNear, nTotal);
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "IsAsleep", LIsAsleep);
		LuaPushRawNamedCFunc(L, "Stats", LStats);
	}

private:
	static bool Idle(const CUnit* u) {
		if (u->isDead || u->IsSkidding() || u->selfDCountdown > 0 || u->unitDef->IsBuilderUnit())
			return false;
		if (Tiers::LiveWake(u))
			return false;
		for (const CWeapon* w : u->weapons)
			if (w->HaveTarget() || w->salvoLeft > 0) return false;
		return true;
	}

	static TiersRule* Self() { return static_cast<TiersRule*>(Registry::Get().Find("Tiers")); }

	static int LIsAsleep(lua_State* L) {
		const CUnit* u = unitHandler.GetUnit(luaL_checkint(L, 1));
		lua_pushboolean(L, u != nullptr && Tiers::Asleep(u));
		return 1;
	}
	static int LStats(lua_State* L) {
		const TiersRule* r = Self();
		lua_pushnumber(L, r ? r->nAsleep : 0);
		lua_pushnumber(L, r ? r->nIdleNear : 0);
		lua_pushnumber(L, r ? r->nTotal : 0);
		return 3;
	}

	float periodSeconds = 0.5f, marginElmos = 80.0f, cellElmos = 512.0f;
	int period = 15;
	int nAsleep = 0, nIdleNear = 0, nTotal = 0;
	TiersCore::Grid grid;
	std::vector<uint64_t> enemyMask;
	std::vector<Snap> snaps;
};

} // namespace

KT1_REGISTER_SYSTEM(Tiers, 900, TiersRule)

} // namespace KT1
