/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// Engine-free tests of Core/FxEvents/FxWreck (LUA-FREE S30): each rule against LuaRules/Gadgets/fx_wreck_physics.lua, at
// 30 and 10 steps per second where a per step value is involved.

#include "../KT1Test.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/FxEvents/FxState.h"
#include "Game/KT1/Core/FxEvents/FxWreck.h"

#include <array>
#include <cmath>
#include <vector>

using namespace KT1::Core;
using namespace KT1::Core::Fx;

#define CHECK(c, msg) KT1_CHECK_MSG(c, msg)

namespace {
constexpr int FEATURES = 64;

bool Near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

// ground: height = base + riseX * x, one normal everywhere
struct World {
	std::array<Vec3, FEATURES> pos{};
	std::array<bool, FEATURES> there{};
	std::array<float, FEATURES> mass{};
	float base = 0.0f, riseX = 0.0f;
	Vec3 normal = {0.0f, 1.0f, 0.0f};
	int draws = 0;

	void Put(int f, float x, float y, float z) {
		pos[f] = {x, y, z};
		there[f] = true;
	}
	void Apply(const FxOutput& out) {
		for (const WreckWrite& w: out.wreckWrites)
			pos[w.feature] = w.pos;
	}
	static float Height(void* c, float x, float) { auto* w = static_cast<World*>(c); return w->base + w->riseX * x; }
	static void Normal(void* c, float, float, Vec3& n) { n = static_cast<World*>(c)->normal; }
	static bool Feature(void* c, int32_t f, Vec3& out) {
		auto* w = static_cast<World*>(c);
		if (!w->there[f])
			return false;
		out = w->pos[f];
		return true;
	}
	static float Mass(void* c, int32_t f) { return static_cast<World*>(c)->mass[f]; }
	static float Random(void* c) { ++static_cast<World*>(c)->draws; return 0.5f; }
	static int32_t Range(void* c, int32_t lo, int32_t) { ++static_cast<World*>(c)->draws; return lo; }

	WorldQuery Query() {
		WorldQuery q;
		q.ctx = this;
		q.groundHeight = &Height;
		q.groundNormal = &Normal;
		q.featurePos = &Feature;
		q.featureMass = &Mass;
		q.random = &Random;
		q.randomRange = &Range;
		q.mapSizeX = 4096.0f;
		q.mapSizeZ = 4096.0f;
		q.gravity = 120.0f / static_cast<float>(Rate::StepsPerSecond() * Rate::StepsPerSecond());
		return q;
	}
};

struct Fixture {
	WreckConfig c;
	WreckParams p;
	WreckState s;
	World w;
	WorldQuery q;
	FxOutput out;

	Fixture() {
		p = MakeWreckParams(c);
		InitWrecks(s, FEATURES);
		q = w.Query();
	}
	void Place(int f, float x, float z, float y = 0.0f) {
		w.Put(f, x, y, z);
		PlaceWreck(s, c, f, q);
	}
	void Step(int32_t step) {
		out.Clear();
		StepWrecks(s, c, p, step, q, out);
		w.Apply(out);
	}
	void Explode(const WreckPushWeapon& weapon, const Vec3& at, std::vector<WreckNear> near, int32_t step) {
		ExplodeNearWrecks(s, c, p, weapon, 7, at, near, step, q, out);
	}
	std::vector<WreckNear> Near3(std::initializer_list<int> features) const {
		std::vector<WreckNear> near;
		for (const int f: features)
			near.push_back({f, w.pos[f]});
		return near;
	}
	std::vector<int32_t> Pushed() const {
		std::vector<int32_t> ids;
		for (const SimHost::DisplayEvent& e: out.events) {
			WreckImpulse i;
			if (e.Payload(i))
				ids.push_back(i.feature);
		}
		return ids;
	}
};
}

KT1_RATE_CASE("FXWRECK-PARAMS", "per step values: the Lua's at 30 steps per second, scaled at 10") {
	const WreckConfig c;
	const WreckParams p = MakeWreckParams(c);
	if (testCase.StepsPerSecond() == 30) {
		CHECK(p.drag == 0.96f && p.waterDrag == 0.80f && p.groundEase == 0.8f, "one period: the shares themselves");
		CHECK(p.stopSpeed == 0.05f && p.pushSpeed == 2.5f && p.upSpeed == 3.0f && p.spin == 0.25f, "the Lua's per frame speeds");
		CHECK(p.bounceMin == 0.8f && p.thudMinDrop == 1.5f, "bounce and thud speeds");
		CHECK(p.thudGap == 12 && p.thudWreckGap == 45, "the Lua's frame gaps");
	} else {
		CHECK(p.drag == 0.96f * 0.96f * 0.96f && p.waterDrag == 0.80f * 0.80f * 0.80f, "three periods a step");
		CHECK(p.groundEase == 0.8f * 0.8f * 0.8f, "ease over three periods");
		CHECK(Near(p.stopSpeed, 0.15f) && p.pushSpeed == 7.5f && p.upSpeed == 9.0f && p.spin == 0.75f, "per step speeds");
		CHECK(p.thudGap == 4 && p.thudWreckGap == 15, "gaps in steps");
	}
}

KT1_CASE("FXWRECK-SHARE", "a step that is not a whole number of drag periods takes the fractional power") {
	Rate::SetStepsPerSecond(20);
	const WreckParams p = MakeWreckParams(WreckConfig{});
	CHECK(Near(p.drag, 0.9406040f), "0.96 to the 1.5");
	CHECK(Near(p.waterDrag, 0.7155418f), "0.8 to the 1.5");
	Rate::Clear();
}

KT1_RATE_CASE("FXWRECK-WEAPON", "explosions that move wrecks: damage and half the area, both thresholds included") {
	const WreckConfig c;
	CHECK(!PushWeaponOf(c, 149.0f, 100.0f).moves && !PushWeaponOf(c, 150.0f, 47.0f).moves, "under a threshold");
	const WreckPushWeapon low = PushWeaponOf(c, 150.0f, 48.0f);
	CHECK(low.moves && !low.tumbles && low.radius == 24.0f && low.damage == 150.0f, "at both thresholds");
	CHECK(PushWeaponOf(c, 400.0f, 200.0f).tumbles, "tumbles from its damage");
	WreckConfig off;
	off.enabled = false;
	CHECK(!PushWeaponOf(off, 1000.0f, 200.0f).moves, "disabled");
}

KT1_RATE_CASE("FXWRECK-PLACE", "a wreck placed on a steep slope starts to move; on flat ground it rests") {
	Fixture f;
	f.Place(3, 100.0f, 100.0f);
	CHECK(!WreckMoving(f.s, 3), "flat: rests");
	f.w.normal = {0.6f, 0.8f, 0.0f};
	f.Place(9, 100.0f, 100.0f);
	f.Place(2, 100.0f, 100.0f);
	CHECK(WreckMoving(f.s, 9) && WreckMoving(f.s, 2), "steep: slides");
	CHECK(f.s.moving.IdOf(0) == 9 && f.s.moving.IdOf(1) == 2, "started order");
	CHECK(f.s.seq[f.s.placed.SlotOf(3)] == 1 && f.s.seq[f.s.placed.SlotOf(2)] == 3, "placing numbers");
	PlaceWreck(f.s, f.c, FEATURES + 1, f.q);
	CHECK(f.s.placed.Size() == 3 && f.w.draws == 0, "an id out of range is ignored; no random draw");
}

KT1_RATE_CASE("FXWRECK-ORDER", "an explosion pushes nearest first, then lowest id, with the Lua's strengths and events") {
	Fixture f;
	f.Place(4, 520.0f, 500.0f);
	f.Place(11, 500.0f, 480.0f);
	f.Place(8, 510.0f, 500.0f);
	f.Place(5, 500.0f, 500.0f);
	f.Place(6, 500.0f, 500.0f);
	f.Place(7, 600.0f, 500.0f);      // outside the radius
	f.w.mass[8] = 100.0f;
	f.w.Put(20, 500.0f, 0.0f, 500.0f);   // never placed
	const WreckPushWeapon weapon{500.0f, 40.0f, true, true};
	f.Explode(weapon, {500.0f, 0.0f, 500.0f}, f.Near3({7, 4, 20, 11, 8, 6, 5}), 10);
	CHECK(f.Pushed() == (std::vector<int32_t>{5, 6, 8, 4, 11}), "nearest first, ties by id; no share, no push");
	CHECK(f.out.events.size() == 5 && f.w.draws == 0, "one event a push, no random draw");
	WreckImpulse a, b, h;
	f.out.events[0].Payload(a);
	f.out.events[1].Payload(b);
	f.out.events[2].Payload(h);
	CHECK(a.vx == -(f.p.pushSpeed * 0.5f) && b.vx == f.p.pushSpeed * 0.5f && a.vz == 0.0f, "at the centre: along x by parity");
	CHECK(a.up == f.p.upSpeed * 1.0f * 0.5f && a.spin == f.p.spin * 1.0f && a.weaponDef == 7, "tumble at the centre");
	CHECK(h.vx == f.p.pushSpeed * 0.5f * 0.75f / (1.0f + 100.0f / 100.0f), "share and mass");
	const Vec3 w = f.s.spin[f.s.moving.SlotOf(5)];   // placed fourth
	CHECK(w.x == a.spin * 1.25f && w.y == a.spin * 2.0f && w.z == a.spin * 1.0f, "spin shares from the placing number");
	const Vec3 i = f.s.impulse[f.s.moving.SlotOf(5)];
	CHECK(i.x == a.vx && i.y == a.up && i.z == a.vz, "wreck_impulse holds the push");
	f.Step(10);
	CHECK(f.s.impulse[f.s.moving.SlotOf(5)].x == a.vx, "kept after the step it was given in");
	f.Step(11);
	CHECK(f.s.impulse[f.s.moving.SlotOf(5)].x == 0.0f, "cleared in a step without an explosion");
}

KT1_RATE_CASE("FXWRECK-LIMITS", "wrecks an explosion moves, explosions a step and wrecks in motion are limited") {
	Fixture f;
	const WreckPushWeapon weapon{500.0f, 400.0f, true, false};
	const Vec3 at{500.0f, 0.0f, 500.0f};
	f.Explode(weapon, at, {}, 1);
	CHECK(f.s.budgetStep == -1, "no wreck: no explosion counted");
	for (int i = 0; i < 20; ++i)
		f.Place(i, 510.0f + i, 500.0f);
	f.c.per_explosion = 3;
	f.c.explosions_a_step = 2;
	f.Explode(weapon, at, f.Near3({5, 1, 3, 2, 4}), 2);
	CHECK(f.Pushed() == (std::vector<int32_t>{1, 2, 3}), "three nearest");
	f.out.Clear();
	f.Explode(weapon, at, {}, 2);               // counted although nothing is near
	f.Explode(weapon, at, f.Near3({6}), 2);
	CHECK(f.out.events.empty(), "the step's explosions are spent");
	f.Explode(weapon, at, f.Near3({6}), 3);
	CHECK(f.Pushed() == (std::vector<int32_t>{6}), "again next step");
	f.c.max_moving = 4;
	f.out.Clear();
	f.Explode(weapon, at, f.Near3({7, 1}), 4);
	CHECK(f.Pushed() == (std::vector<int32_t>{1}) && !WreckMoving(f.s, 7), "full: only one already in motion");
	CHECK(!PushWreck(f.s, f.c, 9, 1.0f, 0.0f, 0.0f, 0.0f), "push refused while full");
}

KT1_RATE_CASE("FXWRECK-DROP", "a dropped wreck's place goes to the last; release unless posed; gone ones are not released") {
	Fixture f;
	for (int i = 1; i <= 5; ++i)
		f.Place(i, 100.0f * i, 100.0f);
	SetWreckPose(f.s, 4, {0.1f, 0.2f, 0.3f});
	for (int i = 1; i <= 5; ++i)
		PushWreck(f.s, f.c, i, (i == 3 || i == 5) ? f.p.pushSpeed : 0.0f, 0.0f, 0.0f, 0.0f);
	f.w.there[2] = false;
	f.Step(1);
	// 1 rests (released); 5 takes its place and moves; 2 is gone (4 takes its place, rests, posed); 3 moves
	CHECK(f.out.releaseWrecks == (std::vector<int32_t>{1}), "released: only the resting, unposed one");
	CHECK(f.out.wreckWrites.size() == 4, "one write for each wreck still there");
	CHECK(f.out.wreckWrites[0].feature == 1 && f.out.wreckWrites[1].feature == 5 && f.out.wreckWrites[2].feature == 4
		&& f.out.wreckWrites[3].feature == 3, "the Lua's list order");
	CHECK(f.s.moving.Size() == 2 && f.s.moving.IdOf(0) == 5 && f.s.moving.IdOf(1) == 3, "left in motion");
	f.out.Clear();
	SetWreckPose(f.s, 5, {0.0f, 1.0f, 0.0f});
	ForgetWreck(f.s, 5, f.out);
	ForgetWreck(f.s, 1, f.out);
	CHECK(f.out.releaseWrecks == (std::vector<int32_t>{5}), "destroyed in motion: released; at rest: nothing");
	CHECK(!WreckMoving(f.s, 5) && !f.s.placed.Has(5) && f.s.moving.IdOf(0) == 3, "forgotten");
	SetWreckPose(f.s, 30, {0.0f, 1.0f, 0.0f});
	ClearWreckPose(f.s, 30);
	CHECK(!f.s.placed.Has(30), "a pose alone leaves nothing behind");
}

KT1_RATE_CASE("FXWRECK-POSE", "a posed wreck's rotation is the pose plus the tilt, written once more when it comes flat") {
	Fixture f;
	SetWreckPose(f.s, 2, {0.5f, 0.25f, 0.125f});   // before it is placed, as eco_wrecks does
	f.Place(2, 100.0f, 100.0f);
	PushWreck(f.s, f.c, 2, 0.0f, 0.0f, 0.0f, 0.0f);
	f.s.rot[0] = {0.011f, 0.0f, 0.0f};
	f.Step(1);
	CHECK(f.out.wreckWrites.size() == 1 && !f.out.wreckWrites[0].setRotation, "flat now and never marked tilted: no rotation");
	CHECK(f.out.releaseWrecks.empty() && !WreckMoving(f.s, 2), "at rest; posed: kept from the engine");
	PushWreck(f.s, f.c, 2, 0.0f, 0.0f, 0.0f, 0.0f);
	f.s.rot[0] = {0.5f, 0.0f, 0.0f};
	f.Step(2);
	CHECK(f.out.wreckWrites[0].setRotation && f.out.wreckWrites[0].rot.x == 0.5f + 0.5f * f.p.groundEase
		&& f.out.wreckWrites[0].rot.y == 0.25f, "pose plus eased tilt");
	int32_t step = 3;
	for (; step < 100 && f.s.rot[0].x != 0.0f; ++step)
		f.Step(step);
	CHECK(f.out.wreckWrites[0].setRotation && f.out.wreckWrites[0].rot.x == 0.5f && !WreckMoving(f.s, 2), "last write is the pose");
}

KT1_RATE_CASE("FXWRECK-GROUND", "slide, drag, water drag and the map edge, one step as the Lua's") {
	Fixture f;
	f.w.normal = {0.6f, 0.8f, 0.0f};
	f.Place(1, 100.0f, 100.0f);
	f.Step(1);
	const float a = f.q.gravity * (0.6f - 0.2f);
	const float vx = (0.0f + 1.0f * a) * f.p.drag;
	CHECK(f.s.vel[0].x == vx && f.out.wreckWrites[0].pos.x == 100.0f + vx, "downhill acceleration then drag");
	CHECK(!f.out.wreckWrites[0].setRotation && f.s.moving.Size() == 1, "flat wreck: no rotation write");
	f.w.base = -50.0f;
	f.w.Put(1, 100.0f, -50.0f, 100.0f);
	f.Step(2);
	CHECK(f.s.vel[0].x == (vx + 1.0f * a) * f.p.waterDrag, "under water");
	f.w.base = 0.0f;
	f.w.riseX = 0.01f;
	f.w.Put(1, 4090.0f, 40.9f, 2.0f);
	f.Step(3);
	const WreckWrite& e = f.out.wreckWrites[0];
	CHECK(e.pos.x == 4096.0f - 8.0f && e.pos.z == 8.0f && f.s.vel[0].x == 0.0f, "kept inside the edge");
	CHECK(e.pos.y > 40.89f, "the height is that of the place before the clamp");
}

KT1_RATE_CASE("FXWRECK-AIR", "a thrown wreck falls, bounces and lands; thuds are rate limited") {
	Fixture f;
	const int sps = testCase.StepsPerSecond();
	f.Place(1, 100.0f, 100.0f);
	f.Place(2, 300.0f, 100.0f);
	for (const int id: {1, 2})
		PushWreck(f.s, f.c, id, 0.0f, 0.0f, f.p.upSpeed, f.p.spin);
	std::vector<int32_t> steps, ids;
	int32_t step = 1;
	for (; step < 40 * sps && f.s.moving.Size() > 0; ++step) {
		f.Step(step);
		for (const SimHost::DisplayEvent& e: f.out.events) {
			WreckThud t;
			if (e.Payload(t)) {
				steps.push_back(step);
				ids.push_back(t.feature);
				CHECK(t.speed >= f.p.thudMinDrop && t.pos.y == 0.0f, "a fast enough landing, at the ground");
			}
		}
	}
	CHECK(f.s.moving.Size() == 0 && step > sps, "both come to rest after the flight");
	CHECK(!steps.empty() && ids[0] == 1, "the first in the list is heard first");
	for (size_t i = 1; i < steps.size(); ++i)
		CHECK(steps[i] - steps[i - 1] >= f.p.thudGap, "gap between any two thuds");
	CHECK(f.w.pos[1].y == 0.0f && f.w.draws == 0, "on the ground; no random draw");
}

KT1_RATE_CASE("FXWRECK-THUD", "the gap between two thuds of one wreck is the longer one") {
	Fixture f;
	f.Place(1, 100.0f, 100.0f);
	const auto land = [&](int32_t step) {
		f.w.Put(1, 100.0f, 0.0f, 100.0f);
		PushWreck(f.s, f.c, 1, 0.0f, 0.0f, 0.001f, 0.0f);
		f.s.vel[0].y = -2.0f * f.p.thudMinDrop;
		f.Step(step);
		return f.out.events.size();
	};
	CHECK(land(0) == 1, "the first, at step 0");
	CHECK(land(f.p.thudWreckGap - 1) == 0, "too soon for this wreck");
	CHECK(land(f.p.thudWreckGap) == 1, "heard again");
}
