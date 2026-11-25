/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// SupComRouteFollow: route following and the end of a blocked move order for SupComGroundMoveType units.
//   PTH-007 v2 R1-R7 (route following: passed points, the aim point, when it is chosen again, no timed re-planning)
//   PTH-002 v7 R9-R13 (retries, rounds, short routes, immediate endings, the order consumed in the same update)
// Switch SupComRouteFollow (sim switch, default on); 0 keeps the engine's own waypoint following, timed re-plans and idle failure.
//
// Values (mod options override; defaults are the released packs, converted from the reference tick of 0.1 s to frames):
//   supcomroute_look_points    10     PARITY:G0-PATHFOL path.look_points
//   supcomroute_look_range     9999   PARITY:G0-PATHFOL path.look_range (map units; a map unit is one square here)
//   supcomroute_retarget       0.5    PARITY:G0-PATHFOL path.retarget_fraction
//   supcomroute_recheck        7      PARITY:G0-PATHFOL path.recheck_ticks (reference ticks)
//   supcomroute_retry_wait     10     PARITY:G0-PTHBLK block.retry_wait (reference ticks)
//   supcomroute_retries        3      PARITY:G0-PTHBLK block.retries
//   supcomroute_rounds         3      PARITY:G0-PTHBLK block.rounds
//   supcomroute_stuck_time     30     PARITY:G0-PTHBLK block.stuck_time (reference ticks)
//   supcomroute_short_route    3      PARITY:G0-PTHBLK block.short_route (route points)
// Not covered: path.recheck_ticks_follow (13) for a unit following a group leader: this move type has no leader following.
//
// Interpretation (implementation freedom of PTH-007: route representation and the straight-line test):
// - a route search is the engine's own (HAPFS, answered in the tick it is asked) from the unit to the order's goal; "no route"
//   is no path, or a path none of whose points is more than a square from the unit
// - a replacement route rejoins when one of its points lies within a square of a remaining point of the old route
// - the straight-line test sweeps the move def footprint along the line in half squares; it is blocked by impassable ground,
//   structures and units that are not moving (an idle unit is never displaced, PTH-002 R3), ignoring objects the footprint
//   already overlaps at the start

#include "SupComGroundMoveType.h"

#include "Game/GameSetup.h"
#include "Map/ReadMap.h"
#include "MoveDefHandler.h"
#include "MoveMath/MoveMath.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/GroundBlockingObjectMap.h"
#include "Sim/Path/IPathManager.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/StringUtil.h"
#include "System/SupComSimSwitch.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

CONFIG(bool, SupComRouteLog).defaultValue(false).description("supcom: log the route follower's searches, waits and endings (diagnosis; no effect on the game).");

namespace {
	float Opt(const char* name, float def)
	{
		const auto& mo = CGameSetup::GetModOptions();
		const auto it = mo.find(StringToLower(std::string(name)));
		if (it == mo.end())
			return def;
		char* end = nullptr;
		const float v = std::strtof(it->second.c_str(), &end);
		return (end != it->second.c_str() && std::isfinite(v) && v >= 0.0f)? v: def;
	}

	// reference ticks to frames (reference tick 0.1 s, PARITY:G0-MOVE ref_tick)
	int Ticks(float t) { return std::max(1, int(t * GAME_SPEED * 0.1f + 0.5f)); }

	int LookPoints() { static const int v = std::max(1, int(Opt("supcomroute_look_points", 10.0f))); return v; }                 // PARITY:G0-PATHFOL
	float LookRange() { static const float v = Opt("supcomroute_look_range", 9999.0f) * SQUARE_SIZE; return v; }                // PARITY:G0-PATHFOL
	float Retarget() { static const float v = Opt("supcomroute_retarget", 0.5f); return v; }                                    // PARITY:G0-PATHFOL
	int RecheckFrames() { static const int v = Ticks(Opt("supcomroute_recheck", 7.0f)); return v; }                             // PARITY:G0-PATHFOL
	int WaitFrames() { static const int v = Ticks(Opt("supcomroute_retry_wait", 10.0f)); return v; }                            // PARITY:G0-PTHBLK
	int Retries() { static const int v = int(Opt("supcomroute_retries", 3.0f)); return v; }                                     // PARITY:G0-PTHBLK
	int Rounds() { static const int v = std::max(1, int(Opt("supcomroute_rounds", 3.0f))); return v; }                          // PARITY:G0-PTHBLK
	int StuckFrames() { static const int v = Ticks(Opt("supcomroute_stuck_time", 30.0f)); return v; }                           // PARITY:G0-PTHBLK
	int ShortRoute() { static const int v = int(Opt("supcomroute_short_route", 3.0f)); return v; }                              // PARITY:G0-PTHBLK

	bool RouteLog() { static const bool on = configHandler->GetBool("SupComRouteLog"); return on; }

	// the engine's waypoint search radius (GroundMoveType.cpp WAYPOINT_RADIUS)
	constexpr float ROUTE_WP_RADIUS = 1.25f * SQUARE_SIZE;

	// a change of position smaller than this is no change (elmos; float noise and collision jitter; DESIGN)
	constexpr float STILL_EPS = 0.1f;
}

bool CSupComGroundMoveType::RouteFollowOn()
{
	static const bool on = SupComSimSwitch::Get("SupComRouteFollow", true);
	return on;
}

void CSupComGroundMoveType::RouteReset()
{
	rfActive = false;
	rfPts.clear();
	rfPathID = 0;
	rfNext = 0;
	rfAimIdx = -1;
	rfAimDist = 0.0f;
	rfWaitUntil = -1;
	rfWaits = 0;
	rfRounds = 0;
	rfRequests = 0;
	rfFresh = false;
	rfShortSeen = false;
	rfLastWaited = false;
	rfWaitShort = false;
	// the test script is kept: a case sets it before giving the order
}

void CSupComGroundMoveType::SlowUpdate()
{
	if (!RouteFollowOn() || useRawMovement || owner->GetTransporter() != nullptr) {
		CGroundMoveType::SlowUpdate();
		return;
	}

	// PTH-007 v2 R4: no new route on a timer and no failure for standing still: the follower (RouteFollowUpdate) asks for a route
	// only when no usable aim point is left, and ends a blocked order by PTH-002 R9 and R10
	wantRepath = false;
	numIdlingUpdates = 0;
	numIdlingSlowUpdates = 0;

	if (!owner->IsFlying() && !owner->pos.IsInBounds())
		owner->Move(oldPos = owner->pos.cClampInBounds(), false);

	AMoveType::SlowUpdate();
}

static void FootprintSquares(const MoveDef* md, const float3& p, int& sx0, int& sz0, int& sx1, int& sz1)
{
	sx0 = std::clamp(int(p.x / SQUARE_SIZE) - md->xsizeh, 0, mapDims.mapxm1);
	sz0 = std::clamp(int(p.z / SQUARE_SIZE) - md->zsizeh, 0, mapDims.mapym1);
	sx1 = std::clamp(int(p.x / SQUARE_SIZE) + md->xsizeh, 0, mapDims.mapxm1);
	sz1 = std::clamp(int(p.z / SQUARE_SIZE) + md->zsizeh, 0, mapDims.mapym1);
}

static bool Blocks(const CSolidObject* o, const CSolidObject* self)
{
	if (o == nullptr || o == self)
		return false;
	if (o->immobile || o->moveDef == nullptr)
		return true;
	// a mobile unit blocks while idle (PTH-002 R3: never displaced); a unit with a move under way (driving, or held in a queue) is
	// traffic, left to PTH-002 R2 and the collisions
	const CUnit* u = static_cast<const CUnit*>(o);
	return (u->moveType == nullptr || (u->moveType->progressState != AMoveType::Active && !o->IsMoving()));
}

bool CSupComGroundMoveType::RouteCellOccupied(const float3& p) const
{
	const MoveDef* md = owner->moveDef;
	if (md == nullptr)
		return false;

	int sx0, sz0, sx1, sz1;
	FootprintSquares(md, p, sx0, sz0, sx1, sz1);

	for (int sz = sz0; sz <= sz1; sz++) {
		for (int sx = sx0; sx <= sx1; sx++) {
			if (Blocks(groundBlockingObjectMap.GroundBlocked(sx, sz), owner))
				return true;
		}
	}
	return false;
}

bool CSupComGroundMoveType::RouteLineClear(const float3& a, const float3& b) const
{
	const MoveDef* md = owner->moveDef;
	if (md == nullptr)
		return true;

	// objects the footprint overlaps at the start do not block (a unit in a crowd or touching a wall can still leave)
	const CSolidObject* here[16] = {};
	int n = 0;
	{
		int sx0, sz0, sx1, sz1;
		FootprintSquares(md, a, sx0, sz0, sx1, sz1);
		for (int sz = sz0; sz <= sz1; sz++) {
			for (int sx = sx0; sx <= sx1; sx++) {
				const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
				if (!Blocks(o, owner))
					continue;
				if (std::find(here, here + n, o) == here + n && n < 16)
					here[n++] = o;
			}
		}
	}

	const bool startBlocked = FootprintBlocked(a);
	const float3 d = (b - a) * XZVector;
	const int steps = std::max(1, int(math::ceil(d.Length2D() / (SQUARE_SIZE * 0.5f))));

	for (int k = 1; k <= steps; k++) {
		const float3 p = a + d * (float(k) / steps);

		if (!startBlocked && FootprintBlocked(p))
			return false;

		int sx0, sz0, sx1, sz1;
		FootprintSquares(md, p, sx0, sz0, sx1, sz1);
		for (int sz = sz0; sz <= sz1; sz++) {
			for (int sx = sx0; sx <= sx1; sx++) {
				const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
				if (Blocks(o, owner) && std::find(here, here + n, o) == here + n)
					return false;
			}
		}
	}
	return true;
}

void CSupComGroundMoveType::RouteLoad(unsigned int id)
{
	std::vector<int> starts;
	pathManager->GetPathWayPoints(id, rfPts, starts);
	rfPathID = id;
	rfNext = 0;
	rfAimIdx = -1;
	rfFresh = true;

	// the last leg ends at the goal when the route reaches the goal area (the engine's own follower does the same)
	if (!rfPts.empty() && rfPts.back().SqDistance2D(goalPos) <= Square(goalRadius + extraRadius + SQUARE_SIZE))
		rfPts.back() = goalPos;
}

unsigned int CSupComGroundMoveType::RouteSearch(std::vector<float3>& pts)
{
	rfRequests++;
	pts.clear();

	const float3 pos = owner->pos;
	unsigned int id = 0;

	if (rfScriptPos < int(rfScript.size())) {
		// test hook: a scripted result (PTH-002 fixture SF-PTH-002-K, PTH-007 fixture SF-PTH-007)
		const int k = rfScript[rfScriptPos++];
		if (k == 0)
			return 0;

		id = pathManager->RequestPath(owner, owner->moveDef, pos, goalPos, goalRadius + extraRadius, true);
		const float3 dir = ((goalPos - pos) * XZVector).SafeNormalize2D();
		if (k > 0) {
			for (int i = 1; i <= k; i++)
				pts.push_back(pos + (goalPos - pos) * XZVector * (float(i) / k));
		} else {
			// sideways, one square apart: it reaches no point of the old route
			const float3 side(-dir.z, 0.0f, dir.x);
			for (int i = 1; i <= -k; i++)
				pts.push_back(pos + side * (SQUARE_SIZE * 2.0f * i));
		}
		return id;
	}

	// answered in the tick it is asked (immediateResult): a synced request is otherwise only queued and has no points yet (perf
	// review F); the follower searches only when no aim point is usable, so these are rare
	id = pathManager->RequestPath(owner, owner->moveDef, pos, goalPos, goalRadius + extraRadius, true, true);
	if (id == 0)
		return 0;

	std::vector<int> starts;
	pathManager->GetPathWayPoints(id, pts, starts);

	// a route none of whose points takes the unit anywhere is no route
	bool anywhere = false;
	for (const float3& p: pts) {
		if (p.SqDistance2D(pos) > Square(SQUARE_SIZE)) {
			anywhere = true;
			break;
		}
	}
	if (!anywhere) {
		pathManager->DeletePath(id);
		pts.clear();
		return 0;
	}
	return id;
}

void CSupComGroundMoveType::RouteAdopt(unsigned int id, const std::vector<float3>& pts)
{
	if (id != 0 && id != pathID) {
		if (nextPathId != 0) {
			pathManager->DeletePath(nextPathId);
			nextPathId = 0;
		}
		if (pathID != 0)
			pathManager->DeletePath(pathID);

		pathID = id;
		earlyCurrWayPoint = currWayPoint = pathManager->NextWayPoint(owner, pathID, 0, owner->pos, std::max(ROUTE_WP_RADIUS, currentSpeed * 1.05f), true);
		earlyNextWayPoint = nextWayPoint = pathManager->NextWayPoint(owner, pathID, 0, currWayPoint, std::max(ROUTE_WP_RADIUS, currentSpeed * 1.05f), true);
		pathController.SetRealGoalPosition(pathID, goalPos);
		pathController.SetTempGoalPosition(pathID, currWayPoint);
	}

	atEndOfPath = false;
	lastWaypoint = false;
	wantRepath = false;

	rfPts = pts;
	rfPathID = pathID;
	rfNext = 0;
	rfAimIdx = -1;
	rfFresh = true;
	if (!rfPts.empty() && rfPts.back().SqDistance2D(goalPos) <= Square(goalRadius + extraRadius + SQUARE_SIZE))
		rfPts.back() = goalPos;
}

void CSupComGroundMoveType::RouteEnd(const char* why)
{
	if (RouteLog())
		LOG("[SupComRoute] f=%d u=%d order ends (%s): searches %d, waits %d, rounds %d", gs->frameNum, owner->id, why, rfRequests, rfWaits, rfRounds);

	rfActive = false;
	rfWaitUntil = -1;

	// PTH-002 R10/R11: the order ends silently (no sound: the engine's failure plays none) and leaves the queue; the next queued
	// order starts in this same update (the command AI is polled here, as an arrival does)
	Fail(false);
	if (owner->commandAI != nullptr)
		owner->commandAI->SlowUpdate();
}

void CSupComGroundMoveType::RouteRequest(bool fresh)
{
	// the old route's next point (R6/R10: short routes are judged against it). On a detour (a short route followed, its points
	// then the old route's remaining ones from rfShortFor) the next point is still the old route's one, rfShortFor, until the unit
	// has passed it: a second short route while on the detour is for the same point
	const bool hadNext = (rfNext >= 0 && rfNext < int(rfPts.size()));
	float3 nextPt = hadNext? rfPts[rfNext]: goalPos;
	if (hadNext && rfShortSeen) {
		for (int i = rfNext; i < int(rfPts.size()); i++) {
			if (rfPts[i].same(rfShortFor)) {
				nextPt = rfShortFor;
				break;
			}
		}
	}

	std::vector<float3> pts;
	const unsigned int id = RouteSearch(pts);
	const bool empty = pts.empty();

	// rejoins: a point of the new route within a square of a remaining point of the old one
	bool rejoins = false;
	if (!empty && hadNext) {
		for (const float3& p: pts) {
			for (int i = rfNext; i < int(rfPts.size()) && !rejoins; i++)
				rejoins = (p.SqDistance2D(rfPts[i]) <= Square(SQUARE_SIZE));
			if (rejoins)
				break;
		}
	}
	const bool isShort = (!empty && !fresh && !rejoins && int(pts.size()) < ShortRoute());

	if (RouteLog())
		LOG("[SupComRoute] f=%d u=%d %s search: %s, %d points, rejoins %d, short %d (waits %d, rounds %d)", gs->frameNum, owner->id, fresh? "fresh": "replacement", empty? "no route": "route", int(pts.size()), int(rejoins), int(isShort), rfWaits, rfRounds);

	if (fresh) {
		// R5/R7, PTH-002 R9/R10: an empty fresh search ends the order at once; a route opens the round
		if (empty) {
			RouteEnd("fresh search empty");
			return;
		}
		rfWaits = 0;
		rfShortSeen = false;
		RouteAdopt(id, pts);
		return;
	}

	const bool nextFree = hadNext && !RouteCellOccupied(nextPt);

	if (isShort && nextFree) {
		// R6, PTH-002 R10: the first short route for this next point is followed; a second one for the same point ends the order
		if (rfShortSeen && rfShortFor.SqDistance2D(nextPt) <= Square(1.0f)) {
			if (id != 0 && id != pathID)
				pathManager->DeletePath(id);
			RouteEnd("second short route");
			return;
		}
		rfShortSeen = true;
		rfShortFor = nextPt;
		// a short route is a detour to the old route's next point: after it the unit heads for that point again
		std::vector<float3> detour = pts;
		const int oldNext = rfNext;
		const std::vector<float3> oldPts = rfPts;
		for (int i = oldNext; i < int(oldPts.size()); i++)
			detour.push_back(oldPts[i]);
		RouteAdopt(id, detour);
		return;
	}

	if (empty || isShort) {
		if (id != 0 && id != pathID)
			pathManager->DeletePath(id);

		// R5, PTH-002 R9: wait and search again, up to the retries; then the round is used up
		if (rfWaits < Retries()) {
			rfWaits++;
			rfWaitUntil = gs->frameNum + WaitFrames();
			rfWaitShort = isShort;
			return;
		}

		rfRounds++;
		// R6: after a short route at a point that is not free no fresh search follows the last wait
		if (rfRounds >= Rounds() || isShort) {
			RouteEnd(isShort? "short routes, next point not free": "rounds used");
			return;
		}
		RouteRequest(true);
		return;
	}

	// a route: followed; the waits start again only after a route that rejoins
	if (rejoins) {
		rfWaits = 0;
		rfShortSeen = false;
	}
	RouteAdopt(id, pts);
}

bool CSupComGroundMoveType::RouteChooseAim(bool stuck)
{
	if (rfPathID == pathID && pathID != 0) {
		// the engine refines a route under the same id: read it again, keep the place by the nearest point to the old next point
		const float3 oldNext = (rfNext < int(rfPts.size()))? rfPts[rfNext]: owner->pos;
		std::vector<float3> pts;
		std::vector<int> starts;
		pathManager->GetPathWayPoints(pathID, pts, starts);
		if (!pts.empty() && !(rfScriptPos > 0)) {
			int best = 0;
			float bd = std::numeric_limits<float>::max();
			for (int i = 0; i < int(pts.size()); i++) {
				const float dd = pts[i].SqDistance2D(oldNext);
				if (dd < bd) { bd = dd; best = i; }
			}
			rfPts.swap(pts);
			rfNext = best;
			if (rfPts.back().SqDistance2D(goalPos) <= Square(goalRadius + extraRadius + SQUARE_SIZE))
				rfPts.back() = goalPos;
		}
	}

	const int n = int(rfPts.size());
	if (n == 0)
		return false;

	const float3 pos = owner->pos;
	rfNext = std::clamp(rfNext, 0, n - 1);

	// R1: a point is dropped when the following one is at least as near and reachable in a straight line
	while (rfNext + 1 < n && rfPts[rfNext + 1].SqDistance2D(pos) <= rfPts[rfNext].SqDistance2D(pos) && RouteLineClear(pos, rfPts[rfNext + 1]))
		rfNext++;

	int aim = -1;
	if (rfFresh && !stuck) {
		// R2: right after a new route, the next route point
		if (RouteLineClear(pos, rfPts[rfNext]))
			aim = rfNext;
	}
	if (aim < 0) {
		// R2: the farthest of the next look_points points within look_range reachable in a straight line
		const int last = std::min(n - 1, rfNext + LookPoints() - 1);
		for (int k = last; k >= rfNext; k--) {
			if (rfPts[k].SqDistance2D(pos) > Square(LookRange()))
				continue;
			if (RouteLineClear(pos, rfPts[k])) {
				aim = k;
				break;
			}
		}
	}
	rfFresh = false;

	if (aim < 0)
		return false;

	rfAimIdx = aim;
	rfAim = rfPts[aim];
	rfAimDist = rfAim.distance2D(pos);
	return true;
}

void CSupComGroundMoveType::RouteFollowUpdate()
{
	if (!RouteFollowOn() || useRawMovement || owner->GetTransporter() != nullptr) {
		rfActive = false;
		return;
	}
	if (progressState != Active || atGoal) {
		rfActive = false;
		rfWaitUntil = -1;
		return;
	}

	const int frame = gs->frameNum;
	const float3 pos = owner->pos;

	if (!rfActive) {
		// a new order: StartMoving made the first search (an empty one failed the order there: R7, PTH-002 R10/R13)
		rfActive = true;
		rfStillPos = pos;
		rfStillFrame = frame;
		rfRecheckFrame = frame + RecheckFrames() - (owner->id % RecheckFrames());
		rfPathID = 0;
		if (rfScriptPos < int(rfScript.size())) {
			// test hook: the order's first search is the script's (an empty one ends the order before the unit moves: R7, PTH-002 R10)
			RouteRequest(true);
			if (progressState != Active)
				return;
		} else {
			// StartMoving made the first search (an empty one failed the order there)
			if (pathID != 0)
				RouteLoad(pathID);
			rfRequests++;
		}
	}

	if (pathID == 0) {
		// no route held while moving (an engine path lost): a fresh search, as at the start of a round
		if (rfWaitUntil < 0)
			RouteRequest(true);
		if (progressState != Active)
			return;
	}

	// the engine switched to another path of its own (it re-plans a route under a new id only on terrain changes), or the order's
	// first path (StartMoving queues it) has no points yet: read it again each frame until it has; until then the engine's own
	// waypoint steers and nothing is searched (a first search that comes back empty fails the order in the engine: R7)
	if (pathID != 0 && (pathID != rfPathID || rfPts.empty()) && rfScriptPos == 0) {
		RouteLoad(pathID);
		if (rfPts.empty())
			return;
	}

	if (pos.SqDistance2D(rfStillPos) > Square(STILL_EPS)) {
		rfStillPos = pos;
		rfStillFrame = frame;
	}
	const bool stuck = (frame - rfStillFrame > StuckFrames());

	// a pending search after a wait
	if (rfWaitUntil >= 0) {
		if (frame < rfWaitUntil)
			return;
		rfWaitUntil = -1;
		RouteRequest(false);
		return;
	}

	const int n = int(rfPts.size());

	// PTH-003 R1 (R7 of PTH-007): a route that ends outside the goal area is followed to its end, and the order ends there
	if (n > 0 && rfAimIdx == n - 1 && !rfPts[n - 1].same(goalPos) &&
	    pos.SqDistance2D(rfPts[n - 1]) <= Square(std::max(goalRadius + extraRadius, float(SQUARE_SIZE)))) {
		RouteEnd("end of a route outside the goal area");
		return;
	}

	bool retarget = (rfAimIdx < 0);
	if (!retarget && rfAimIdx < n - 1 && rfAim.SqDistance2D(pos) <= Square(Retarget() * rfAimDist))
		retarget = true;
	if (!retarget && frame >= rfRecheckFrame) {
		rfRecheckFrame = frame + RecheckFrames();
		retarget = !RouteLineClear(pos, rfAim);
	}
	if (!retarget && stuck)
		retarget = true;

	if (!retarget)
		return;

	const int prevAim = rfAimIdx;
	if (RouteChooseAim(stuck)) {
		// a stuck unit is checked again only after another stuck_time
		if (stuck)
			rfStillFrame = frame;
		rfLastWaited = false;
		return;
	}

	// R4, PTH-002 R10: stuck beyond stuck_time with no point to steer at: the order ends, with no new search
	if (stuck) {
		RouteEnd("stuck, no aim point");
		return;
	}

	// R4: aiming at the last route point with its cell occupied: first wait retry_wait
	if (!rfLastWaited && prevAim >= 0 && prevAim == n - 1 && RouteCellOccupied(rfPts[n - 1])) {
		rfLastWaited = true;
		rfWaitUntil = frame + WaitFrames();
		rfWaitShort = false;
		rfAimIdx = prevAim;
		return;
	}

	RouteRequest(false);
}
