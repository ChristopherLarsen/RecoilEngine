/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComGroundMoveType.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "MoveDefHandler.h"
#include "MoveMath/MoveMath.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
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

	CR_MEMBER(scSpeed),
	CR_MEMBER(scPhase),
	CR_MEMBER(scDir),
	CR_MEMBER(scBraking),
	CR_MEMBER(scStopNow),
	CR_MEMBER(scRestart),

	CR_MEMBER(directGoal),
	CR_MEMBER(directCheckFrame),
	CR_MEMBER(directClear),

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
}


void CSupComGroundMoveType::ResetForNewOrder()
{
	scBraking = false;
	scStopNow = false;

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
	const float hx = (alongX? footprintZ: footprintX) * 0.5f;
	const float hz = (alongX? footprintX: footprintZ) * 0.5f;

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

	CGroundMoveType::SetWaypointDir(cwp, opos);
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

			ReRequestPath(false);
		} else {
			positionStuck = false;
		}
	}

	oldSpeed = owner->speed.dot(flatFrontDir);
	newSpeed = moveRequest.dot(flatFrontDir);

	// reported velocity is the step's displacement
	owner->SetVelocityAndSpeed(moveRequest);
	resultantForces = moveRequest;

	UpdateTurnRate();
}

void CSupComGroundMoveType::AdjustPosToWaterLine()
{
	RECOIL_DETAILED_TRACY_ZONE;

	if (owner->IsFalling())
		return;
	if (owner->IsFlying())
		return;

	if (owner->FloatOnWater() || footprintX <= 0.0f || footprintZ <= 0.0f) {
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
	float hMax = hMin;

	for (float3& c: corners) {
		c.y = CGround::GetHeightReal(c.x, c.z);
		hMean += c.y * 0.25f;
		hMin = std::min(hMin, c.y);
		hMax = std::max(hMax, c.y);
	}

	float height = hMean;

	if (owner->upright || sinkLower)
		height -= uprightSink * (hMax - hMin);

	owner->Move(UpVector * (height - pos.y), true);

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
