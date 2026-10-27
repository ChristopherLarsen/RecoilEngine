/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComGroundMoveType.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "MoveDefHandler.h"
#include "MoveMath/MoveMath.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/SpringHash.h"
#include "System/SpringMath.h"
#include "System/Threading/ThreadPool.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CSupComGroundMoveType, CGroundMoveType, (nullptr))
CR_REG_METADATA(CSupComGroundMoveType, (
	CR_MEMBER(baseTurnRate),
	CR_MEMBER(turnAccelFactor),
	CR_MEMBER(turnRadius),
	CR_MEMBER(settleRate),
	CR_MEMBER(stopSpeed),
	CR_MEMBER(blockedRestart),
	CR_MEMBER(uprightSink),
	CR_MEMBER(footprintX),
	CR_MEMBER(footprintZ),
	CR_MEMBER(sinkLower),
	CR_MEMBER(hoverHeight),
	CR_MEMBER(waterSpeedFactor),
	CR_MEMBER(floorClearance),

	CR_MEMBER(scSpeed),
	CR_MEMBER(scPhase),
	CR_MEMBER(scDir),
	CR_MEMBER(scBraking),
	CR_MEMBER(scStopNow),
	CR_MEMBER(scRestart),
	CR_MEMBER(scBlockedFrames),
	CR_MEMBER(scYieldFrames),
	CR_MEMBER(scYieldUnit),
	CR_MEMBER(scYielding),
	CR_MEMBER(contactLift),

	CR_MEMBER(directGoal),
	CR_MEMBER(directCheckFrame),
	CR_MEMBER(directClear),

	CR_MEMBER(submarine),
	CR_MEMBER(subOrdered),
	CR_MEMBER(diveDepth),
	CR_MEMBER(diveFrames),
	CR_MEMBER(depthFrom),
	CR_MEMBER(depthTo),
	CR_MEMBER(depthFrames),
	CR_MEMBER(depthStartFrame),

	CR_PREALLOC(GetPreallocContainer)
))

static constexpr float NO_STOP = std::numeric_limits<float>::infinity();

// direct steering: how far ahead a straight line to the goal is tested, and how often
static constexpr float DIRECT_MAX_DIST = 128.0f * SQUARE_SIZE;
static constexpr int DIRECT_CHECK_FRAMES = 8;

static float CustomParam(const UnitDef* ud, const char* key, float def)
{
	const auto it = ud->customParams.find(key);

	if (it == ud->customParams.end() || it->second.empty())
		return def;

	return std::strtof(it->second.c_str(), nullptr);
}


// frames a wanted step may stay refused, with the footprint partly on impassable squares, before the engine's
// own ground movement takes over
static constexpr int BLOCKED_HANDOVER_FRAMES = GAME_SPEED;
// frames an idle allied unit must stay in contact in front of a mover before it is asked to make way (PTH-002 R3:
// the yield bound is PLACEHOLDER 30 ticks, and the idle unit still has to clear the mover's way)
static constexpr int YIELD_DELAY_FRAMES = 4;
// a spot made way to: up to this many footprints ahead along the mover's direction and to either side
static constexpr int YIELD_STEPS_AHEAD = 6;
static constexpr int YIELD_STEPS_SIDE = 4;
// elmos of float rounding a footprint edge may lie across a square boundary, across the heading
static constexpr float FOOTPRINT_SIDE_EPS = 0.01f;

bool CSupComGroundMoveType::IsWantedBy(const UnitDef* ud)
{
	const auto it = ud->customParams.find("supcom_move");
	return (it != ud->customParams.end() && it->second == "1");
}

CSupComGroundMoveType::CSupComGroundMoveType(CUnit* owner): CGroundMoveType(owner)
{
	// creg
	if (owner == nullptr)
		return;

	const UnitDef* ud = owner->unitDef;
	const MoveDef* md = owner->moveDef;

	// the base class has set turnRate, turnAccel, accRate and decRate from the unit definition
	baseTurnRate = turnRate;
	turnAccelFactor = turnAccel / turnRate;

	// per second -> per frame; a missing settle means constant braking to rest
	settleRate = std::max(1e-6f, CustomParam(ud, "supcom_settle_rate", 1e6f)) / GAME_SPEED;
	stopSpeed = std::max(0.0f, CustomParam(ud, "supcom_stop_speed", 0.0f)) / GAME_SPEED;
	blockedRestart = std::max(0.0f, CustomParam(ud, "supcom_blocked_restart", 0.0f));
	uprightSink = std::max(0.0f, CustomParam(ud, "supcom_upright_sink", 0.0f));
	turnRadius = std::max(0.0f, CustomParam(ud, "supcom_turn_radius", 0.0f));
	footprintX = std::max(0.0f, CustomParam(ud, "supcom_footprint_x", md->xsize * SQUARE_SIZE));
	footprintZ = std::max(0.0f, CustomParam(ud, "supcom_footprint_z", md->zsize * SQUARE_SIZE));
	sinkLower = (CustomParam(ud, "supcom_sink_lower", 0.0f) != 0.0f);
	hoverHeight = std::max(0.0f, CustomParam(ud, "supcom_hover_height", 0.0f));
	waterSpeedFactor = std::max(0.01f, CustomParam(ud, "supcom_water_speed_factor", 1.0f));
	floorClearance = std::max(0.0f, CustomParam(ud, "supcom_floor_clearance", 0.0f));

	// submarine: depth in elmos, dive time in seconds
	submarine = (CustomParam(ud, "supcom_submarine", 0.0f) != 0.0f);
	diveDepth = std::max(0.0f, CustomParam(ud, "supcom_dive_depth", 0.0f));
	diveFrames = std::max(1.0f, CustomParam(ud, "supcom_dive_time", 0.0f) * GAME_SPEED);
	subOrdered = submarine && (CustomParam(ud, "supcom_start_submerged", 0.0f) != 0.0f);
	depthFrom = depthTo = (subOrdered? diveDepth: 0.0f);
	depthStartFrame = gs->frameNum;
}


void CSupComGroundMoveType::ResetForNewOrder()
{
	scBraking = false;
	scStopNow = false;
	scBlockedFrames = 0;

	// a new order during braking or the settle replaces it
	if (scSpeed > 0.0f)
		scPhase = PHASE_DRIVE;
}

void CSupComGroundMoveType::StartMovingRaw(const float3 moveGoalPos, float moveGoalRadius)
{
	CGroundMoveType::StartMovingRaw(moveGoalPos, moveGoalRadius);
	ResetForNewOrder();
}

void CSupComGroundMoveType::StartMoving(float3 moveGoalPos, float moveGoalRadius)
{
	CGroundMoveType::StartMoving(moveGoalPos, moveGoalRadius);
	ResetForNewOrder();
}

void CSupComGroundMoveType::StopMoving(bool callScript, bool hardStop, bool cancelRaw)
{
	CGroundMoveType::StopMoving(callScript, hardStop, cancelRaw);

	// a stop order brakes at once
	scStopNow = true;

	if (hardStop) {
		scSpeed = 0.0f;
		scPhase = PHASE_REST;
	}
}

bool CSupComGroundMoveType::ArrivalAllowed() const
{
	// the order is complete once the unit is at rest; queued waypoints are passed through
	return (scSpeed <= 0.0f || owner->commandAI->commandQue.size() > 1);
}


float CSupComGroundMoveType::StoppingDistance(float v) const
{
	const float settleSpeed = std::max(decRate / settleRate, stopSpeed);
	const auto settleDist = [&](float u) { return (u > stopSpeed)? ((u - stopSpeed) / settleRate): 0.0f; };

	if (v <= settleSpeed)
		return settleDist(v);

	return ((v * v - settleSpeed * settleSpeed) / (2.0f * decRate) + settleDist(settleSpeed));
}

void CSupComGroundMoveType::AdvanceDrive(float dt, float topSpeed, float& v, float& dist) const
{
	float t = 0.0f;

	if (v < topSpeed) {
		// accelerate up to the top speed
		t = std::min(dt, (topSpeed - v) / accRate);
		dist += (v + 0.5f * accRate * t) * t;
		v = (t < dt)? topSpeed: (v + accRate * t);
	} else if (v > topSpeed) {
		// top speed was lowered: brake down to it
		t = std::min(dt, (v - topSpeed) / decRate);
		dist += (v - 0.5f * decRate * t) * t;
		v = (t < dt)? topSpeed: (v - decRate * t);
	}

	dist += v * (dt - t);
}

float CSupComGroundMoveType::AdvanceBraking(float dt, float& dist)
{
	const float settleSpeed = std::max(decRate / settleRate, stopSpeed);
	float t = 0.0f;

	while (t < dt && scPhase != PHASE_REST) {
		const float rem = dt - t;

		if (scPhase != PHASE_SETTLE) {
			// constant braking down to the settle speed
			if (scSpeed <= settleSpeed) {
				scPhase = (scSpeed > stopSpeed)? PHASE_SETTLE: PHASE_REST;
				continue;
			}

			const float tb = (scSpeed - settleSpeed) / decRate;
			const float tau = std::min(rem, tb);

			dist += (scSpeed - 0.5f * decRate * tau) * tau;
			scSpeed = (tau < tb)? (scSpeed - decRate * tau): settleSpeed;
			t += tau;
			continue;
		}

		// exponential settle until the speed falls under stop_speed
		if (scSpeed <= stopSpeed) {
			scPhase = PHASE_REST;
			continue;
		}

		const float ts = math::log(scSpeed / stopSpeed) / settleRate;
		const float tau = std::min(rem, ts);
		const float dec = math::exp(-settleRate * tau);

		dist += scSpeed * (1.0f - dec) / settleRate;
		scSpeed = (tau < ts)? (scSpeed * dec): stopSpeed;
		t += tau;
	}

	if (scPhase == PHASE_REST)
		scSpeed = 0.0f;

	return t;
}

float CSupComGroundMoveType::Advance(float topSpeed, float stopDist, bool stop)
{
	float dist = 0.0f;

	if (stop || scBraking) {
		if (scSpeed <= 0.0f) {
			scSpeed = 0.0f;
			scPhase = PHASE_REST;
			return 0.0f;
		}

		if (scPhase == PHASE_DRIVE || scPhase == PHASE_REST)
			scPhase = PHASE_BRAKE;

		AdvanceBraking(1.0f, dist);
		return dist;
	}

	if (scPhase == PHASE_REST)
		scPhase = PHASE_DRIVE;

	// braking starts at the moment the remaining distance equals the stopping distance
	const auto remainder = [&](float tau) {
		float v = scSpeed;
		float d = 0.0f;
		AdvanceDrive(tau, topSpeed, v, d);
		return (stopDist - d - StoppingDistance(v));
	};

	float tb = 1.0f;

	if (stopDist == NO_STOP || remainder(1.0f) > 0.0f) {
		AdvanceDrive(1.0f, topSpeed, scSpeed, dist);
		return dist;
	}

	if (remainder(0.0f) <= 0.0f) {
		tb = 0.0f;
	} else {
		float lo = 0.0f;
		float hi = 1.0f;

		for (int i = 0; i < 24; i++) {
			const float mid = 0.5f * (lo + hi);

			if (remainder(mid) > 0.0f) {
				lo = mid;
			} else {
				hi = mid;
			}
		}

		tb = hi;
	}

	AdvanceDrive(tb, topSpeed, scSpeed, dist);

	scBraking = true;
	scPhase = PHASE_BRAKE;

	if (scSpeed <= 0.0f) {
		scSpeed = 0.0f;
		scPhase = PHASE_REST;
		return dist;
	}

	AdvanceBraking(1.0f - tb, dist);
	return dist;
}


bool CSupComGroundMoveType::FootprintBlocked(const float3& pos) const
{
	const MoveDef* md = owner->moveDef;

	// axis-aligned footprint, turned with the unit's facing
	const bool alongX = (math::fabs(flatFrontDir.x) > math::fabs(flatFrontDir.z));
	// across the heading an edge within FOOTPRINT_SIDE_EPS of a square boundary does not enter the next square, so
	// a unit can line up with a gap exactly its own width despite float rounding (PTH-001 R2); along the heading
	// the footprint is exact (MOV-007, MOV-008)
	const float hx = (alongX? footprintZ: footprintX) * 0.5f - (alongX? 0.0f: FOOTPRINT_SIDE_EPS);
	const float hz = (alongX? footprintX: footprintZ) * 0.5f - (alongX? FOOTPRINT_SIDE_EPS: 0.0f);

	const float x0 = pos.x - hx;
	const float x1 = pos.x + hx;
	const float z0 = pos.z - hz;
	const float z1 = pos.z + hz;

	if (x0 < 0.0f || z0 < 0.0f || x1 > mapDims.mapx * SQUARE_SIZE || z1 > mapDims.mapy * SQUARE_SIZE)
		return true;

	const int sx0 = std::clamp(int(x0 / SQUARE_SIZE), 0, mapDims.mapxm1);
	const int sz0 = std::clamp(int(z0 / SQUARE_SIZE), 0, mapDims.mapym1);
	// the last square the footprint overlaps; an edge exactly on a square boundary does not enter the next square
	const int sx1 = std::clamp(std::max(sx0, int(math::ceil(x1 / SQUARE_SIZE)) - 1), 0, mapDims.mapxm1);
	const int sz1 = std::clamp(std::max(sz0, int(math::ceil(z1 / SQUARE_SIZE)) - 1), 0, mapDims.mapym1);

	for (int sz = sz0; sz <= sz1; sz++) {
		for (int sx = sx0; sx <= sx1; sx++) {
			if (CMoveMath::GetPosSpeedMod(*md, sx, sz) <= 0.0f)
				return true;
		}
	}

	return false;
}

bool CSupComGroundMoveType::DirectLineClear()
{
	// re-checked a few times per second, and at once for a new goal
	if (goalPos.equals(directGoal) && gs->frameNum < directCheckFrame)
		return directClear;

	directGoal = goalPos;
	directCheckFrame = gs->frameNum + DIRECT_CHECK_FRAMES;
	directClear = false;

	const float3 delta = (goalPos - owner->pos) * XZVector;
	const float len = delta.Length2D();

	if (len > DIRECT_MAX_DIST)
		return false;

	const MoveDef* md = owner->moveDef;
	const int thread = ThreadPool::GetThreadNum();
	const int steps = std::max(1, int(math::ceil(len / (SQUARE_SIZE * 0.5f))));

	for (int i = 1; i <= steps; i++) {
		const float3 p = owner->pos + delta * (float(i) / steps);

		if (FootprintBlocked(p))
			return false;
		if (CMoveMath::IsBlockedStructure(*md, int(p.x / SQUARE_SIZE), int(p.z / SQUARE_SIZE), owner, thread))
			return false;
	}

	return (directClear = true);
}

void CSupComGroundMoveType::SetWaypointDir(const float3& cwp, const float3& opos)
{
	// a clear straight line to the goal is driven directly, not through the path's waypoints
	if (progressState == Active && owner->commandAI->commandQue.size() <= 1 && DirectLineClear()) {
		CGroundMoveType::SetWaypointDir(goalPos, opos);
		return;
	}

	// the engine's waypoints can lie where this move type's centred footprint does not fit (beside a wall: the engine
	// places an even-sized footprint off its centre), or round a corner the footprint cannot cut: repair them locally
	float3 target = FitWaypoint(cwp);

	// at a repaired waypoint the engine does not see its own as reached (it skips only within 7 elmos): ask it to
	// move on to the next one
	if (!target.same(cwp) && target.SqDistance2D(opos) < Square(SQUARE_SIZE * 0.5f))
		TriggerSkipWayPoint();

	CGroundMoveType::SetWaypointDir(target, opos);
}

// offsets of up to three squares around a point, nearest first. Worker threads call this (FollowPath ->
// SetWaypointDir -> FitWaypoint), so the list is built once by the thread-safe initialisation of a local static;
// filling it lazily raced (heap corruption, and a list that could differ between runs: Q107)
static const std::vector<float3>& NearOffsets()
{
	static const std::vector<float3> offsets = [] {
		std::vector<float3> v;

		for (int dz = -3; dz <= 3; dz++) {
			for (int dx = -3; dx <= 3; dx++) {
				if (dx != 0 || dz != 0)
					v.emplace_back(dx * SQUARE_SIZE * 0.5f, 0.0f, dz * SQUARE_SIZE * 0.5f);
			}
		}
		std::stable_sort(v.begin(), v.end(), [](const float3& a, const float3& b) { return (a.SqLength2D() < b.SqLength2D()); });
		return v;
	}();

	return offsets;
}

bool CSupComGroundMoveType::LineFits(const float3& a, const float3& b) const
{
	const float3 d = (b - a) * XZVector;
	const int steps = std::max(1, int(math::ceil(d.Length2D() / (SQUARE_SIZE * 0.25f))));

	for (int k = 1; k <= steps; k++) {
		if (FootprintBlocked(a + d * (float(k) / steps)))
			return false;
	}

	return true;
}

float3 CSupComGroundMoveType::FitWaypoint(const float3& wp) const
{
	// the nearest point to the waypoint (within a square and a half) where the footprint fits
	float3 fit = wp;

	if (FootprintBlocked(wp)) {
		for (const float3& o: NearOffsets()) {
			if (!FootprintBlocked(wp + o)) {
				fit = wp + o;
				break;
			}
		}
	}

	// not reachable in a straight line from here (round a corner): first a point near the unit from which it is
	const float3& pos = owner->pos;

	if (scBlockedFrames == 0 || LineFits(pos, fit))
		return fit;

	// first the two axis-aligned corners (line up with a gap, or round a wall end), then points round the unit
	const float3 corners[2] = {{pos.x, pos.y, fit.z}, {fit.x, pos.y, pos.z}};

	for (const float3& p: corners) {
		// a corner on the unit itself would leave it steering nowhere
		if (p.SqDistance2D(pos) < 1.0f)
			continue;
		if (LineFits(pos, p) && LineFits(p, fit))
			return p;
	}

	for (const float3& o: NearOffsets()) {
		const float3 p = pos + o * 2.0f;

		if (LineFits(pos, p) && LineFits(p, fit))
			return p;
	}

	return fit;
}

float CSupComGroundMoveType::GridStep(float p, float d)
{
	// round a step to the float spacing at its start and end, so that the new position is exact and steps of
	// equal length stay equal once applied
	const float a = math::fabs(p);
	const float b = math::fabs(p + d);
	const float u = std::max(std::nextafter(a, std::numeric_limits<float>::infinity()) - a,
	                         std::nextafter(b, std::numeric_limits<float>::infinity()) - b);

	return (math::floor(d / u + 0.5f) * u);
}

void CSupComGroundMoveType::UpdateTurnRate()
{
	// a turn radius raises the turn rate limit with speed: v / r radians per frame
	float rate = baseTurnRate;

	if (turnRadius > 0.0f)
		rate = std::max(rate, (scSpeed / turnRadius) * (SPRING_CIRCLE_DIVS / math::TWOPI));

	turnRate = std::clamp(rate, 1.0f, SPRING_CIRCLE_DIVS * 0.5f - 1.0f);
	turnAccel = turnRate * turnAccelFactor;
}


void CSupComGroundMoveType::UpdateUnitPosition()
{
	RECOIL_DETAILED_TRACY_ZONE;

	if (owner->UnderFirstPersonControl()) {
		CGroundMoveType::UpdateUnitPosition();

		scSpeed = math::fabs(owner->speed.dot(flatFrontDir));
		scPhase = PHASE_DRIVE;
		return;
	}

	// pushed by a crowd partly onto impassable squares and blocked there for a second: the engine's own ground
	// movement (it side-steps and is pushed out by the terrain collision response) until the footprint is clear;
	// this move type's exact steps cannot leave such a spot, and the engine's idle check then failed the order. A
	// unit standing at an obstacle without overlapping it (MOV-007) keeps this move type.
	if (scBlockedFrames >= BLOCKED_HANDOVER_FRAMES) {
		if (FootprintBlocked(owner->pos) && progressState == Active) {
			CGroundMoveType::UpdateUnitPosition();

			scSpeed = math::fabs(owner->speed.dot(flatFrontDir));
			scPhase = PHASE_DRIVE;
			scBraking = false;
			scRestart = false;
			return;
		}

		scBlockedFrames = 0;
	}

	resultantForces = ZeroVector;

	if (owner->IsSkidding())
		return;

	bool stop = scStopNow;
	bool wantReverse = false;

	switch (setHeading) {
		case HEADING_CHANGED_MOVE: {
			wantReverse = WantReverse(waypointDir, flatFrontDir);
		} break;
		case HEADING_CHANGED_STOP:
		case HEADING_CHANGED_STUN: {
			stop = true;
		} break;
		default: {
			stop |= (progressState != Active);
		} break;
	}

	setHeading = HEADING_CHANGED_NONE;

	if (owner->GetTransporter() != nullptr)
		return;
	if (owner->beingBuilt)
		return;

	const int wantDir = wantReverse? -1: 1;

	float topSpeed = wantReverse? maxReverseSpeed: maxSpeed;

	// under water, the move class's depth speed rule (depthModParams), the same one the engine's own ground movement
	// applies through its terrain speed: one rule for both move types (the game's default is no slowdown)
	// on the sea floor (amphibious) or on water (hover), the unit's water speed factor (MOV-009 R7); ships and
	// submarines keep their speed (MOV-011 R1)
	{
		const MoveDef* md = owner->moveDef;
		const float terrain = CGround::GetHeightReal(owner->pos.x, owner->pos.z);

		if (!md->FloatOnWater())
			topSpeed *= md->GetDepthMod(terrain);
		if (terrain < 0.0f && !submarine && md->speedModClass != MoveDef::Ship)
			topSpeed *= waterSpeedFactor;
	}

	if (maxWantedSpeed > 0.0f)
		topSpeed = std::min(topSpeed, maxWantedSpeed);

	wantedSpeed = topSpeed * (1 - stop);

	// distance left to the point where the unit should be at rest: the goal itself
	// for move orders, the goal radius for orders that only need to come within reach
	float stopDist = NO_STOP;

	if (!stop && owner->commandAI->commandQue.size() <= 1) {
		const float stopRadius = ((goalRadius > SQUARE_SIZE)? goalRadius: 0.0f) + extraRadius;

		stopDist = goalPos.distance2D(owner->pos) - stopRadius;

		// at rest well short of the stopping point (pushed away, or the path bent): go again
		if (scBraking && scSpeed <= 0.0f && stopDist > std::max(goalRadius, float(SQUARE_SIZE)))
			scBraking = false;
	}

	float dist = 0.0f;

	if (scRestart && !stop && !scBraking) {
		// the step after a blocked one is taken at the restart speed
		scDir = wantDir;
		scPhase = PHASE_DRIVE;
		scSpeed = std::min(topSpeed, blockedRestart * maxSpeed);
		dist = scSpeed;
	} else if (scSpeed > 0.0f && wantDir != scDir) {
		// change of direction: brake to rest first
		dist = Advance(topSpeed, NO_STOP, true);
	} else {
		if (scSpeed <= 0.0f)
			scDir = wantDir;

		dist = Advance(topSpeed, stopDist, stop);
	}

	scRestart = false;

	float3 moveRequest = flatFrontDir * (dist * scDir);

	moveRequest.x = GridStep(owner->pos.x, moveRequest.x);
	moveRequest.z = GridStep(owner->pos.z, moveRequest.z);

	if (!moveRequest.same(ZeroVector)) {
		bool blocked = false;

		if (!positionStuck) {
			blocked = (FootprintBlocked(owner->pos + moveRequest) && !FootprintBlocked(owner->pos));

			if (!blocked) {
				float3 resultantMove;
				UpdatePos(owner, moveRequest, resultantMove, ThreadPool::GetThreadNum());
				blocked = !resultantMove.same(moveRequest);
			}
		}

		if (blocked) {
			// the step is not taken and the speed is lost
			moveRequest = ZeroVector;
			scSpeed = 0.0f;
			scPhase = PHASE_DRIVE;
			scRestart = (!stop && !scBraking);
			scBlockedFrames += int(!stop);

			ReRequestPath(false);
		} else {
			positionStuck = false;
			scBlockedFrames = 0;
		}
	}

	oldSpeed = owner->speed.dot(flatFrontDir);
	newSpeed = moveRequest.dot(flatFrontDir);

	// reported velocity is the step's displacement
	owner->SetVelocityAndSpeed(moveRequest);
	resultantForces = moveRequest;

	UpdateTurnRate();
}

float CSupComGroundMoveType::CurrentDepth() const
{
	if (depthFrames <= 0.0f)
		return depthTo;

	// smoothstep: monotone, slow at both ends, fastest in the middle, no overshoot
	const float u = std::clamp((gs->frameNum - depthStartFrame) / depthFrames, 0.0f, 1.0f);
	return (depthFrom + (depthTo - depthFrom) * (u * u * (3.0f - 2.0f * u)));
}

bool CSupComGroundMoveType::IsDiving() const
{
	return (depthFrames > 0.0f && (gs->frameNum - depthStartFrame) < depthFrames);
}

void CSupComGroundMoveType::StartDive(bool submerge)
{
	const float from = CurrentDepth();
	const float to = submerge? diveDepth: 0.0f;

	subOrdered = submerge;
	depthFrom = from;
	depthTo = to;
	depthStartFrame = gs->frameNum;

	// a reversal part-way takes the matching part of the full time
	depthFrames = (diveDepth > 0.0f)? (diveFrames * math::fabs(to - from) / diveDepth): 0.0f;
}

bool CSupComGroundMoveType::SetMemberValue(unsigned int memberHash, void* memberValue)
{
	static const unsigned int SUBMERGED_HASH = spring::LiteHash("submerged", sizeof("submerged") - 1, 0);

	if (memberHash == SUBMERGED_HASH) {
		if (!submarine)
			return false;

		const bool submerge = *(reinterpret_cast<bool*>(memberValue));

		if (submerge != subOrdered)
			StartDive(submerge);

		return true;
	}

	return CGroundMoveType::SetMemberValue(memberHash, memberValue);
}

const char* CSupComGroundMoveType::GetLayerName() const
{
	// a dive or surfacing keeps the layer it started in until it ends (Sub only at full depth)
	if (submarine) {
		if (IsDiving())
			return ((depthFrom >= diveDepth)? "sub": "water");

		return ((subOrdered && depthTo >= diveDepth)? "sub": "water");
	}

	const MoveDef* md = owner->moveDef;

	if (md->speedModClass == MoveDef::Ship)
		return "water";

	// terrain under the unit's position against the water surface (height 0)
	const bool underWater = (CGround::GetHeightReal(owner->pos.x, owner->pos.z) < 0.0f);

	if (md->speedModClass == MoveDef::Hover)
		return (underWater? "water": "land");

	return (underWater? "seabed": "land");
}

void CSupComGroundMoveType::AdjustPosToWaterLine()
{
	RECOIL_DETAILED_TRACY_ZONE;

	if (owner->IsFalling())
		return;
	if (owner->IsFlying())
		return;

	if (submarine) {
		// the water surface is at height 0; in shallow water the dive holds floorClearance above the floor (MOV-011 R7)
		const float floor = CGround::GetHeightReal(owner->pos.x, owner->pos.z);
		const float height = std::max(floor + floorClearance, -CurrentDepth());
		owner->Move(UpVector * (height - owner->pos.y), true);
		return;
	}

	const bool hover = (owner->moveDef->speedModClass == MoveDef::Hover && hoverHeight > 0.0f);

	if ((owner->FloatOnWater() && !hover) || footprintX <= 0.0f || footprintZ <= 0.0f) {
		CGroundMoveType::AdjustPosToWaterLine();
		return;
	}

	const float3 fDir = GetVectorFromHeading(owner->heading);
	const float3 rDir = float3{-fDir.z, 0.0f, fDir.x};
	const float3 fOff = fDir * (footprintZ * 0.5f);
	const float3 rOff = rDir * (footprintX * 0.5f);
	const float3& pos = owner->pos;

	// footprint corners: front right, front left, rear right, rear left
	float3 corners[4] = {pos + fOff + rOff, pos + fOff - rOff, pos - fOff + rOff, pos - fOff - rOff};

	float hMean = 0.0f;
	float hMin = CGround::GetHeightReal(pos.x, pos.z);
	const float hMinCentre = hMin;
	float hMax = hMin;

	for (float3& c: corners) {
		// hover units: the higher of the terrain and the water surface (height 0) under each corner
		c.y = CGround::GetHeightReal(c.x, c.z);
		c.y = hover? std::max(c.y, 0.0f): c.y;
		hMean += c.y * 0.25f;
		hMin = std::min(hMin, c.y);
		hMax = std::max(hMax, c.y);
	}

	float height = hMean;

	if (hover) {
		height += hoverHeight;
	} else if (owner->upright || sinkLower) {
		height -= uprightSink * (hMax - hMin);
	}

	owner->Move(UpVector * (height - pos.y), true);

	// the hover height and the footprint's height above the surface under the centre are contact, not flight: an
	// object the engine thinks is in the air gets no terrain collision response and is not avoided by other units
	const float centreSurface = hover? std::max(hMinCentre, 0.0f): hMinCentre;
	contactLift = std::max(0.0f, owner->pos.y - centreSurface);

	if (owner->upright)
		return;

	// tilt: the normal of the plane spanned by the two diagonals
	float3 normal = (corners[0] - corners[3]).cross(corners[1] - corners[2]);

	if (normal.y < 0.0f)
		normal = -normal;

	if (normal.SqLength() <= 0.0f)
		return;

	owner->UpdateDirVectors(normal.Normalize());
	owner->UpdateMidAndAimPos();
}


bool CSupComGroundMoveType::Update()
{
	const bool moved = CGroundMoveType::Update();

	// arrived (or stopped) after making way: an ordinary idle unit again
	if (scYielding && progressState != Active)
		scYielding = false;

	CheckYield();
	return moved;
}

bool CSupComGroundMoveType::CanYield() const
{
	if (progressState == Active || scYielding)
		return false;
	if (owner->beingBuilt || owner->GetTransporter() != nullptr || owner->IsStunned())
		return false;
	// a unit holding position is moved only by an explicit swap (PTH-002 R3, case G)
	if (owner->moveState == MOVESTATE_HOLDPOS)
		return false;

	return owner->commandAI->commandQue.empty();
}

void CSupComGroundMoveType::CheckYield()
{
	// runs in the serial move type update, so it may start another unit moving
	if (progressState != Active || scYielding || owner->beingBuilt || owner->GetTransporter() != nullptr) {
		scYieldFrames = 0;
		scYieldUnit = -1;
		return;
	}

	const float3& pos = owner->pos;
	const float3 front = (scDir < 0)? -flatFrontDir: flatFrontDir;
	const float searchRadius = HalfSize() * 2.0f + SQUARE_SIZE * 4.0f;

	QuadFieldQuery qfQuery;
	quadField.GetUnitsExact(qfQuery, pos, searchRadius, false);

	// the lowest id among the idle allied units in contact in front (deterministic)
	CSupComGroundMoveType* blocker = nullptr;

	for (CUnit* u: *qfQuery.units) {
		if (u == owner || !teamHandler.AlliedTeams(owner->team, u->team))
			continue;

		CSupComGroundMoveType* mt = dynamic_cast<CSupComGroundMoveType*>(u->moveType);

		if (mt == nullptr || !mt->CanYield())
			continue;

		const float3 d = (u->pos - pos) * XZVector;
		const float dist = d.Length2D();
		// in contact: footprints or collision spheres within a square and a half (a blocked mover stops a few elmos short)
		const float contact = std::max(HalfSize() + mt->HalfSize(), owner->radius + u->radius) + SQUARE_SIZE * 1.5f;
		// in front: within 60 degrees of the heading or of the way to the current waypoint
		const float3 toWp = (currWayPoint - pos) * XZVector;
		const bool inFront = (dist <= 0.01f) || (d.dot(front) >= 0.5f * dist) ||
			(toWp.SqLength2D() > 1.0f && d.dot(toWp) >= 0.5f * dist * toWp.Length2D());

		if (dist > contact || !inFront)
			continue;
		if (blocker == nullptr || u->id < blocker->owner->id)
			blocker = mt;
	}

	if (blocker == nullptr) {
		scYieldFrames = 0;
		scYieldUnit = -1;
		return;
	}

	if (blocker->owner->id != scYieldUnit) {
		scYieldUnit = blocker->owner->id;
		scYieldFrames = 0;
	}

	if (++scYieldFrames < YIELD_DELAY_FRAMES)
		return;

	scYieldFrames = 0;
	blocker->MakeWay(this);
}

// distance from p to the segment a-b, in the horizontal plane
static float SegmentDist2D(const float3& p, const float3& a, const float3& b)
{
	const float3 ab = (b - a) * XZVector;
	const float len2 = ab.SqLength2D();
	const float t = (len2 > 0.0f)? std::clamp(((p - a) * XZVector).dot(ab) / len2, 0.0f, 1.0f): 0.0f;

	return (a + ab * t).distance2D(p);
}

bool CSupComGroundMoveType::MakeWay(const CSupComGroundMoveType* mover)
{
	const float3& pos = owner->pos;
	const float3& mpos = mover->owner->pos;

	// along the mover's way: from the mover to this unit, else the mover's heading
	float3 ahead = (pos - mpos) * XZVector;
	ahead = (ahead.SqLength2D() > 0.01f)? ahead.SafeNormalize2D(): (mover->flatFrontDir * XZVector).SafeNormalize2D();
	const float3 side = float3(-ahead.z, 0.0f, ahead.x);

	// the mover's route near here: to its current and next waypoints, then its goal
	const float3 route[4] = {mpos, mover->currWayPoint, mover->nextWayPoint, mover->goalPos};
	const float clearance = HalfSize() + mover->HalfSize() + SQUARE_SIZE * 0.5f;
	const float step = std::max(HalfSize() * 2.0f, float(SQUARE_SIZE));

	const auto OffRoute = [&](const float3& p) {
		for (int i = 0; i < 3; i++) {
			if (SegmentDist2D(p, route[i], route[i + 1]) < clearance)
				return false;
		}
		return true;
	};
	const auto Free = [&](const float3& p) {
		if (FootprintBlocked(p))
			return false;

		QuadFieldQuery qfQuery;
		quadField.GetUnitsExact(qfQuery, p, HalfSize() + SQUARE_SIZE * 0.5f, false);

		for (const CUnit* u: *qfQuery.units) {
			if (u != owner)
				return false;
		}
		return true;
	};

	// nearest first: a footprints ahead (through a gap), then b to the side; reached in two straight legs
	for (int n = 1; n <= YIELD_STEPS_AHEAD + YIELD_STEPS_SIDE; n++) {
		for (int a = 0; a <= std::min(n, YIELD_STEPS_AHEAD); a++) {
			const int b = n - a;

			if (b > YIELD_STEPS_SIDE)
				continue;

			const float3 mid = pos + ahead * (a * step);

			for (int k = 0; k < ((b == 0)? 1: 2); k++) {
				const float3 spot = mid + side * (((k == 0)? 1: -1) * b * step);

				if (!OffRoute(spot) || !Free(spot))
					continue;
				if (!LineFits(pos, mid) || !LineFits(mid, spot))
					continue;

				StartMoving(spot, SQUARE_SIZE * 0.5f);
				scYielding = true;
				return true;
			}
		}
	}

	return false;
}
