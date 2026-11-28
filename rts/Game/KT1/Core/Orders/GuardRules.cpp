/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the patrol, guard, assist and reach decisions behind GuardRules.h, as ord_orders.lua has them
// (float, in the Lua's order of evaluation). No engine include.

#include "GuardRules.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "OrdersConfig.h"
#include "Game/KT1/Core/Common/DetMath.h"
#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Orders {

namespace {

constexpr float PI = 3.14159265358979323846f;   // DESIGN: the Lua's math.pi

float Square(float x) { return x * x; }

float Gap2(float ax, float az, float bx, float bz) { return Square(ax - bx) + Square(az - bz); }

} // namespace

void Nearest::OfferByLowestId(UnitId unit, float d) {
	if (id == NO_UNIT || d < distance || (d == distance && unit < id)) {
		id = unit;
		distance = d;
	}
}

void Nearest::OfferFirst(UnitId unit, float d) {
	if (id == NO_UNIT || d < distance) {
		id = unit;
		distance = d;
	}
}

bool NeedsRepair(float health, float maxHealth, float threshold, float epsilon) {
	return health <= threshold * maxHealth + epsilon && health < maxHealth;
}

GuardParams MakeGuardParams(const OrdersConfig& config) {
	const float sps = static_cast<float>(Rate::StepsPerSecond());
	const float mu = config.elmos_per_mu;
	GuardParams p;
	p.check = Rate::Every(config.guard.check_s);
	p.half = config.guard.half_radius_mu * mu;
	p.leadSteps = config.guard.follow_lead_s * sps;
	p.behind = config.guard.follow_behind_mu * mu;
	p.movingSpeed = config.guard.moving_speed_elmo_s / sps;
	p.ringSettle = config.guard.ring_settle_mu * mu;
	p.ringPush = config.guard.ring_push_mu * mu;
	p.ringOnWay = config.guard.ring_on_way_mu * mu;
	return p;
}

namespace {

// a group guard's unit keeps to its ring place beside the guarded unit (FRM-005 R3)
FollowPlan FollowRing(const FollowFacts& f, const GuardParams& p, bool moving) {
	FollowPlan plan;
	const float px = f.gx + f.ringDx;
	const float pz = f.gz + f.ringDz;
	const float miss2 = Gap2(f.ux, f.uz, px, pz);
	plan.ringSettled = f.ringSettled || miss2 <= Square(p.ringSettle);
	const bool onWay = f.frontIsMove && !moving && Gap2(f.frontX, f.frontZ, px, pz) <= Square(p.ringOnWay);
	if (onWay || !(moving || miss2 > Square(plan.ringSettled ? p.ringPush : p.ringSettle)))
		return plan;
	if (miss2 > Square(p.ringPush))
		plan.ringSettled = false;
	plan.removeOwnFront = true;
	plan.move = true;
	plan.x = px + f.vx * p.leadSteps;
	plan.y = f.gy;
	plan.z = pz + f.vz * p.leadSteps;
	return plan;
}

} // namespace

FollowPlan PlanFollow(const FollowFacts& f, const GuardParams& p) {
	const bool moving = f.vx * f.vx + f.vz * f.vz > Square(p.movingSpeed);
	if (f.hasRing)
		return FollowRing(f, p, moving);
	FollowPlan plan;
	plan.ringSettled = f.ringSettled;
	const float dist = DetMath::Dist2(f.ux - f.gx, f.uz - f.gz);
	const float near = p.half / 2;   // DESIGN: half of half the guard radius
	// at rest and near: a builder helps from where it stands, any other guard has nothing to do (f.repairs changes
	// nothing: the Lua's two tests give the same answer)
	if (!moving && !(dist > near))
		return plan;
	const float px = f.gx + f.vx * p.leadSteps;
	const float pz = f.gz + f.vz * p.leadSteps;
	const float dd = std::max(1.0f, DetMath::Dist2(f.ux - px, f.uz - pz));   // DESIGN: at least one elmo
	const float k = std::min(1.0f, p.behind / dd);
	plan.removeOwnFront = true;
	plan.move = true;
	plan.x = px + (f.ux - px) * k;
	plan.y = f.gy;
	plan.z = pz + (f.uz - pz) * k;
	return plan;
}

AssistWant WantedAssist(const Task& task, UnitId unit, UnitId guarded, const DefFacts& def, int32_t maxUnits) {
	AssistWant want;
	const bool buildOrRepair = task.kind == WorkKind::Build || task.kind == WorkKind::Repair;
	if (buildOrRepair && task.target != unit) {
		want = {false, true, CommandKind::Repair, task.target};
	} else if (task.kind == WorkKind::Reclaim && def.Has(DefFlag::Reclaims)) {
		want = {false, true, CommandKind::Reclaim, task.isFeature ? task.target + maxUnits : task.target};
	} else if (task.kind == WorkKind::Factory && task.target != guarded) {
		want = {false, true, CommandKind::Guard, task.target};   // the lead's factory, further down a chain
	} else if (task.kind == WorkKind::Factory) {
		want.leaveAlone = true;
	}
	return want;
}

bool WantsShield(float health, float maxHealth, bool beingBuilt, bool shieldHasRoom) {
	return health >= maxHealth && !beingBuilt && shieldHasRoom;
}

ReachParams MakeReachParams(const OrdersConfig& config) {
	const ReachConfig& c = config.reach;
	ReachParams p;
	p.stepsPerSecond = static_cast<float>(Rate::StepsPerSecond());
	p.every = Rate::Every(c.every_s);
	p.walkingEvery = Rate::Every(c.walking_every_s);
	p.stall = c.stall_s * p.stepsPerSecond;
	p.walkSlack = c.walk_slack_s * p.stepsPerSecond;
	p.walkTimeFactor = c.walk_time_factor;
	p.progress = c.progress_elmo;
	p.inside = c.inside_elmo;
	p.minOut = c.min_out_elmo;
	p.clear = c.clear_elmo;
	p.tried = c.tried_elmo;
	p.out = c.out_elmo;
	p.arcStep = c.arc_step_rad;
	p.doneInside = c.done_inside_elmo;
	p.featureRadius = c.feature_radius_elmo;
	p.unitRadius = c.unit_radius_elmo;
	p.spots = c.spots;
	p.tries = std::min(c.tries, static_cast<int>(REACH_TRIED));
	return p;
}

bool ReachCheckDue(UnitId unit, Step now, bool walking, const ReachParams& p) {
	return (now + unit) % (walking ? p.walkingEvery : p.every) == 0;
}

bool NoteReach(ReachStall& stall, CommandKind kind, float target, float dist, float reach, Step now,
               const ReachParams& p) {
	if (!stall.active || stall.kind != kind || stall.target != target) {
		stall = ReachStall{};
		stall.active = true;
		stall.kind = kind;
		stall.target = target;
		stall.best = dist;
		stall.since = now;
	}
	if (dist <= reach || dist < stall.best - p.progress) {
		stall.best = std::min(stall.best, dist);
		stall.since = now;
		return false;
	}
	return !(static_cast<float>(now - stall.since) < p.stall) && stall.tries < p.tries;
}

float SpotRadius(float targetRadius, float reach, const ReachParams& p) {
	return std::max(targetRadius + p.minOut, reach - p.inside);
}

void SpotPoint(int32_t i, float tx, float tz, float r, const ReachParams& p, float& sx, float& sz) {
	assert(p.trig.cos != nullptr && p.trig.sin != nullptr);
	const float half = static_cast<float>(p.spots) / 2;   // DESIGN: spots per half turn
	const float a = static_cast<float>(i) * PI / half;
	sx = tx + r * p.trig.cos(a);
	sz = tz + r * p.trig.sin(a);
}

bool SpotTried(const ReachStall& stall, float sx, float sz, const ReachParams& p) {
	for (int32_t i = 0; i < stall.tried; ++i) {
		if (Gap2(stall.spots[i][0], stall.spots[i][1], sx, sz) < Square(p.tried))
			return true;
	}
	return false;
}

void NoteSpotSearch(ReachStall& stall, float dist, Step now) {
	++stall.tries;
	stall.since = now;
	stall.best = dist;
}

void NoteSpot(ReachStall& stall, float sx, float sz) {
	if (stall.tried < static_cast<int32_t>(REACH_TRIED))
		stall.spots[stall.tried++] = {sx, sz};
}

Walk PlanWalk(float ux, float uz, float tx, float tz, float spotX, float spotZ, float dist, float reach,
              const ReachParams& p) {
	assert(p.trig.cos != nullptr && p.trig.sin != nullptr && p.trig.atan2 != nullptr);
	const float ring = std::max(dist, reach) + p.out;
	const float a0 = p.trig.atan2(uz - tz, ux - tx);
	const float a1 = p.trig.atan2(spotZ - tz, spotX - tx);
	const float turn = 2 * PI;   // DESIGN: a full turn
	const float da = DetMath::Mod(a1 - a0 + PI, turn) - PI;
	// the arc has n + 1 points and the spot one more, so n leaves room for two
	const int32_t most = static_cast<int32_t>(WALK_ORDERS) - 1 - 1;
	const int32_t n = std::clamp(static_cast<int32_t>(std::ceil(std::fabs(da) / p.arcStep)), 1, most);
	Walk walk;
	for (int32_t k = 0; k <= n; ++k) {
		const float a = a0 + da * static_cast<float>(k) / static_cast<float>(n);
		walk.points[walk.count++] = {tx + ring * p.trig.cos(a), tz + ring * p.trig.sin(a)};
	}
	walk.points[walk.count++] = {spotX, spotZ};
	float px = ux;
	float pz = uz;
	for (int32_t k = 0; k < walk.count; ++k) {
		walk.length = walk.length + DetMath::Dist2(walk.points[k][0] - px, walk.points[k][1] - pz);
		px = walk.points[k][0];
		pz = walk.points[k][1];
	}
	return walk;
}

void BeginWalk(ReachStall& stall, float length, float speed, Step now, const ReachParams& p) {
	const float slackS = p.walkSlack / p.stepsPerSecond;
	stall.walked = true;
	stall.walkFrom = now;
	stall.walkFor = (length / std::max(speed, 1.0f) * p.walkTimeFactor + slackS) * p.stepsPerSecond;
}

bool WalkArrived(float ux, float uz, float tx, float tz, float reach, const ReachParams& p) {
	return Gap2(ux, uz, tx, tz) <= Square(reach - p.doneInside);
}

bool WalkExpired(const ReachStall& stall, Step now) {
	// the Lua drops a walk with no stall record, or with no start time, at once
	return !stall.active || !stall.walked || static_cast<float>(now - stall.walkFrom) >= stall.walkFor;
}

void EndWalk(ReachStall& stall, Step now) {
	if (!stall.active)
		return;
	stall.since = now;
	stall.best = std::numeric_limits<float>::infinity();
}

} // namespace KT1::Core::Orders
