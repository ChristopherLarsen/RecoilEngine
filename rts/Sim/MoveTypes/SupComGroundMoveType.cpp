/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComGroundMoveType.h"
#include "Lua/LuaUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>
#include <string>

#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "MoveDefHandler.h"
#include "MoveMath/MoveMath.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/QuadField.h"
#include "Sim/Path/IPathManager.h"
#include "Sim/Misc/TeamHandler.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/SpringHash.h"
#include "System/SpringMath.h"
#include "System/Threading/ThreadPool.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CSupComGroundMoveType, CGroundMoveType, (nullptr))
CR_REG_METADATA(CSupComGroundMoveType, (
	CR_MEMBER(baseTurnRate),
	CR_MEMBER(turnAccelFactor),
	CR_MEMBER(turnRadius),
	CR_MEMBER(inPlaceRate),
	CR_MEMBER(wpDip),
	CR_MEMBER(wpDipFrames),
	CR_MEMBER(wpDipUntil),
	CR_MEMBER(wpThrough),
	CR_MEMBER(wpAdvance),
	CR_MEMBER(settleRate),
	CR_MEMBER(stopSpeed),
	CR_MEMBER(blockedRestart),
	CR_MEMBER(restartDelay),
	CR_MEMBER(blockedSinceMove),
	CR_MEMBER(deadUntil),
	CR_MEMBER(deadGoal),
	CR_MEMBER(reverseDist),
	CR_MEMBER(reverseAngle),
	CR_MEMBER(backTurn),
	CR_MEMBER(turnGoCos),
	CR_MEMBER(uprightSink),
	CR_MEMBER(footprintX),
	CR_MEMBER(footprintZ),
	CR_MEMBER(sinkLower),
	CR_MEMBER(hoverHeight),
	CR_MEMBER(waterSpeedFactor),
	CR_MEMBER(floorClearance),
	CR_MEMBER(seabedSwitchDepth),
	CR_MEMBER(onSeabed),
	CR_MEMBER(headingFree),

	CR_MEMBER(scSpeed),
	CR_MEMBER(scPhase),
	CR_MEMBER(scDir),
	CR_MEMBER(scBraking),
	CR_MEMBER(scStopNow),
	CR_MEMBER(scRestart),
	CR_MEMBER(scBlockedFrames),
	CR_MEMBER(scStructFrames),
	CR_MEMBER(scHandoverFrames),
	CR_MEMBER(scHandoverFrom),
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
	CR_MEMBER(holdBestDist),
	CR_MEMBER(holdBestFrame),
	CR_MEMBER(holdGoal),

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
// frames a wanted step may stay refused by a structure while following a path before the engine's own ground
// movement takes over (Q100; DESIGN), and the longest such handover (it ends once the unit has moved two squares)
static constexpr int STRUCT_HANDOVER_FRAMES = GAME_SPEED / 2;
static constexpr int STRUCT_HANDOVER_TIME = GAME_SPEED * 2;
// frames an idle allied unit must stay in contact in front of a mover before it is asked to make way (PTH-002 R3:
// the yield bound is PLACEHOLDER 30 ticks, and the idle unit still has to clear the mover's way)
static constexpr int YIELD_DELAY_FRAMES = 4;
CONFIG(bool, SupComYieldLog).defaultValue(false).description("supcom: log each make-way step of SupComGroundMoveType units (diagnosis; no effect on the game).");
static bool yieldLogSquares = false;

CONFIG(bool, SupComMakeWay).defaultValue(true).description("supcom: SupComGroundMoveType units line up with a gap their own width instead of stopping at its edge, and make way faster (PTH-002, PTH-004); 0 behaves as before.");
static bool MakeWayOn() { static const bool on = configHandler->GetBool("SupComMakeWay"); return on; }
CONFIG(bool, SupComCornerLog).defaultValue(false).description("supcom: log the corner speed check once a second per unit (diagnosis)");
static bool CornerLog() { static const bool on = configHandler->GetBool("SupComCornerLog"); return on; }
// sideways search for a line that fits through a gap: up to this many elmos either side, in this step (PLACEHOLDER)
static constexpr float LINEUP_MAX = SQUARE_SIZE;
static constexpr float LINEUP_STEP = 0.25f;
// a mover asks an idle unit on its line ahead to make way this many frames of travel before it reaches it, so the
// yield is under way before contact (PTH-002 R3: yield_delay_max 30 ticks; PLACEHOLDER)
static constexpr float YIELD_LOOKAHEAD_FRAMES = 20.0f;

CONFIG(bool, SupComPathSmooth).defaultValue(false).description("supcom: 1 makes SupComGroundMoveType units steer to the furthest waypoint ahead they reach in a straight line over open ground, clear of walls and structures, not the engine's 8-direction grid path (PTH-001 B and I); default 0 (off: it still costs PTH-002-A and PTH-004-D) follows the waypoints as before.");
static bool PathSmoothOn() { static const bool on = configHandler->GetBool("SupComPathSmooth"); return on; }
// path smoothing: waypoints looked ahead, and frames between re-checks of the target (PLACEHOLDER)
static constexpr int SMOOTH_LOOKAHEAD = 32;
static constexpr int SMOOTH_RECHECK_FRAMES = 8;

CONFIG(bool, SupComExactGoal).defaultValue(true).description("supcom: a SupComGroundMoveType unit's move ends at its goal when its own footprint fits there; the engine widens the arrival radius by the footprint radius whenever its wider move class does not fit (PTH-001-F, PTH-003); 0 widens as before.");
static bool ExactGoalOn() { static const bool on = configHandler->GetBool("SupComExactGoal"); return on; }

CONFIG(bool, SupComCrowdArrive).defaultValue(true).description("supcom: a SupComGroundMoveType unit in a crowd keeps driving to its goal for a while before its order counts as arrived outside the goal radius (the engine ends it when it touches a unit near the goal, or widens the radius while it waits) (PTH-004 A and B); 0 arrives as before.");
static bool CrowdArriveOn() { static const bool on = configHandler->GetBool("SupComCrowdArrive"); return on; }
// the longest an order's early arrivals are held back in all (PLACEHOLDER: under PTH-004's stuck.time plus stuck.recover)
static constexpr int CROWD_MAX_FRAMES = GAME_SPEED * 5;

CONFIG(bool, SupComGapQueue).defaultValue(true).description("supcom: at a gap their own width, SupComGroundMoveType units pass one at a time in the order they arrive: each keeps a footprint of room behind the unit queued before it instead of pushing in beside it (PTH-002 R2, case A; PTH-004-A); 0 crowds the gap as before.");
static bool GapQueueOn() { static const bool on = configHandler->GetBool("SupComGapQueue"); return on; }
CONFIG(bool, SupComTurnGo).defaultValue(true).description("supcom: SupComGroundMoveType units with customParams.supcom_reverse_dist reverse to a goal behind them within that distance (else drive forwards), and units with customParams.supcom_turn_go_angle turn on the spot from rest until their heading is within that angle of their way before they move off; 0 (or no keys) keeps the engine's own reverse choice and moves off while turning, as before.");
static bool TurnGoOn() { static const bool on = configHandler->GetBool("SupComTurnGo"); return on; }
// a unit joins the queue this far before the gap along its axis, elmos (PLACEHOLDER: SF-PTH-002 counts arrival 2 cells
// before the wall face, 2.5 cells from the gap's centre)
static constexpr float GAPQ_REACH = SQUARE_SIZE * 5.0f;
// room kept between the footprints of queued units, elmos (PLACEHOLDER: none; the engine's collisions keep footprints
// apart, PTH-002-F measured at most 0.03 of a footprint of overlap)
static constexpr float GAPQ_MARGIN = 0.0f;
// a unit ahead neither held by the queue nor any nearer the gap for this long is passed over, frames (PLACEHOLDER)
static constexpr int GAPQ_STALL_FRAMES = GAME_SPEED;
// room beyond the stopping distance before a unit held by the queue drives again, elmos (PLACEHOLDER)
static constexpr float GAPQ_RESUME = 1.0f;
// the gap's mouth: the point on its line that units off the line steer to first, footprints before its centre (PLACEHOLDER)
static constexpr float GAPQ_MOUTH = 2.0f;
// path smoothing: the room a cut-across line keeps on each side of the footprint, elmos (PLACEHOLDER: one square)
static constexpr float SMOOTH_CLEARANCE = SQUARE_SIZE * 1.0f;

CONFIG(bool, SupComDirectCost).defaultValue(true).description("supcom: a straight line to the goal is driven directly only if no square on it is slower than open ground (PTH-001-D: costly areas are left to the path); 0 checks only impassable squares, as before.");
static bool DirectCostOn() { static const bool on = configHandler->GetBool("SupComDirectCost"); return on; }
static bool YieldLog() { static const bool on = configHandler->GetBool("SupComYieldLog"); return on; }

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
	restartDelay = int(std::max(0.0f, CustomParam(ud, "supcom_blocked_restart_delay", 0.0f)) * GAME_SPEED + 0.5f);
	// SupComTurnGo: reverse distance (elmos; 0 = the engine's own choice) and the move-off angle (degrees; 0 = off)
	reverseDist = std::max(0.0f, CustomParam(ud, "supcom_reverse_dist", 0.0f));
	goalRadiusParam = CustomParam(ud, "supcom_goal_radius", -1.0f);
	// MOV-005 R5 (supcom_reverse_angle with supcom_reverse_dist): a goal behind, within the distance and at least the angle off the
	// heading, is reached backwards, rear first (the engine's own reverse choice given these two thresholds, which also turns the
	// heading); any other goal behind: back up while the front turns towards it, brake once it is within a right angle, drive on
	reverseAngle = std::clamp(CustomParam(ud, "supcom_reverse_angle", 0.0f), 0.0f, 180.0f);

	{
		const float a = CustomParam(ud, "supcom_turn_go_angle", 0.0f);
		turnGoCos = (a > 0.0f && a < 180.0f)? math::cos(a * math::DEG_TO_RAD): 2.0f;
	}
	uprightSink = std::max(0.0f, CustomParam(ud, "supcom_upright_sink", 0.0f));
	turnRadius = std::max(0.0f, CustomParam(ud, "supcom_turn_radius", 0.0f));
	inPlaceRate = std::max(0.0f, CustomParam(ud, "supcom_turn_in_place_rate", 0.0f)) * (SPRING_CIRCLE_DIVS / 360.0f) / GAME_SPEED;
	// MOV-020 R3: moving on to a new point while driving, the speed dips to this fraction of the top speed for this many frames, then
	// the unit accelerates again at its own rate (0 or no keys: no dip)
	wpDip = std::clamp(CustomParam(ud, "supcom_wp_dip", 0.0f), 0.0f, 1.0f);
	wpDipFrames = std::max(0, int(CustomParam(ud, "supcom_wp_dip_frames", 0.0f)));
	footprintX = std::max(0.0f, CustomParam(ud, "supcom_footprint_x", md->xsize * SQUARE_SIZE));
	footprintZ = std::max(0.0f, CustomParam(ud, "supcom_footprint_z", md->zsize * SQUARE_SIZE));
	sinkLower = (CustomParam(ud, "supcom_sink_lower", 0.0f) != 0.0f);
	hoverHeight = std::max(0.0f, CustomParam(ud, "supcom_hover_height", 0.0f));
	waterSpeedFactor = std::max(0.01f, CustomParam(ud, "supcom_water_speed_factor", 1.0f));
	floorClearance = std::max(0.0f, CustomParam(ud, "supcom_floor_clearance", 0.0f));
	seabedSwitchDepth = std::max(0.0f, CustomParam(ud, "supcom_seabed_switch_depth", 0.0f));

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
	smoothPathID = 0;
	smoothFrame = -1;
	smoothPts.clear();
	crowdFrames = 0;
	holdBestDist = -1.0f;
	gqKey = -1;
	gqDoneKey = -1;
	gqFrame = -1;
	gqAhead = false;
	gqHeld = false;

	// a new order during braking or the settle replaces it
	if (scSpeed > 0.0f)
		scPhase = PHASE_DRIVE;

	// MOV-020 R3: on to a new point while driving: the speed dip
	if (wpDip > 0.0f && wpDipFrames > 0 && scSpeed > 0.0f && wpAdvance) {
		wpDipUntil = gs->frameNum + wpDipFrames;
		scSpeed = std::min(scSpeed, wpDip * maxSpeed);
		wpThrough = true;
	} else {
		wpThrough = false;
	}
}

bool CSupComGroundMoveType::FootprintFitsAt(const float3& pos) const
{
	if (FootprintBlocked(pos))
		return false;

	// no structure under the footprint (the facing at the goal is not known: the larger side both ways)
	const float h = std::max(footprintX, footprintZ) * 0.5f - FOOTPRINT_SIDE_EPS;
	const int sx0 = std::clamp(int((pos.x - h) / SQUARE_SIZE), 0, mapDims.mapxm1);
	const int sz0 = std::clamp(int((pos.z - h) / SQUARE_SIZE), 0, mapDims.mapym1);
	const int sx1 = std::clamp(std::max(sx0, int(math::ceil((pos.x + h) / SQUARE_SIZE)) - 1), 0, mapDims.mapxm1);
	const int sz1 = std::clamp(std::max(sz0, int(math::ceil((pos.z + h) / SQUARE_SIZE)) - 1), 0, mapDims.mapym1);

	MoveTypes::CheckCollisionQuery collisionQuery(owner);
	collisionQuery.UpdateElevationForPos(int2{(sx0 + sx1) / 2, (sz0 + sz1) / 2});

	return ((CMoveMath::RangeIsBlocked(sx0, sx1, sz0, sz1, &collisionQuery, 0) & CMoveMath::BLOCK_STRUCTURE) == 0);
}

bool CSupComGroundMoveType::ExactGoalEnabled() { return ExactGoalOn(); }

void CSupComGroundMoveType::ExactGoalRadius()
{
	if (ExactGoalOn() && extraRadius > 0.0f && FootprintFitsAt(goalPos))
		extraRadius = 0.0f;

	// supcom_goal_radius: a plain move order (the command AI's default radius, one square or less) to ground the footprint fits
	// on ends within this radius of the ordered point (no footprint widening); orders with a wider radius (build range, guard) keep theirs
	if (goalRadiusParam >= 0.0f && goalRadius <= SQUARE_SIZE && FootprintFitsAt(goalPos)) {
		goalRadius = goalRadiusParam;
		extraRadius = 0.0f;
		atGoal = (goalPos.SqDistance2D(owner->pos) < Square(goalRadius));
	}
}

void CSupComGroundMoveType::PushLuaState(lua_State* L) const
{
	LuaPushNamedNumber(L, "supcomGoalRadius", goalRadius);
	LuaPushNamedNumber(L, "supcomExtraRadius", extraRadius);
	LuaPushNamedNumber(L, "supcomGoalRadiusParam", goalRadiusParam);
	LuaPushNamedBool(L, "supcomAtGoal", atGoal);
	LuaPushNamedBool(L, "supcomGapAhead", gqAhead);
	LuaPushNamedBool(L, "supcomGapHeld", gqHeld);
	LuaPushNamedNumber(L, "supcomGapRoom", gqRoom);
	LuaPushNamedNumber(L, "supcomCrowdFrames", crowdFrames);
	LuaPushNamedNumber(L, "supcomStructFrames", scStructFrames);
}

void CSupComGroundMoveType::StartMovingRaw(const float3 moveGoalPos, float moveGoalRadius)
{
	// MOV-020 R3: an advance moves on from a point the unit has (nearly) reached; a new order from elsewhere is no advance
	wpAdvance = ((owner->pos - goalPos) * XZVector).Length2D() <= 2.0f * SQUARE_SIZE;
	CGroundMoveType::StartMovingRaw(moveGoalPos, moveGoalRadius);
	ExactGoalRadius();
	ResetForNewOrder();
}

void CSupComGroundMoveType::StartMoving(float3 moveGoalPos, float moveGoalRadius)
{
	// MOV-020 R3: an advance moves on from a point the unit has (nearly) reached; a new order from elsewhere is no advance
	wpAdvance = ((owner->pos - goalPos) * XZVector).Length2D() <= 2.0f * SQUARE_SIZE;
	CGroundMoveType::StartMoving(moveGoalPos, moveGoalRadius);
	ExactGoalRadius();
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

bool CSupComGroundMoveType::HoldLastLeg()
{
	// the path manager has no waypoint after the last one; with the current waypoint at the goal (the last leg, open ground)
	// the unit drives on to the goal and brakes there instead of failing the order 3 to 5 squares short (base-cross L4705:
	// commander 27.6, tank 20.8 elmos short)
	if (earlyCurrWayPoint.y < 0.0f || earlyCurrWayPoint.SqDistance2D(goalPos) > Square(SQUARE_SIZE))
		return false;

	if (holdBestDist < 0.0f || holdGoal != goalPos) {
		holdGoal = goalPos;
		holdBestDist = owner->pos.distance2D(goalPos);
		holdBestFrame = gs->frameNum;
	}

	earlyNextWayPoint = goalPos;
	earlyNextWayPoint.y = earlyCurrWayPoint.y;
	return true;
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


bool CSupComGroundMoveType::FootprintAtMapEdge() const
{
	// the footprint (the larger of the move type's and the unit def's, turned with the facing) within half an elmo of an edge
	const float fx = std::max(footprintX, owner->unitDef->xsize * SQUARE_SIZE * 1.0f);
	const float fz = std::max(footprintZ, owner->unitDef->zsize * SQUARE_SIZE * 1.0f);
	const bool alongX = (math::fabs(flatFrontDir.x) > math::fabs(flatFrontDir.z));
	const float hx = (alongX? fz: fx) * 0.5f;
	const float hz = (alongX? fx: fz) * 0.5f;
	const float3& p = owner->pos;

	return (p.x - hx < 0.5f || p.z - hz < 0.5f || p.x + hx > mapDims.mapx * SQUARE_SIZE - 0.5f || p.z + hz > mapDims.mapy * SQUARE_SIZE - 0.5f);
}

void CSupComGroundMoveType::KeepInsideMap()
{
	// the map edge by the footprint's edge in elmos (MAP-EDGE-UNIT-INSIDE): the engine's collision push tests only the
	// square under the centre, so a pushed unit could rest with its footprint over the edge; the facing as in
	// FootprintBlocked, and an axis narrower than the footprint is left alone
	// the larger of the move type's footprint and the unit def's (MOV-007 R1: the whole unit footprint stays inside;
	// the move type's defaults to the planner footprint, which may be smaller)
	const float fx = std::max(footprintX, owner->unitDef->xsize * SQUARE_SIZE * 1.0f);
	const float fz = std::max(footprintZ, owner->unitDef->zsize * SQUARE_SIZE * 1.0f);
	if (fx <= 0.0f || fz <= 0.0f)
		return;

	const bool alongX = (math::fabs(flatFrontDir.x) > math::fabs(flatFrontDir.z));
	const float hx = (alongX? fz: fx) * 0.5f;
	const float hz = (alongX? fx: fz) * 0.5f;
	const float mx = mapDims.mapx * SQUARE_SIZE * 1.0f;
	const float mz = mapDims.mapy * SQUARE_SIZE * 1.0f;

	float3 d;
	if (hx * 2.0f <= mx)
		d.x = std::clamp(owner->pos.x, hx, mx - hx) - owner->pos.x;
	if (hz * 2.0f <= mz)
		d.z = std::clamp(owner->pos.z, hz, mz - hz) - owner->pos.z;

	if (d.x != 0.0f || d.z != 0.0f)
		owner->Move(d, true);
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
			if (CMoveMath::GetPosSpeedMod(*md, sx, sz) <= 0.0f) {
				if (YieldLog() && yieldLogSquares)
					LOG("[SupComYield]   blocked square (%d, %d) of footprint x %.2f..%.2f z %.2f..%.2f (squares %d..%d, %d..%d), along x %d", sx, sz, x0, x1, z0, z1, sx0, sx1, sz0, sz1, int(alongX));
				return true;
			}
		}
	}

	return false;
}

bool CSupComGroundMoveType::FootprintOnStructure(const float3& pos) const
{
	// the same squares as FootprintBlocked, checked for structures (FootprintBlocked checks the terrain only)
	const bool alongX = (math::fabs(flatFrontDir.x) > math::fabs(flatFrontDir.z));
	const float hx = (alongX? footprintZ: footprintX) * 0.5f - (alongX? 0.0f: FOOTPRINT_SIDE_EPS);
	const float hz = (alongX? footprintX: footprintZ) * 0.5f - (alongX? FOOTPRINT_SIDE_EPS: 0.0f);
	const int sx0 = std::clamp(int((pos.x - hx) / SQUARE_SIZE), 0, mapDims.mapxm1);
	const int sz0 = std::clamp(int((pos.z - hz) / SQUARE_SIZE), 0, mapDims.mapym1);
	const int sx1 = std::clamp(std::max(sx0, int(math::ceil((pos.x + hx) / SQUARE_SIZE)) - 1), 0, mapDims.mapxm1);
	const int sz1 = std::clamp(std::max(sz0, int(math::ceil((pos.z + hz) / SQUARE_SIZE)) - 1), 0, mapDims.mapym1);
	const MoveTypes::CheckCollisionQuery q(owner);
	return (CMoveMath::RangeIsBlocked(sx0, sx1, sz0, sz1, &q, ThreadPool::GetThreadNum()) & CMoveMath::BLOCK_STRUCTURE) != 0;
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

	// a square slower than open ground (a costly area or a slope, PTH-001-D) is left to the path, which goes round
	// it; the start square too (a unit on the edge of a costly area otherwise drove straight back across it)
	const float startMod = CMoveMath::GetPosSpeedMod(*md, owner->pos);

	for (int i = 1; i <= steps; i++) {
		const float3 p = owner->pos + delta * (float(i) / steps);

		if (FootprintBlocked(p))
			return false;
		if (DirectCostOn() && CMoveMath::GetPosSpeedMod(*md, p) < 0.99f)
			return false;
		if (CMoveMath::IsBlockedStructure(*md, int(p.x / SQUARE_SIZE), int(p.z / SQUARE_SIZE), owner, thread))
			return false;
	}

	if (YieldLog()) {
		float lo = startMod;
		for (int i = 1; i <= steps; i++) lo = std::min(lo, CMoveMath::GetPosSpeedMod(*md, owner->pos + delta * (float(i) / steps)));
		LOG("[SupComSteer] f=%d unit %d direct line clear: start mod %.3f, lowest on the line %.3f, %d samples, cost check %d", gs->frameNum, owner->id, startMod, lo, steps, int(DirectCostOn()));
	}
	return (directClear = true);
}

bool CSupComGroundMoveType::GetDirectSteer() const
{
	// the same conditions as SetWaypointDir, with the cached answer of DirectLineClear (re-checked at most
	// DIRECT_CHECK_FRAMES later); calling DirectLineClear here would change the cache, which is synced state
	return (progressState == Active && owner->commandAI->commandQue.size() <= 1 && directClear && goalPos.equals(directGoal));
}

bool CSupComGroundMoveType::LineOpen(const float3& a, const float3& b) const
{
	if (!LineFits(a, b))
		return false;

	// open ground only: a costly square is left to the path (PTH-001-D)
	const float3 d = (b - a) * XZVector;
	const int steps = std::max(1, int(math::ceil(d.Length2D() / (SQUARE_SIZE * 0.5f))));
	// and clear of walls and structures: the footprint moved sideways by the clearance fits too (PTH-002 A and B,
	// PTH-004-D)
	const float3 side = float3(-d.z, 0.0f, d.x).SafeNormalize2D() * SMOOTH_CLEARANCE;

	for (int k = 1; k <= steps; k++) {
		const float3 p = a + d * (float(k) / steps);

		if (CMoveMath::GetPosSpeedMod(*owner->moveDef, p) < 0.99f)
			return false;
		if (!FootprintFitsAt(p + side) || !FootprintFitsAt(p - side))
			return false;
	}

	return true;
}

float3 CSupComGroundMoveType::SmoothedWaypoint(const float3& cwp, const float3& opos)
{
	if (!PathSmoothOn() || pathID == 0)
		return cwp;

	// the target holds until the re-check, unless reached
	if (pathID == smoothPathID && gs->frameNum < smoothFrame && smoothTarget.SqDistance2D(opos) > Square(SQUARE_SIZE))
		return smoothTarget;

	// read again at every re-check: the engine re-plans a path under the same id
	{
		std::vector<int> starts;
		pathManager->GetPathWayPoints(pathID, smoothPts, starts);
		smoothPathID = pathID;
	}

	smoothFrame = gs->frameNum + SMOOTH_RECHECK_FRAMES;
	smoothTarget = cwp;

	// the path point nearest the engine's current waypoint, then the furthest one ahead in a straight open line
	int i0 = -1;
	float best = std::numeric_limits<float>::max();

	for (int i = 0; i < int(smoothPts.size()); i++) {
		const float d = smoothPts[i].SqDistance2D(cwp);

		if (d < best) {
			best = d;
			i0 = i;
		}
	}

	if (i0 < 0)
		return cwp;

	const int iEnd = std::min(int(smoothPts.size()) - 1, i0 + SMOOTH_LOOKAHEAD);

	for (int k = i0 + 1; k <= iEnd; k++) {
		if (!LineOpen(opos, smoothPts[k]))
			break;

		smoothTarget = smoothPts[k];
	}

	// the unit past the engine's waypoint along the path (the path point nearest the unit comes after it): the engine
	// moves on to its next one
	if (!smoothTarget.same(cwp)) {
		int iu = -1;
		float bu = std::numeric_limits<float>::max();

		for (int i = 0; i < int(smoothPts.size()); i++) {
			const float d = smoothPts[i].SqDistance2D(opos);

			if (d < bu) {
				bu = d;
				iu = i;
			}
		}

		if (iu > i0)
			TriggerSkipWayPoint();
	}

	return smoothTarget;
}

void CSupComGroundMoveType::SetWaypointDir(const float3& cwp, const float3& opos)
{
	// a clear straight line to the goal is driven directly, not through the path's waypoints
	if (progressState == Active && owner->commandAI->commandQue.size() <= 1 && DirectLineClear()) {
		if (YieldLog() && !goalPos.same(scLogTarget)) {
			scLogTarget = goalPos;
			LOG("[SupComSteer] f=%d unit %d direct to goal (%.0f, %.0f) from (%.0f, %.0f)", gs->frameNum, owner->id, goalPos.x, goalPos.z, opos.x, opos.z);
		}
		CGroundMoveType::SetWaypointDir(goalPos, opos);
		return;
	}

	// the engine's waypoints can lie where this move type's centred footprint does not fit (beside a wall: the engine
	// places an even-sized footprint off its centre), or round a corner the footprint cannot cut: repair them locally
	float3 target = FitWaypoint(SmoothedWaypoint(cwp, opos));
	if (YieldLog() && !target.same(scLogTarget)) {
		scLogTarget = target;
		LOG("[SupComSteer] f=%d unit %d path waypoint (%.0f, %.0f) fitted (%.0f, %.0f) from (%.0f, %.0f)", gs->frameNum, owner->id, cwp.x, cwp.z, target.x, target.z, opos.x, opos.z);
	}

	// at a repaired waypoint the engine does not see its own as reached (it skips only within 7 elmos): ask it to
	// move on to the next one
	if (!target.same(cwp) && target.SqDistance2D(opos) < Square(SQUARE_SIZE * 0.5f))
		TriggerSkipWayPoint();

	CGroundMoveType::SetWaypointDir(GapQueueTarget(cwp, opos, target), opos);
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

	// supcom_turn_in_place_rate (deg/s): a unit stopped or nearly stopped (under a tenth of its top speed) turns at
	// least this fast, so a moving turn radius above 0 keeps a standstill turn time
	if (inPlaceRate > 0.0f && math::fabs(scSpeed) <= maxSpeed * 0.1f)
		rate = std::max(rate, inPlaceRate);

	turnRate = std::clamp(rate, 1.0f, SPRING_CIRCLE_DIVS * 0.5f - 1.0f);
	turnAccel = turnRate * turnAccelFactor;
}


void CSupComGroundMoveType::HoldEarlyArrival()
{
	// the engine ends a move in a crowd outside the goal radius: on touching a unit at or near the goal, or with the
	// radius widened while the unit waits (PTH-004 A and B ended orders up to 6 squares short): the arrival waits
	if (!CrowdArriveOn() || progressState != Active || !pathingArrived || pathingFailed || crowdFrames >= CROWD_MAX_FRAMES)
		return;
	if (owner->pos.SqDistance2D(goalPos) <= Square(goalRadius + extraRadius))
		return;

	pathingArrived = false;
	atGoal = false;
	atEndOfPath = false;
	crowdFrames++;
}

void CSupComGroundMoveType::HeldLegProgress()
{
	// a unit on a held last leg (HoldLastLeg) that circles the goal without getting closer (a turn circle wider than its goal
	// radius: base-cross L4813, tank orbiting 8.0 to 8.2 elmos from a goal radius of 8) gets the stock failure after a second of
	// no progress, from where it is (values: 1 s and 0.5 elmo are DESIGN, this function: well under one orbit, over float noise)
	if (holdBestDist < 0.0f)
		return;
	if (holdGoal != goalPos || progressState != Active || atGoal || pathingArrived || pathingFailed) {
		holdBestDist = -1.0f;
		return;
	}
	const float dist = owner->pos.distance2D(goalPos);
	if (dist < holdBestDist - 0.5f) {
		holdBestDist = dist;
		holdBestFrame = gs->frameNum;
		return;
	}
	if (gs->frameNum - holdBestFrame <= GAME_SPEED)
		return;
	holdBestDist = -1.0f;
	pathingFailed = true;
}

bool CSupComGroundMoveType::IsAtGoal() const
{
	// the command AI finishes the order on this: a crowd arrival set by this frame's collisions waits as in HoldEarlyArrival
	if (atGoal && CrowdArriveOn() && progressState == Active && crowdFrames < CROWD_MAX_FRAMES)
		return (owner->pos.SqDistance2D(goalPos) <= Square(goalRadius + extraRadius));

	return atGoal;
}

void CSupComGroundMoveType::UpdateUnitPosition()
{
	RECOIL_DETAILED_TRACY_ZONE;

	// before the engine acts on this frame's arrival flags (UpdatePreCollisions)
	HoldEarlyArrival();
	HeldLegProgress();

	if (owner->UnderFirstPersonControl()) {
		CGroundMoveType::UpdateUnitPosition();

		scSpeed = math::fabs(owner->speed.dot(flatFrontDir));
		scPhase = PHASE_DRIVE;
		return;
	}

	// refused by a structure for half a second while following a path: the planner, with the move class's
	// square-offset footprint, cut the corner of a structure that the engine's own position test then refuses (a mex
	// beside a factory exit, Q100), and this move type's exact steps cannot get round it. The engine's own ground
	// movement slides along the structure, as stock units do, until the unit has moved two squares or two seconds pass.
	if (scHandoverFrames == 0 && scStructFrames >= STRUCT_HANDOVER_FRAMES && progressState == Active) {
		scHandoverFrames = STRUCT_HANDOVER_TIME;
		scHandoverFrom = owner->pos;
		scStructFrames = 0;
	}
	if (scHandoverFrames > 0) {
		if (progressState == Active && owner->pos.SqDistance2D(scHandoverFrom) < Square(SQUARE_SIZE * 2.0f)) {
			scHandoverFrames--;
			CGroundMoveType::UpdateUnitPosition();

			scSpeed = math::fabs(owner->speed.dot(flatFrontDir));
			scPhase = PHASE_DRIVE;
			scBraking = false;
			scRestart = false;
			return;
		}

		scHandoverFrames = 0;
		scBlockedFrames = 0;
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
			wantReverse = !HeadingLocked() && WantReverseTurnGo(waypointDir, flatFrontDir);
			backTurn = !wantReverse && WantBackTurn();

			// MOV-020 R4 (courses L5937, commander 150 deg turn-back): moved on to a new point while driving (the dip), the unit
			// keeps driving forwards and turns while moving, with no reverse and no back-turn to rest, until the goal is no
			// longer behind it or it has stopped
			if (wpThrough) {
				const float3 toGoal = (goalPos - owner->pos) * XZVector;
				if (scSpeed <= 0.0f || toGoal.SqLength2D() <= 1e-6f || toGoal.dot(flatFrontDir) >= 0.0f) {
					wpThrough = false;
				} else {
					wantReverse = false;
					backTurn = false;
				}
			}
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

	const int wantDir = (wantReverse || backTurn)? -1: 1;

	float topSpeed = (wantReverse || backTurn)? maxReverseSpeed: maxSpeed;

	// under water, the move class's depth speed rule (depthModParams), the same one the engine's own ground movement
	// applies through its terrain speed: one rule for both move types (the game's default is no slowdown)
	// on the sea floor (amphibious) or on water (hover), the unit's water speed factor (MOV-009 R7); ships and
	// submarines keep their speed (MOV-011 R1)
	{
		const MoveDef* md = owner->moveDef;
		const float terrain = CGround::GetHeightReal(owner->pos.x, owner->pos.z);

		if (!md->FloatOnWater())
			topSpeed *= md->GetDepthMod(terrain);
		// hover units over water; amphibious units on the sea floor (their layer, MOV-009 R8)
		const bool inWaterLayer = (md->speedModClass == MoveDef::Hover)? (terrain < 0.0f): OnSeabed();

		if (inWaterLayer && !submarine && md->speedModClass != MoveDef::Ship)
			topSpeed *= waterSpeedFactor;
	}

	if (maxWantedSpeed > 0.0f)
		topSpeed = std::min(topSpeed, maxWantedSpeed);

	// MOV-020 R3: during the dip after moving on to a new point, no faster than the dip speed
	if (gs->frameNum < wpDipUntil)
		topSpeed = std::min(topSpeed, wpDip * maxSpeed);

	// corner speed (owner pathing courses 2026-11-15): before a queued move point whose next leg turns away, the unit
	// slows so that the turn at its turn rate stays within the arrival radius of the point: the arc tangent to both legs
	// passes the point at r * (1 / cos(d / 2) - 1) for a deflection d, so r <= arrival / that, and the speed cap is
	// turnRate (rad/frame) * r; braking at decRate reaches the cap at the point. Straight on (d under 10 deg): no cap.
	if (MakeWayOn() && !stop && owner->commandAI != nullptr) {
		const CCommandQueue& q = owner->commandAI->commandQue;
		if (CornerLog() && (gs->frameNum + owner->id) % GAME_SPEED == 0)
			LOG("[SupComCorner] f=%d u=%d q=%d ids %d %d goal (%.0f,%.0f) top %.3f turnRate %.1f", gs->frameNum, owner->id, int(q.size()), q.empty()? 0: q[0].GetID(), (q.size() < 2)? 0: q[1].GetID(), goalPos.x, goalPos.z, topSpeed, turnRate);

		// the point being driven to is the goal; the next leg runs to the first later queued command with a position
		// more than a square from it (a game's own waypoint-step command may stand in front of the move it serves)
		const float3 p0 = goalPos * XZVector;
		float3 p1;
		bool haveNext = false;
		bool seenGoal = false;

		for (size_t k = 0; k < q.size() && k < 8 && !haveNext; k++) {
			if (q[k].GetNumParams() < 3)
				continue;
			const float3 pk(q[k].GetParam(0), 0.0f, q[k].GetParam(2));
			if (!seenGoal) {
				seenGoal = (pk.distance2D(p0) < SQUARE_SIZE);
				continue;
			}
			if (pk.distance2D(p0) > SQUARE_SIZE) {
				p1 = pk;
				haveNext = true;
			}
		}

		float cornerFloor = 0.0f; // the corner arc speed below which the turn-circle cap must not go while a further point is queued
		if (haveNext) {
			const float3 here = owner->pos * XZVector;
			const float3 in = (p0 - here).SafeNormalize2D();
			const float3 out = (p1 - p0).SafeNormalize2D();
			const float d = math::acos(std::clamp(in.dot(out), -1.0f, 1.0f));
			const float toPoint = (p0 - here).Length2D();

			if (d > 10.0f * math::DEG_TO_RAD) {
				constexpr float MIN_CORNER_RADIUS = 1.0f;
				const float arrival = std::max(goalRadius, float(SQUARE_SIZE));
				const float sec1 = 1.0f / std::max(math::cos(d * 0.5f), 0.05f) - 1.0f;
				const float r = std::max(arrival / std::max(sec1, 1e-3f), MIN_CORNER_RADIUS);
				const float w = turnRate * math::TWOPI / SPRING_CIRCLE_DIVS;
				const float cap = w * r;
				const float allow = math::sqrt(cap * cap + 2.0f * decRate * std::max(toPoint - arrival, 0.0f));

				if (CornerLog() && (gs->frameNum + owner->id) % 5 == 0)
					LOG("[SupComCorner] f=%d u=%d defl %.0f to point %.1f cap %.3f allow %.3f top %.3f speed %.3f", gs->frameNum, owner->id, d * math::RAD_TO_DEG, toPoint, cap, allow, topSpeed, scSpeed);
				// MOV-020 R4 (courses L5937 commander course 3, 150 deg): a unit with the waypoint dip slows into a corner no
				// further than the dip speed and makes the turn while moving after it
				const float dipFloor = (wpDip > 0.0f && wpDipFrames > 0)? wpDip * maxSpeed: 0.0f;
				topSpeed = std::min(topSpeed, std::max({allow, cap, dipFloor}));
				cornerFloor = std::max(cap, dipFloor);
			}
		}

		// turn circle (courses trace L5070: a tank arriving on the heading its last turn left it with, 16 deg against a 60 deg
		// leg, had the next point inside its turn circle and looped round it at full speed): a target at distance D and angle
		// phi off the heading lies inside the circle of radius R tangent to the heading when D < 2 R sin(phi), so the speed
		// is capped at turnRate x D / (2 sin phi) (x 0.9 margin) for the goal, and near the goal (within 3 arrival radii)
		// for the next queued point too. No cap for a target within 15 deg of straight ahead or behind (turn on the spot
		// and reversing handle those); never below a tenth of the top speed.
		{
			const float3 here = owner->pos * XZVector;
			const float3 fwd = (flatFrontDir * XZVector).SafeNormalize2D();
			const float w = turnRate * math::TWOPI / SPRING_CIRCLE_DIVS;
			const float arrival = std::max(goalRadius, float(SQUARE_SIZE));
			const auto CircleCap = [&](const float3& target) {
				const float3 rel = (target * XZVector) - here;
				const float D = rel.Length2D();
				if (D <= 1e-3f)
					return topSpeed;
				const float sinPhi = math::fabs(fwd.x * rel.z - fwd.z * rel.x) / D;
				if (sinPhi < math::sin(15.0f * math::DEG_TO_RAD))
					return topSpeed;
				return std::max(0.9f * w * D / (2.0f * sinPhi), 0.1f * topSpeed);
			};

			float circ = CircleCap(p0);
			if (haveNext && (p0 - here).Length2D() <= 3.0f * arrival)
				circ = std::min(circ, CircleCap(p1));
			// courses trace L5115 lane 12 (MOV-020 R2: no stop at an intermediate point): with a further point queued the cap
			// keeps at least the corner arc speed (an arc passing within the arrival radius), so the unit rounds the
			// corner instead of slowing to under 3 elmos/s and turning on the spot.
			if (haveNext)
				circ = std::max(circ, std::min(cornerFloor, topSpeed));
			if (CornerLog() && circ < topSpeed && (gs->frameNum + owner->id) % 5 == 0)
				LOG("[SupComCorner] f=%d u=%d circle cap %.3f (top %.3f, speed %.3f)", gs->frameNum, owner->id, circ, topSpeed, scSpeed);
			topSpeed = std::min(topSpeed, circ);
		}
	}

	wantedSpeed = topSpeed * (1 - stop);

	// distance left to the point where the unit should be at rest: the goal itself
	// for move orders, the goal radius for orders that only need to come within reach
	float stopDist = NO_STOP;

	if (!stop && owner->commandAI->commandQue.size() <= 1) {
		const float stopRadius = ((goalRadius > SQUARE_SIZE)? goalRadius: 0.0f) + extraRadius;

		stopDist = goalPos.distance2D(owner->pos) - stopRadius;

		// at rest well short of the stopping point (pushed away, or the path bent): go again
		if (scBraking && scSpeed <= 0.0f && stopDist > ((goalRadiusParam >= 0.0f)? std::max(goalRadius, 0.5f): std::max(goalRadius, float(SQUARE_SIZE))))
			scBraking = false;
	}

	// SupComGapQueue: at rest no nearer the gap than a footprint behind the unit queued before this one
	if (!stop && gqAhead) {
		const float room = std::max(gqRoom, 0.0f);
		const float need = StoppingDistance(scSpeed) + GAPQ_RESUME;

		// braking for the queue, not for the goal: drive again once the room ahead exceeds the stopping distance
		if (gqHeld && scBraking && room > need && stopDist > need)
			scBraking = false;

		gqHeld = (room < stopDist && room <= need + scSpeed);
		stopDist = std::min(stopDist, room);
	} else if (gqHeld) {
		if (scBraking && !stop && stopDist > StoppingDistance(scSpeed) + GAPQ_RESUME)
			scBraking = false;

		gqHeld = false;
	}

	float dist = 0.0f;

	// supcom_blocked_restart_delay: held against the map edge (blocked there, or resting there), the next movement from rest waits this long (counted from the first
	// frame it wants to move again; a new goal restarts the count)
	bool deadHold = false;
	// resting with the footprint against the map edge counts as held there (an order that ended on arrival at the edge)
	if (restartDelay > 0 && scSpeed <= 0.0f && !blockedSinceMove && FootprintAtMapEdge())
		blockedSinceMove = true;
	// at rest against the map edge with an order: the edge blocked the last step, so the next one is a blocked restart (MOV-007 R3)
	if (blockedRestart > 0.0f && scSpeed <= 0.0f && !stop && !scBraking && FootprintAtMapEdge())
		scRestart = true;
	if (restartDelay > 0 && blockedSinceMove && !stop && scSpeed <= 0.0f) {
		if (!goalPos.equals(deadGoal)) {
			deadGoal = goalPos;
			deadUntil = -1;
		}
		if (deadUntil < 0)
			deadUntil = gs->frameNum + restartDelay;
		if (gs->frameNum < deadUntil) {
			deadHold = true;
		} else {
			// the hold ends as a blocked restart: the first step is taken at the restart speed
			blockedSinceMove = false;
			deadUntil = -1;
			scRestart = !scBraking;
		}
	}

	if (deadHold) {
		scSpeed = 0.0f;
		dist = 0.0f;
	} else if (scRestart && !stop && !scBraking) {
		// the step after a blocked one is taken at the restart speed, once any turn on the spot from rest is done
		scDir = wantDir;
		if (scSpeed <= 0.0f && !backTurn && HoldForTurn(wantDir)) {
			dist = 0.0f;
		} else {
			scPhase = PHASE_DRIVE;
			scSpeed = std::min(topSpeed, blockedRestart * maxSpeed);
			dist = scSpeed;
		}
	} else if (scSpeed > 0.0f && wantDir != scDir) {
		// change of direction: brake to rest first
		dist = Advance(topSpeed, NO_STOP, true);
	} else {
		if (scSpeed <= 0.0f)
			scDir = wantDir;

		// SupComTurnGo: from rest, turn on the spot until the heading (backwards when reversing) is within the move-off
		// angle of the way; the base class turns the heading meanwhile
		if (scSpeed <= 0.0f && !stop && !backTurn && HoldForTurn(wantDir))
			dist = 0.0f;
		else
			dist = Advance(topSpeed, stopDist, stop);
	}

	// a pending restart waits through frames at rest (turning on the spot, the edge dead time) for the first step taken
	if (dist > 0.0f || stop || scBraking)
		scRestart = false;

	const float3 motionDir = MotionDir();
	float3 moveRequest = motionDir * (dist * scDir);

	moveRequest.x = GridStep(owner->pos.x, moveRequest.x);
	moveRequest.z = GridStep(owner->pos.z, moveRequest.z);

	if (!moveRequest.same(ZeroVector)) {
		bool blocked = false;
		bool byStructure = false;

		if (!positionStuck) {
			blocked = (FootprintBlocked(owner->pos + moveRequest) && !FootprintBlocked(owner->pos));

			// a refused step lines up first (PTH-002: a unit at a gap its own width, a few elmos off its line, or
			// grazing the wall face by a hundredth of an elmo, was refused every step): the nearest sideways offset,
			// across the step's main axis, from which the forward part of the step fits, approached by at most the
			// step's length. Nearest first, the positive side first on a tie (deterministic).
			if (blocked && MakeWayOn()) {
				const bool alongX = (math::fabs(moveRequest.x) >= math::fabs(moveRequest.z));
				const float3 fwd = alongX? float3(moveRequest.x, 0.0f, 0.0f): float3(0.0f, 0.0f, moveRequest.z);
				const float3 lat = alongX? float3(0.0f, 0.0f, 1.0f): float3(1.0f, 0.0f, 0.0f);
				const float stepLen = moveRequest.Length2D();

				// candidates: 0 (the forward part alone: a heading a little off a gap's axis grazes its walls), then the
				// footprint's lower edge snapped onto the square boundary below or above it (a gap exactly its own width
				// leaves only FOOTPRINT_SIDE_EPS of play), the smaller shift first, then a grid either side
				const float latPos = alongX? owner->pos.z: owner->pos.x;
				const float lowEdge = latPos - std::max(footprintX, footprintZ) * 0.5f + FOOTPRINT_SIDE_EPS;
				const float below = math::floor(lowEdge / SQUARE_SIZE) * SQUARE_SIZE - lowEdge + 0.001f;
				const float above = below + SQUARE_SIZE;
				std::vector<float> cands = {0.0f};
				if (math::fabs(below) <= math::fabs(above)) { cands.push_back(below); cands.push_back(above); } else { cands.push_back(above); cands.push_back(below); }
				for (int k = 1; k <= int(LINEUP_MAX / LINEUP_STEP) * 2; k++)
					cands.push_back(LINEUP_STEP * float((k + 1) / 2) * ((k & 1)? 1.0f: -1.0f));

				for (size_t k = 0; k < cands.size() && blocked; k++) {
					const float d = cands[k];
					const float3 off = lat * d;

					if (math::fabs(d) > LINEUP_MAX)
						continue;
					if (FootprintBlocked(owner->pos + off + fwd) || (k > 0 && !LineFits(owner->pos, owner->pos + off)))
						continue;

					const float m = std::min(math::fabs(d), stepLen);
					moveRequest = (k == 0)? fwd: lat * ((d > 0.0f)? m: -m);
					moveRequest.x = GridStep(owner->pos.x, moveRequest.x);
					moveRequest.z = GridStep(owner->pos.z, moveRequest.z);
					blocked = FootprintBlocked(owner->pos + moveRequest);
				}

				// no line-up within reach (pressed against a wall face further from the gap): the step's other part alone,
				// along the wall towards where the unit steers; the line-up above takes over once it is near the gap's line
				if (blocked) {
					const float3 side = alongX? float3(0.0f, 0.0f, moveRequest.z): float3(moveRequest.x, 0.0f, 0.0f);

					if (!side.same(ZeroVector) && !FootprintBlocked(owner->pos + side)) {
						moveRequest = side;
						blocked = false;
					}
				}
			}

			if (!blocked) {
				float3 resultantMove;
				UpdatePos(owner, moveRequest, resultantMove, ThreadPool::GetThreadNum());
				blocked = !resultantMove.same(moveRequest);

				// refused by a structure under the engine's own footprint (not by terrain or a unit)
				if (blocked) {
					const float3 next = owner->pos + moveRequest;
					const int nsx = int(next.x / SQUARE_SIZE);
					const int nsz = int(next.z / SQUARE_SIZE);

					byStructure = (CMoveMath::IsBlockedStructure(*owner->moveDef, nsx, nsz, owner, ThreadPool::GetThreadNum()) != 0);

					// courses L5866 lane 12 (MOV-020): a tank 4 elmos clear of a structure was refused by the engine's square
					// footprint (the move def's size) while its own footprint (supcom_footprint_x/z) touched no structure square;
					// this move type's footprint decides, so the step is taken
					if (byStructure && !FootprintBlocked(next) && !FootprintOnStructure(next)) {
						byStructure = false;
						blocked = false;
					}
				}
			}
		}

		if (blocked) {
			if (YieldLog() && scBlockedFrames == 0 && !stop) {
				const float3 next = owner->pos + moveRequest;
				yieldLogSquares = true; FootprintBlocked(next); FootprintBlocked(owner->pos); yieldLogSquares = false;
				QuadFieldQuery q;
				quadField.GetUnitsExact(q, next, HalfSize() + SQUARE_SIZE, false);
				std::string near;
				for (const CUnit* u: *q.units) { if (u != owner) near += " " + std::to_string(u->id) + "@(" + std::to_string(int(u->pos.x)) + "," + std::to_string(int(u->pos.z)) + ")"; }
				LOG("[SupComYield] f=%d unit %d wanted step (%.2f, %.2f) to (%.1f, %.1f); our footprint %s; units near:%s", gs->frameNum, owner->id, moveRequest.x, moveRequest.z, next.x, next.z, FootprintBlocked(next)? "blocked": "fits", near.c_str());
			}
			// the step is not taken and the speed is lost
			moveRequest = ZeroVector;
			scSpeed = 0.0f;
			scPhase = PHASE_DRIVE;
			scRestart = (!stop && !scBraking);
			// the restart delay is for a unit held against the map edge only (window B); a step blocked by a unit or a
			// structure inland (a walker under a transport, a queue) keeps moving off as before
			if (!stop && FootprintAtMapEdge()) {
				blockedSinceMove = true;
				deadUntil = -1;
			}
			if (YieldLog() && scBlockedFrames == 0 && !stop) {
				const bool byTerrain = FootprintBlocked(owner->pos + moveRequest) && !FootprintBlocked(owner->pos);
				LOG("[SupComYield] f=%d unit %d step refused by %s at (%.1f, %.1f) heading (%.2f, %.2f) step (%.2f, %.2f) wp (%.0f, %.0f)", gs->frameNum, owner->id,
					byStructure? "a structure": (byTerrain? "terrain": "a unit or the engine"), owner->pos.x, owner->pos.z, flatFrontDir.x, flatFrontDir.z, moveRequest.x, moveRequest.z, currWayPoint.x, currWayPoint.z);
			}
			scBlockedFrames += int(!stop);
			scStructFrames = (byStructure && !stop)? scStructFrames + 1: 0;

			ReRequestPath(false);
		} else {
			positionStuck = false;
			if (YieldLog() && scBlockedFrames > 0)
				LOG("[SupComYield] f=%d unit %d moves again after %d refused frames", gs->frameNum, owner->id, scBlockedFrames);
			scBlockedFrames = 0;
			scStructFrames = 0;
		}
	}

	oldSpeed = owner->speed.dot(motionDir);
	newSpeed = moveRequest.dot(motionDir);

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

bool CSupComGroundMoveType::WantReverseTurnGo(const float3& wpDir, const float3& ffDir) const
{
	if (reverseAngle > 0.0f && reverseDist > 0.0f)
		return WantReverse(wpDir, ffDir);
	// a reverse distance set: reverse exactly when the goal is behind (more than 90 degrees off the facing) and within it
	if (!TurnGoOn() || reverseDist <= 0.0f)
		return WantReverse(wpDir, ffDir);
	if (owner->unitDef->rSpeed <= 0.0f || maxReverseSpeed <= 0.0f)
		return false;

	const float3 toGoal = (goalPos - owner->pos) * XZVector;
	const float d = toGoal.Length2D();

	if (d <= 1e-3f || d > reverseDist)
		return false;

	return ((toGoal / d).dot(ffDir) < 0.0f);
}

bool CSupComGroundMoveType::WantReverse(const float3& wpDir, const float3& ffDir) const
{
	// MOV-005 R5: with supcom_reverse_angle the reverse choice (which also sets the heading: rear or front towards the way) is
	// measured to the goal, not to the current waypoint (the engine's maxReverseDist reads the waypoint, always near)
	if (reverseAngle <= 0.0f || reverseDist <= 0.0f)
		return CGroundMoveType::WantReverse(wpDir, ffDir);
	if (owner->unitDef->rSpeed <= 0.0f || maxReverseSpeed <= 0.0f)
		return false;

	const float3 toGoal = (goalPos - owner->pos) * XZVector;
	const float d = toGoal.Length2D();

	if (d <= 1e-3f || d > reverseDist)
		return false;

	return ((toGoal / d).dot(ffDir) <= -math::cos((180.0f - reverseAngle) * math::DEG_TO_RAD) + 1e-6f);
}

bool CSupComGroundMoveType::WantBackTurn() const
{
	// MOV-005 R5, second branch: the goal behind (more than a right angle off the heading) but not a reverse goal
	if (reverseAngle <= 0.0f || reverseDist <= 0.0f || HeadingLocked())
		return false;
	if (owner->unitDef->rSpeed <= 0.0f || maxReverseSpeed <= 0.0f)
		return false;

	const float3 toGoal = (goalPos - owner->pos) * XZVector;
	const float d = toGoal.Length2D();

	return (d > 1e-3f && (toGoal / d).dot(flatFrontDir) < 0.0f);
}

bool CSupComGroundMoveType::HoldForTurn(int dir) const
{
	if (!TurnGoOn() || turnGoCos > 1.0f || HeadingLocked())
		return false;

	float3 w = waypointDir * XZVector;

	if (w.SqLength2D() <= 1e-6f)
		return false;

	w.SafeNormalize2D();
	return ((flatFrontDir * float(dir)).dot(w) < turnGoCos);
}

bool CSupComGroundMoveType::HeadingLocked() const
{
	return (headingFree && owner->moveDef != nullptr && owner->moveDef->speedModClass == MoveDef::Hover);
}

float3 CSupComGroundMoveType::MotionDir() const
{
	if (!HeadingLocked())
		return flatFrontDir;

	float3 d = waypointDir * XZVector;

	if (d.SqLength2D() <= 1e-6f)
		return flatFrontDir;

	return d.SafeNormalize2D();
}

bool CSupComGroundMoveType::SetMemberValue(unsigned int memberHash, void* memberValue)
{
	static const unsigned int SUBMERGED_HASH = spring::LiteHash("submerged", sizeof("submerged") - 1, 0);
	static const unsigned int HEADING_FREE_HASH = spring::LiteHash("supcomHeadingFree", sizeof("supcomHeadingFree") - 1, 0);

	if (memberHash == HEADING_FREE_HASH) {
		if (owner->moveDef == nullptr || owner->moveDef->speedModClass != MoveDef::Hover)
			return false;

		headingFree = *(reinterpret_cast<bool*>(memberValue));
		return true;
	}

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

	return (OnSeabed()? "seabed": "land");
}

bool CSupComGroundMoveType::OnSeabed() const
{
	if (seabedSwitchDepth <= 0.0f)
		return (CGround::GetHeightReal(owner->pos.x, owner->pos.z) < 0.0f);

	return onSeabed;
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

	// MOV-009 R8: Land -> Seabed at the switch depth, back to Land as the depth falls to zero
	if (seabedSwitchDepth > 0.0f) {
		const float depth = -CGround::GetHeightReal(owner->pos.x, owner->pos.z);

		if (!onSeabed && depth >= seabedSwitchDepth)
			onSeabed = true;
		else if (onSeabed && depth <= 0.0f)
			onSeabed = false;
	}

	// arrived (or stopped) after making way: an ordinary idle unit again
	if (scYielding && progressState != Active) {
		scYielding = false;
		if (YieldLog())
			LOG("[SupComYield] f=%d unit %d done making way at (%.0f, %.0f)", gs->frameNum, owner->id, owner->pos.x, owner->pos.z);
	}

	UpdateGapQueue();
	// held behind a queue that moves (a stuck unit ahead is passed over): not idle, so the engine neither re-plans nor
	// fails the order (it failed units waiting their turn after eight seconds, PTH-002-B)
	if (gqHeld && gqAhead) {
		numIdlingUpdates = 0;
		numIdlingSlowUpdates = 0;
	}
	CheckYield();
	return moved;
}

bool CSupComGroundMoveType::GapAt(const float3& wp, float3& at, float3& dir) const
{
	// the waypoint, or the nearest point to it where the footprint fits
	at = wp;

	if (FootprintBlocked(at)) {
		bool found = false;

		for (const float3& o: NearOffsets()) {
			if (!FootprintBlocked(wp + o)) {
				at = wp + o;
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	const float w = HalfSize() * 2.0f;
	const float3 axes[2] = {float3(1.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f)};

	for (int k = 0; k < 2; k++) {
		const float3& ax = axes[k];
		const float3& lat = axes[1 - k];

		// walls on both sides across the axis, open ground before and after along it
		if (!FootprintBlocked(at + lat * w) || !FootprintBlocked(at - lat * w))
			continue;
		if (FootprintBlocked(at + ax * w) || FootprintBlocked(at - ax * w))
			continue;

		dir = ax * (((at - owner->pos).dot(ax) >= 0.0f)? 1.0f: -1.0f);
		return true;
	}

	return false;
}

void CSupComGroundMoveType::UpdateGapQueue()
{
	// runs in the serial move type update: it reads other units' queue state, written here or in the parallel phase
	gqAhead = false;

	if (!GapQueueOn() || progressState != Active || scYielding || owner->GetTransporter() != nullptr) {
		gqKey = -1;
		gqFrame = -1;
		gqHeld = false;
		return;
	}

	const float3& pos = owner->pos;

	// a new gap: the current or the next waypoint, at least half a footprint ahead with the goal beyond it, not the one
	// passed on this order
	if (gqKey < 0) {
		for (const float3& wp: {currWayPoint, nextWayPoint}) {
			float3 at;
			float3 dir;

			if (wp.SqDistance2D(pos) > Square(GAPQ_REACH * 3.0f) || !GapAt(wp, at, dir))
				continue;

			const int key = int(at.z / SQUARE_SIZE) * mapDims.mapx + int(at.x / SQUARE_SIZE);

			if (key == gqDoneKey || (at - pos).dot(dir) < HalfSize() || (goalPos - at).dot(dir) <= 0.0f)
				continue;

			gqKey = key;
			gqPos = at;
			gqDir = dir;
			gqFrame = -1;
			break;
		}
		if (gqKey < 0)
			return;
	}

	const float s = (gqPos - pos).dot(gqDir);
	const float d = pos.distance2D(gqPos);

	// through the gap: out of its queue
	if (s < -(HalfSize() + SQUARE_SIZE)) {
		gqDoneKey = gqKey;
		gqKey = -1;
		gqFrame = -1;
		gqHeld = false;
		return;
	}

	// arrived at the gap: in its queue from this frame (ties by unit id)
	if (gqFrame < 0) {
		if (s > GAPQ_REACH || d > GAPQ_REACH * 3.0f)
			return;

		gqFrame = gs->frameNum;
		gqBest = s;
		gqBestFrame = gs->frameNum;
	}
	// progress: along the gap's axis, before and after it
	if (s < gqBest - 0.5f) {
		gqBest = s;
		gqBestFrame = gs->frameNum;
	}

	// the unit ahead: the last one queued before this one at the same gap in the same direction; one stuck (neither
	// held by the queue nor any further along the gap's axis for GAPQ_STALL_FRAMES) is passed over
	QuadFieldQuery q;
	quadField.GetUnitsExact(q, gqPos, GAPQ_REACH * 3.0f + SQUARE_SIZE * 2.0f, false);

	const CSupComGroundMoveType* ahead = nullptr;

	for (const CUnit* u: *q.units) {
		if (u == owner)
			continue;

		const CSupComGroundMoveType* mt = dynamic_cast<const CSupComGroundMoveType*>(u->moveType);

		if (mt == nullptr || mt->gqKey != gqKey || mt->gqFrame < 0 || mt->gqDir.dot(gqDir) <= 0.0f)
			continue;
		if (mt->gqFrame > gqFrame || (mt->gqFrame == gqFrame && u->id > owner->id))
			continue;
		if (!mt->gqHeld && (gs->frameNum - mt->gqBestFrame) > GAPQ_STALL_FRAMES)
			continue;
		if (ahead == nullptr || mt->gqFrame > ahead->gqFrame || (mt->gqFrame == ahead->gqFrame && u->id > ahead->owner->id))
			ahead = mt;
	}

	if (ahead == nullptr)
		return;

	const float own = GapDist(pos);
	const float aheadDist = GapDist(ahead->owner->pos);

	// already nearer the gap than the unit queued before it (it got ahead of it): waiting cannot let that unit by in a
	// gap one unit wide (units held inside the gap for seconds, PTH-002-A); it goes on
	if (aheadDist > own)
		return;

	// the room left: this unit's way to the gap less the unit ahead's and a footprint, plus the distance the unit ahead
	// needs to come to rest (it cannot stop sooner: a follower keeps moving while it does, instead of stopping a footprint
	// behind it and starting again, PTH-002-A throughput)
	gqRoom = own - aheadDist - (HalfSize() + ahead->HalfSize() + GAPQ_MARGIN) + ahead->StoppingDistance(ahead->scSpeed);
	gqAhead = true;
}

float CSupComGroundMoveType::GapDist(const float3& p) const
{
	// the way from p to the gap's centre: straight on its line, by its mouth from off it (GapQueueTarget), so that units
	// waiting off the line keep their room round the mouth rather than closing in on the wall face; negative beyond it
	const float s = (gqPos - p).dot(gqDir);
	const float d = p.distance2D(gqPos);

	if (s <= 0.0f)
		return -d;

	const float across = math::fabs((p - gqPos).dot(float3(gqDir.z, 0.0f, -gqDir.x)));
	const float w = HalfSize() * 2.0f;

	const float mouth = w * GAPQ_MOUTH;

	if (across <= 1.0f || s <= mouth)
		return d;

	return (p.distance2D(gqPos - gqDir * mouth) + mouth);
}

float3 CSupComGroundMoveType::GapQueueTarget(const float3& cwp, const float3& opos, const float3& target) const
{
	// the engine moves on from the gap's waypoint when the unit ahead touches it: a unit still off the gap's line then
	// steered at a waypoint behind the wall and ground along it (PTH-002-A); one heading at the gap's own waypoint from
	// off its line caught the wall's corner. To the gap's mouth on its line first, GAPQ_MOUTH footprints before it, so
	// that a unit is on the line before the wall and drives through at its full speed.
	const float w = HalfSize() * 2.0f;
	const float mouth = w * GAPQ_MOUTH;

	if (!GapQueueOn() || gqKey < 0 || gqFrame < 0 || (gqPos - cwp).dot(gqDir) > mouth)
		return target;
	// not held by the queue and no nearer the gap for GAPQ_STALL_FRAMES twice over (something stands in it): the path's
	// own waypoints, as without the queue, so that the engine sees a blocked unit at rest and ends its order (aimed at the
	// gap, a unit blocked by an enemy standing in it was knocked about and its order never ended, PTH-002-D)
	if (!gqHeld && (gs->frameNum - gqBestFrame) > GAPQ_STALL_FRAMES * 2)
		return target;

	const float s = (gqPos - opos).dot(gqDir);
	const float across = math::fabs((opos - gqPos).dot(float3(gqDir.z, 0.0f, -gqDir.x)));

	if (s <= 0.0f || across <= 1.0f)
		return target;
	if (s > mouth + 1.0f)
		return (gqPos - gqDir * mouth);
	// past the mouth within the line-up's reach of the line: through the gap's centre
	if (across <= LINEUP_MAX)
		return gqPos;

	// past the mouth further off: onto the line half a footprint on (beside the wall face: straight across to it)
	return (gqPos - gqDir * ((s > w)? std::max(s - w * 0.5f, w): s));
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

// distance from p to the segment a-b, in the horizontal plane
static float SegmentDist2D(const float3& p, const float3& a, const float3& b)
{
	const float3 ab = (b - a) * XZVector;
	const float len2 = ab.SqLength2D();
	const float t = (len2 > 0.0f)? std::clamp(((p - a) * XZVector).dot(ab) / len2, 0.0f, 1.0f): 0.0f;

	return (a + ab * t).distance2D(p);
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

	// PTH-004 F (Reference recheck 2026-11-15: 0 of 16 engineers moved when a tank left a factory): no make-way on a factory exit
	if (MakeWayOn()) {
		for (const CUnit* u: *qfQuery.units) {
			if (u->unitDef == nullptr || !u->unitDef->IsFactoryUnit())
				continue;
			if (math::fabs(pos.x - u->pos.x) <= u->xsize * SQUARE_SIZE * 0.5f && math::fabs(pos.z - u->pos.z) <= u->zsize * SQUARE_SIZE * 0.5f) {
				scYieldFrames = 0;
				scYieldUnit = -1;
				return;
			}
		}
	}

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

		// ahead on the mover's line (within the two footprints sideways) up to YIELD_LOOKAHEAD_FRAMES of travel further
		bool onLineAhead = false;
		if (MakeWayOn() && dist > contact && scSpeed > 0.0f) {
			const float along = d.dot(front);
			const float across = math::fabs(d.dot(float3(-front.z, 0.0f, front.x)));
			onLineAhead = (along > 0.0f && along <= contact + scSpeed * YIELD_LOOKAHEAD_FRAMES && across <= HalfSize() + mt->HalfSize() + SQUARE_SIZE * 0.5f);
		}

		if ((dist > contact && !onLineAhead) || !inFront)
			continue;
		// PTH-004 F (Reference recheck 2026-11-15: an idle unit yields only when it is actually in the path): the idle
		// unit's footprint must overlap the mover's swept footprint along its way to the next two waypoints
		if (MakeWayOn()) {
			const float overlap = HalfSize() + mt->HalfSize();
			const float pathDist = std::min(SegmentDist2D(u->pos, pos, currWayPoint), SegmentDist2D(u->pos, currWayPoint, nextWayPoint));
			if (pathDist >= overlap)
				continue;
		}
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
		if (YieldLog())
			LOG("[SupComYield] f=%d mover %d in contact with idle %d (%.1f elmos)", gs->frameNum, owner->id, blocker->owner->id, owner->pos.distance2D(blocker->owner->pos));
	}

	if (++scYieldFrames < YIELD_DELAY_FRAMES)
		return;

	scYieldFrames = 0;
	const bool made = blocker->MakeWay(this);
	if (YieldLog())
		LOG("[SupComYield] f=%d mover %d asks %d to make way: %s", gs->frameNum, owner->id, blocker->owner->id, made? "moving": "no spot");
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

				if (YieldLog())
					LOG("[SupComYield] f=%d unit %d makes way to (%.0f, %.0f): %d ahead, %d side, heading dot %.2f", gs->frameNum, owner->id, spot.x, spot.z, a, b, ((spot - pos) * XZVector).SafeNormalize2D().dot(flatFrontDir));
				StartMoving(spot, SQUARE_SIZE * 0.5f);
				scYielding = true;
				return true;
			}
		}
	}

	return false;
}
