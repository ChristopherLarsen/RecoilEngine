/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Core/Targeting, engine-free: the cases of cmb_targeting.lua and unit_cmd_gun.lua around the engine step (LUA-FREE S19).

#include "../KT1Test.h"

#include <map>

#include "Game/KT1/Core/Targeting/TargetState.h"

using namespace KT1::Core::Targeting;

namespace {

struct FakeUnit {
	int32_t def = 1, team = 0, ally = 0;
	float pos[3] = {0, 0, 0};
	float height = 10, radius = 8;
	bool dead = false, arriving = false;
	Layer layer = Layer::Land;
	int32_t los = 1;              // bits as seen by ally 1 (the other side)
	int32_t fire = 2;
	CommandView cmd;
	std::map<int32_t, float> range;   // by weapon number
	int32_t weaponTarget = NONE;
};

class FakeWorld : public TargetingWorld {
public:
	std::map<int32_t, FakeUnit> u;
	float ground = 0;
	std::vector<int32_t> order;       // the cylinder query's order
	std::map<int32_t, int32_t> chosen;
	int32_t refused = 0;
	Refusal lastRefusal = Refusal::None;
	int32_t setTargets = 0;

	FakeUnit* F(int32_t id) { auto it = u.find(id); return it == u.end() ? nullptr : &it->second; }
	bool ValidUnit(int32_t id) override { return F(id) != nullptr; }
	bool DeadUnit(int32_t id) override { return F(id) == nullptr || F(id)->dead; }
	int32_t UnitDef(int32_t id) override { return F(id) ? F(id)->def : NONE; }
	int32_t UnitTeam(int32_t id) override { return F(id) ? F(id)->team : NONE; }
	int32_t AllyTeam(int32_t id) override { return F(id) ? F(id)->ally : NONE; }
	bool Allied(int32_t a, int32_t b) override { return a == b; }
	bool Arriving(int32_t id) override { return F(id) && F(id)->arriving; }
	bool ScriptAI(int32_t) override { return false; }
	int32_t LosBits(int32_t id, int32_t) override { return F(id) ? F(id)->los : 0; }
	bool InLos(int32_t id, int32_t) override { return F(id) && (F(id)->los & 1); }
	bool Position(int32_t id, float (&p)[3]) override {
		if (!F(id)) return false;
		for (int i = 0; i < 3; ++i) p[i] = F(id)->pos[i];
		return true;
	}
	bool MidPosition(int32_t id, float (&p)[3]) override { return Position(id, p); }
	float Height(int32_t id) override { return F(id) ? F(id)->height : 0; }
	float Radius(int32_t id) override { return F(id) ? F(id)->radius : 0; }
	Layer MovementLayer(int32_t id) override { return F(id) ? F(id)->layer : Layer::Unknown; }
	float GroundHeight(float, float) override { return ground; }
	void UnitsInCylinder(float, float, float, std::vector<int32_t>& out) override { out = order; }
	void UnitsInRectangle(float, float, float, float, std::vector<int32_t>& out) override { out = order; }
	int32_t FireState(int32_t id) override { return F(id) ? F(id)->fire : 0; }
	CommandView CurrentCommand(int32_t id) override { return F(id) ? F(id)->cmd : CommandView{}; }
	bool WeaponRange(int32_t id, int32_t n, float& r) override {
		if (!F(id) || !F(id)->range.count(n)) return false;
		r = F(id)->range[n];
		return true;
	}
	int32_t WeaponTarget(int32_t id, int32_t) override { return F(id) ? F(id)->weaponTarget : NONE; }
	int32_t ChosenTarget(int32_t id, int32_t n) override { auto it = chosen.find(id * 100 + n); return it == chosen.end() ? NONE : it->second; }
	void SetWeaponTarget(int32_t id, int32_t, int32_t t) override { if (F(id)) F(id)->weaponTarget = t; ++setTargets; }
	void WriteRefusal(int32_t, Refusal r, int32_t) override { ++refused; lastRefusal = r; }
};

// defs: unit def 1 a tank (weapon def 1: scanned, ground, direct; weapon def 2: scanned, air), def 2 a plane (air),
// def 3 a command unit (weapon def 3: the command gun), def 4 a defence
TargetingDefs Defs() {
	TargetingDefs d;
	d.weapons.resize(5);
	d.units.resize(5);
	WeaponTargets& w1 = d.weapons[1];
	w1.scanned = true; w1.direct = true; w1.scanInterval = 0.3f; w1.track = 1.1f;
	w1.reach.surface = true;
	w1.priorities = {{"defence"}, {"mobile"}};
	WeaponTargets& w2 = d.weapons[2];
	w2.scanned = true; w2.scanInterval = 0.4f; w2.reach.air = true; w2.reach.noGround = true;
	w2.priorities = {{"any"}};
	d.weapons[3].commandGun = true;
	d.weapons[4].manualFire = true;
	d.weapons[4].manualReach.surface = true;
	d.units[1].mobile = true; d.units[1].armed = true; d.units[1].weapons[0] = 1; d.units[1].weapons[1] = 2;
	d.units[2].mobile = true; d.units[2].air = true;
	d.units[3].command = true; d.units[3].weapons[0] = 3; d.units[3].weapons[1] = 4;
	d.units[4].defence = true;
	BuildRanks(d);
	d.gunWeaponDef = 3;
	BuildGunRanks(d, {{"mobile"}, {"command"}, {"defence"}, {"any"}});
	return d;
}

TargetingParams Params(int32_t sps = 30) {
	TargetingConfig cfg;
	cfg.scan_extra_ticks = 1;
	cfg.engage_inset_mu = 0.75f;
	CommandIds ids;
	ids.attack = 20; ids.manualFire = 105; ids.insert = 1; ids.internalOption = 8;
	return MakeTargetingParams(cfg, ids, sps);
}

// shooter 1 (tank, team 0) at the origin; enemies of team 1 on +x
FakeWorld World() {
	FakeWorld w;
	w.u[1].range = {{1, 300}, {2, 400}};
	FakeUnit e;
	e.team = 1; e.ally = 1;
	w.u[10] = e; w.u[10].pos[0] = 100;
	w.u[11] = e; w.u[11].pos[0] = 150; w.u[11].def = 4;
	w.u[12] = e; w.u[12].pos[0] = 120; w.u[12].def = 2; w.u[12].pos[1] = 200;
	return w;
}

} // namespace

KT1_RATE_CASE("TGT-SCAN-STEPS", "SIM-RATE-001 R2: whole reference ticks rounded up, plus the extra tick, in steps") {
	const TargetingParams p = Params(testCase.StepsPerSecond());
	const int32_t perTick = testCase.StepsPerSecond() == 30 ? 3 : 1;
	KT1_CHECK_EQ(ScanSteps(p, 0.3f), (3 + 1) * perTick);   // 0.3 / 0.1 is 3, not 4
	KT1_CHECK_EQ(ScanSteps(p, 0.25f), (3 + 1) * perTick);
	KT1_CHECK_EQ(ScanSteps(p, 0.0f), (1 + 1) * perTick);
	KT1_CHECK_EQ(p.skipSteps, 5 * testCase.StepsPerSecond());
}

KT1_CASE("TGT-FIRST-SCAN", "first scans spread by unit and weapon within one delay") {
	const TargetingParams p = Params();
	KT1_CHECK_EQ(FirstScan(p, 5, 1, 12, 100), 100 + 1 + (5 * 7 + 1) % 12);
}

KT1_CASE("TGT-RANK", "CMB-004 R4: the first group whose classes the def all has; none: 0") {
	const TargetingDefs d = Defs();
	KT1_CHECK_EQ(d.Rank(1, 4), 1);   // defence
	KT1_CHECK_EQ(d.Rank(1, 1), 2);   // mobile
	KT1_CHECK_EQ(d.Rank(1, 3), 0);   // a command unit is not "mobile" by the project's names
	KT1_CHECK_EQ(d.Rank(2, 3), 1);   // any
	KT1_CHECK_EQ(d.gunRanks[1], 1);
	KT1_CHECK_EQ(d.gunRanks[3], 2);
	UnitTargets own;
	own.classes = {"heavy"};
	KT1_CHECK(HasClass(own, "heavy", true));
	KT1_CHECK(!HasClass(own, "heavy", false));   // the command gun knows the project's names only
}

KT1_CASE("TGT-LAYER", "CMB-018, CMB-019: layers and their refusal reasons") {
	LayerReach ground; ground.surface = true;
	LayerReach torp; torp.under = true; torp.below = true;
	KT1_CHECK(LayerCheck(ground, false, Layer::Land, 0, 10) == Refusal::None);
	KT1_CHECK(LayerCheck(ground, true, Layer::Land, 0, 10) == Refusal::NoAirTarget);
	KT1_CHECK(LayerCheck(torp, true, Layer::Land, 0, 10) == Refusal::BelowWaterOnly);
	KT1_CHECK(LayerCheck(ground, false, Layer::Sub, 0, 10) == Refusal::TargetBelowWater);
	KT1_CHECK(LayerCheck(torp, false, Layer::Land, 0, 10) == Refusal::TargetAboveWater);
	KT1_CHECK(LayerCheck(torp, false, Layer::Sub, 0, 10) == Refusal::None);
	KT1_CHECK(LayerCheck(ground, false, Layer::Water, 0, 10) == Refusal::None);
	KT1_CHECK(LayerCheck(ground, false, Layer::Unknown, -30, 10) == Refusal::TargetBelowWater);   // the height rule
	LayerReach aa; aa.air = true; aa.noGround = true;
	KT1_CHECK(LayerCheck(aa, false, Layer::Land, 0, 10) == Refusal::AirToAirOnly);
	KT1_CHECK_EQ(std::string(RefusalName(Refusal::CannotTarget)), std::string("cannot_target"));
}

KT1_CASE("TGT-ACCEPT", "allied, arriving, no contact, wrong layer, behind a ridge") {
	const TargetingDefs d = Defs();
	const TargetingParams p = Params();
	const TargetingContext c{d, p};
	FakeWorld w = World();
	KT1_CHECK(Acceptable(c, 1, 1, 10, false, w).ok);
	w.u[10].los = 0;
	TargetTest t = Acceptable(c, 1, 1, 10, false, w);
	KT1_CHECK(!t.ok && t.failed);                     // CMB-027 R2: a check failed: skipped for a while
	w.u[10].los = 4;                                  // seen before (a remembered structure) is a contact
	KT1_CHECK(Acceptable(c, 1, 1, 10, false, w).ok);
	w.u[10].arriving = true;
	t = Acceptable(c, 1, 1, 10, false, w);
	KT1_CHECK(!t.ok && !t.failed);
	w.u[10].arriving = false;
	w.ground = 50;                                    // CMB-016 R2: a ridge above the path
	KT1_CHECK(!Acceptable(c, 1, 1, 10, false, w).ok);
	KT1_CHECK(Acceptable(c, 1, 1, 10, true, w).ok);   // noLine
	KT1_CHECK(!Acceptable(c, 1, 1, 12, false, w).ok); // a ground weapon on a plane
}

KT1_CASE("TGT-ENGAGE", "Engage: best rank, then longest reach; nothing on hold fire; only one weapon") {
	const TargetingDefs d = Defs();
	const TargetingParams p = Params();
	const TargetingContext c{d, p};
	FakeWorld w = World();
	Engagement e = Engage(c, 1, 12, true, 0, w);
	KT1_CHECK(e.found);
	KT1_CHECK_EQ(e.rank, 1);
	KT1_CHECK_EQ(e.reach, 400 - 0.75f * 8);
	KT1_CHECK(!Engage(c, 1, 12, true, 1, w).found);   // weapon 1 cannot take a plane
	const Engagement2 e2 = Engage2(c, 1, 12, true, w);
	KT1_CHECK(!e2.primary.found && e2.best.found);
	w.u[1].fire = 0;
	KT1_CHECK(!Engage(c, 1, 10, true, 0, w).found);
	int32_t kind = 0, rank = 0;
	KT1_CHECK(BestKey(c, 1, 4, kind, rank) && kind == 0 && rank == 1);
	KT1_CHECK(BestKey(c, 1, 2, kind, rank) && kind == 0 && rank == 2);   // def kinds only: no layer test
	KT1_CHECK(!BestKey(c, 4, 1, kind, rank));
}

KT1_CASE("TGT-ORDER-REFUSAL", "CMB-018 R4: an attack order no weapon can take is refused, the reason left on the unit") {
	TargetingDefs d = Defs();
	d.units[5].weapons[0] = 1;   // a tank with weapon 1 alone
	d.units.resize(6);
	d.units[5].weapons[0] = 1;
	const TargetingParams p = Params();
	const TargetingContext c{d, p};
	FakeWorld w = World();
	w.u[1].def = 5;
	TargetState s;
	s.Init(64);
	TargetUnitCreated(s, c, 1, 5, 0);
	OrderView o;
	o.unit = 1; o.unitDef = 5; o.team = 0; o.command = 20; o.nparams = 1; o.params[0] = 12;
	KT1_CHECK(!AllowOrder(s, c, o, 7, w));
	KT1_CHECK(w.lastRefusal == Refusal::NoAirTarget);
	o.params[0] = 10;
	KT1_CHECK(AllowOrder(s, c, o, 7, w));
	// an inserted attack carries its own order from the fourth parameter
	o.command = 1; o.nparams = 4; o.params = {0, 20, 0, 12, 0, 0};
	KT1_CHECK(!AllowOrder(s, c, o, 8, w));
	o.nparams = 3; o.params = {0, 20, 0, 0, 0, 0};    // no fourth: not one parameter, allowed
	KT1_CHECK(AllowOrder(s, c, o, 8, w));
	KT1_CHECK(OrderRefusal(c, 3, 12, true, w) == Refusal::NoAirTarget);   // manual fire: its own reach
	KT1_CHECK(OrderRefusal(c, 3, 12, false, w) == Refusal::None);         // a weapon outside the system decides itself
}

KT1_CASE("TGT-ALLOW-WEAPON", "AllowWeaponTarget: the engine step's choice, a player's attack order, the command gun's") {
	const TargetingDefs d = Defs();
	const TargetingParams p = Params();
	const TargetingContext c{d, p};
	FakeWorld w = World();
	TargetState s;
	s.Init(64);
	TargetUnitCreated(s, c, 1, 1, 0);
	KT1_CHECK(!AllowTarget(s, c, 1, 10, 1, 1, 0, w));
	w.chosen[1 * 100 + 1] = 10;
	KT1_CHECK(AllowTarget(s, c, 1, 10, 1, 1, 0, w));
	KT1_CHECK(!AllowTarget(s, c, 1, 11, 1, 1, 0, w));
	w.u[1].cmd = CommandView{20, 0, 1, 11};
	KT1_CHECK(AllowTarget(s, c, 1, 11, 1, 1, 0, w));
	w.u[1].cmd.options = 8;                           // the engine's own internal attack is no override
	KT1_CHECK(!AllowTarget(s, c, 1, 11, 1, 1, 0, w));
	KT1_CHECK(AllowTarget(s, c, 1, 11, 3, 0, 0, w));  // a weapon of no system: allowed
	KT1_CHECK(AllowTarget(s, c, 1, -1, 1, 1, 0, w));
}

KT1_CASE("TGT-INFLIGHT", "CMB-024: shots in flight count their share; off at factor 0") {
	const TargetingDefs d = Defs();
	TargetingConfig cfg;
	cfg.overkill_inflight_factor = 0.5f;
	const TargetingParams p = MakeTargetingParams(cfg, CommandIds{}, 30);
	const TargetingContext c{d, p};
	TargetState s;
	s.Init(64);
	TargetUnitCreated(s, c, 10, 1, 0);
	float total = 0;
	int32_t t = NONE;
	KT1_CHECK(AddShot(s, c, 500, 1, 10, 40, total));
	KT1_CHECK_EQ(total, 20.0f);
	KT1_CHECK(AddShot(s, c, 501, 1, 10, 40, total));
	KT1_CHECK_EQ(InFlight(s, 10), 40.0f);
	KT1_CHECK(!AddShot(s, c, 502, 0, 10, 40, total));   // not a scanned weapon
	KT1_CHECK(RemoveShot(s, c, 500, t, total));
	KT1_CHECK(t == 10 && total == 20.0f);
	KT1_CHECK(!RemoveShot(s, c, 500, t, total));
	const TargetingParams off = Params();
	const TargetingContext c0{d, off};
	KT1_CHECK(!AddShot(s, c0, 503, 1, 10, 40, total));
}

KT1_CASE("TGT-COMMAND-GUN", "unit_cmd_gun: scan on its delay, first class then nearest, ties keep the engine's order") {
	const TargetingDefs d = Defs();
	const TargetingParams p = Params();
	const TargetingContext c{d, p};
	FakeWorld w = World();
	w.u[1].def = 3;
	w.u[1].range = {{1, 200}};
	TargetState s;
	s.Init(64);
	TargetUnitCreated(s, c, 1, 3, 0);
	w.order = {11, 10, 12};
	RunCommandGuns(s, c, 1, w);                      // before its first scan
	KT1_CHECK_EQ(s.gunner[s.units.index.SlotOf(1)].target, NONE);
	RunCommandGuns(s, c, p.gunScanSteps, w);
	KT1_CHECK_EQ(s.gunner[s.units.index.SlotOf(1)].target, 10);   // mobile before defence; the plane is above 0 but rank 1 too
	KT1_CHECK_EQ(w.u[1].weaponTarget, 10);
	w.u[1].fire = 0;
	RunCommandGuns(s, c, p.gunScanSteps + 1, w);
	KT1_CHECK_EQ(s.gunner[s.units.index.SlotOf(1)].target, NONE);
	KT1_CHECK(AllowTarget(s, c, 1, 10, 1, 3, 0, w) == false);
}
