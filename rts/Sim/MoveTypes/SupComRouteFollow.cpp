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
#include "System/Threading/ThreadPool.h"

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

	// a change of position smaller than this is no change (elmos; DESIGN): a mover pressed against an idle unit or a wall is
	// rocked back and forth by the collision push by more than 0.1 elmo, which kept resetting its stuck clock (L11101 route log:
	// PTH-002-C/D movers blocked for the whole case, no stuck ending); a quarter square still sees any unit making headway
	constexpr float STILL_EPS = SQUARE_SIZE * 0.25f;
}

// a stuck unit cut off by idle units looks for a way round them on the square grid before it stops following (DESIGN; switch
// SupComRouteDetour, default 1), this many squares each way (DESIGN: about two group widths of T1 units)
static bool RouteDetourOn() { static const bool on = SupComSimSwitch::Get("SupComRouteDetour", true); return on; }
static constexpr int DETOUR_HALF = 24;

// route points are cell centres (PTH-007 implementation freedom, route representation; the reference route is a list of cells and a
// unit aims at a cell's centre): the engine path finder gives a square by its corner, half a square off the line of a unit standing
// on a square centre, so the first aim (R2: the next route point, no straight-line condition) turned the unit off its line and back
// (L11857 MOV-003-A: braking from 18 map units, speed rising again after it). Switch SupComRouteCellCentres, default 1.
// a re-read of the engine path (it refines a route under the same id) replaces only the points that came from it; the old route
// spliced on after a replacement route is kept, and an unchanged path is not re-read (L11858 route log: after a rejoining replacement
// every aim choice read back only the replacement's points, ending at the old next point, and the order ended there as "end of a route
// outside the goal area": PTH-004-A/B, PTH-003-C/F, MOV-018-F). A route point the unit has reached (within a square) is not a usable
// aim while later points remain (R4: then a new route is requested; L11858 PTH-005-A circled its reached aim point for 4000 frames, no
// later point reachable in a straight line). Switch SupComRouteKeepTail, default 1.
static bool RouteKeepTailOn() { static const bool on = SupComSimSwitch::Get("SupComRouteKeepTail", true); return on; }
static bool RouteCellCentresOn() { static const bool on = SupComSimSwitch::Get("SupComRouteCellCentres", true); return on; }
static void RouteCellCentres(std::vector<float3>& pts)
{
	if (!RouteCellCentresOn())
		return;
	for (float3& p: pts) {
		p.x = (math::floor(p.x / SQUARE_SIZE) + 0.5f) * SQUARE_SIZE;
		p.z = (math::floor(p.z / SQUARE_SIZE) + 0.5f) * SQUARE_SIZE;
	}
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

// whether idle units block the straight-line test: only while choosing an aim point for a unit that has not moved for stuck_time
// (R3/R10: such a unit with no point to steer at ends its order). Otherwise idle units are left to the steering round them
// (SupComGoRound), as before the follower: counted always, a mover beside one idle unit searched for routes that ran through it
// and never got round (L11088, PTH-002-C/D/G/I). Set and read only in the single-threaded follower update.
static bool routeUnitsBlock = false;

// The centre-line test for a stuck unit (PTH-007 implementation freedom: how the straight-line test is made; the reference's test
// is by grid cells, fixture SF-PTH-007 unit footprint 1): the squares the unit's centre crosses from a to b are passable for its
// move class, hold no structure and (routeUnitsBlock) no idle unit other than those under the unit at a. The footprint-wide test
// (DoRawSearch) finds no route point from a unit pushed a little off the line of a gap its own width, so a unit queued at a
// one-cell gap stopped following there and plugged it (L11194: PTH-004-A, first ending at f=1947).
static bool RouteCentreOn() { static const bool on = SupComSimSwitch::Get("SupComRouteCentreLine", true); return on; }

static bool CentreLineClear(const CSolidObject* self, const MoveDef* md, const float3& a, const float3& b, bool unitsBlock)
{
	const int ax = int(a.x / SQUARE_SIZE), az = int(a.z / SQUARE_SIZE);
	const CSolidObject* here = groundBlockingObjectMap.GroundBlocked(std::clamp(ax, 0, mapDims.mapxm1), std::clamp(az, 0, mapDims.mapym1));
	const float3 d = (b - a) * XZVector;
	const int steps = std::max(1, int(math::ceil(d.Length2D() / (SQUARE_SIZE * 0.25f))));
	int lx = -1, lz = -1;
	for (int k = 1; k <= steps; k++) {
		const float3 p = a + d * (float(k) / steps);
		const int sx = int(p.x / SQUARE_SIZE), sz = int(p.z / SQUARE_SIZE);
		if (sx == lx && sz == lz)
			continue;
		lx = sx; lz = sz;
		if (sx < 0 || sz < 0 || sx > mapDims.mapxm1 || sz > mapDims.mapym1)
			return false;
		if (CMoveMath::GetPosSpeedMod(*md, sx, sz) <= 0.0f)
			return false;
		const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
		if (o == nullptr || o == self || o == here)
			continue;
		if (o->moveDef == nullptr)
			return false;
		if (unitsBlock && Blocks(o, self))
			return false;
	}
	return true;
}

bool CSupComGroundMoveType::RouteLineClear(const float3& a, const float3& b) const
{
	const MoveDef* md = owner->moveDef;
	if (md == nullptr)
		return true;

	const int thread = ThreadPool::GetThreadNum();

	// terrain and structures: the path finder's own straight-line test (MoveDef::DoRawSearch, as the stock follower's waypoint
	// skip uses), so a route the path finder made is never judged unreachable by a stricter sweep (L11095: a replacement route
	// every frame, none of its points steerable)
	if (!md->DoRawSearch(owner, md, a, b, 0, true, true, false, nullptr, nullptr, nullptr, thread))
		return false;
	if (!routeUnitsBlock)
		return true;

	// idle units (only for a stuck unit, see routeUnitsBlock); those the footprint overlaps at the start do not block
	const CSolidObject* here[16] = {};
	int n = 0;
	{
		int sx0, sz0, sx1, sz1;
		FootprintSquares(md, a, sx0, sz0, sx1, sz1);
		for (int sz = sz0; sz <= sz1; sz++) {
			for (int sx = sx0; sx <= sx1; sx++) {
				const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
				if (o == nullptr || o->moveDef == nullptr || !Blocks(o, owner))
					continue;
				if (std::find(here, here + n, o) == here + n && n < 16)
					here[n++] = o;
			}
		}
	}

	const float3 d = (b - a) * XZVector;
	const int steps = std::max(1, int(math::ceil(d.Length2D() / (SQUARE_SIZE * 0.5f))));

	for (int k = 1; k <= steps; k++) {
		const float3 p = a + d * (float(k) / steps);
		int sx0, sz0, sx1, sz1;
		FootprintSquares(md, p, sx0, sz0, sx1, sz1);
		for (int sz = sz0; sz <= sz1; sz++) {
			for (int sx = sx0; sx <= sx1; sx++) {
				const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
				if (o != nullptr && o->moveDef != nullptr && Blocks(o, owner) && std::find(here, here + n, o) == here + n)
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
	RouteCellCentres(rfPts);
	rfHead = rfEngN = int(rfPts.size());
	if (!rfPts.empty()) rfEngLast = rfPts.back();
	rfPathID = id;
	rfNext = 0;
	rfAimIdx = -1;
	rfFresh = true;

	// the last leg ends at the goal when the route reaches the goal area (the engine's own follower does the same)
	if (!rfPts.empty() && rfPts.back().SqDistance2D(goalPos) <= Square(goalRadius + extraRadius + SQUARE_SIZE))
		rfPts.back() = goalPos;
}

unsigned int CSupComGroundMoveType::RouteSearch(std::vector<float3>& pts, bool fresh, const float3& target)
{
	// a fresh search (the order's first, or one opening a round) goes to the order's goal; a replacement search goes to the old
	// route's next point (PTH-002 v7: a replacement route rejoins when it ends at that point)
	rfRequests++;
	pts.clear();

	const float3 pos = owner->pos;
	const float radius = fresh? (goalRadius + extraRadius): float(SQUARE_SIZE);
	unsigned int id = 0;

	if (rfScriptPos < int(rfScript.size())) {
		// test hook: a scripted result (PTH-002 fixture SF-PTH-002-K, PTH-007 fixture SF-PTH-007): k > 0 points ending at the
		// search's target (the goal, or the old route's next point: it rejoins), -k points sideways that end nowhere on the old route
		const int k = rfScript[rfScriptPos++];
		if (k == 0)
			return 0;

		id = pathManager->RequestPath(owner, owner->moveDef, pos, target, radius, true);
		const float3 dir = ((target - pos) * XZVector).SafeNormalize2D();
		if (k > 0) {
			for (int i = 1; i <= k; i++)
				pts.push_back(pos + (target - pos) * XZVector * (float(i) / k));
		} else {
			const float3 side(-dir.z, 0.0f, dir.x);
			for (int i = 1; i <= -k; i++)
				pts.push_back(pos + side * (SQUARE_SIZE * 2.0f * i));
		}
		return id;
	}

	// answered in the tick it is asked (immediateResult): a synced request is otherwise only queued and has no points yet (perf
	// review F); the follower searches only when no aim point is usable, so these are rare
	id = pathManager->RequestPath(owner, owner->moveDef, pos, target, radius, true, true);
	if (id == 0)
		return 0;

	std::vector<int> starts;
	pathManager->GetPathWayPoints(id, pts, starts);
	RouteCellCentres(pts);

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

void CSupComGroundMoveType::RouteAdopt(unsigned int id, const std::vector<float3>& pts, int head)
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
	rfHead = rfEngN = std::clamp(head, 0, int(pts.size()));
	if (rfHead > 0) rfEngLast = pts[rfHead - 1];
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
	rfEndFrame = gs->frameNum;

	// PTH-002 R10/R11: the order ends silently (no sound: the engine's failure plays none) and leaves the queue; the next queued
	// order starts in this same update (the command AI is polled here, as an arrival does)
	Fail(false);
	if (owner->commandAI != nullptr)
		owner->commandAI->SlowUpdate();
}

void CSupComGroundMoveType::RouteRequest(bool fresh)
{
	// the old route's next point and its index (R6/R10: short routes are judged against it). On a detour (a short route followed,
	// then the old route's points from rfShortFor) the next point is still the old route's one, rfShortFor, until the unit passes it
	const bool hadNext = (rfNext >= 0 && rfNext < int(rfPts.size()));
	int nextIdx = hadNext? rfNext: -1;
	if (hadNext && rfShortSeen) {
		for (int i = rfNext; i < int(rfPts.size()); i++) {
			if (rfPts[i].same(rfShortFor)) {
				nextIdx = i;
				break;
			}
		}
	}
	const float3 nextPt = (nextIdx >= 0)? rfPts[nextIdx]: goalPos;
	const bool asFresh = (fresh || !hadNext);

	std::vector<float3> pts;
	const unsigned int id = RouteSearch(pts, asFresh, asFresh? goalPos: nextPt);
	const bool empty = pts.empty();

	// PTH-002 v7: a replacement route rejoins when it ends at the old route's next point
	const bool rejoins = (!empty && !asFresh && pts.back().SqDistance2D(nextPt) <= Square(SQUARE_SIZE));
	const bool isShort = (!empty && !asFresh && !rejoins && int(pts.size()) < ShortRoute());

	if (RouteLog())
		LOG("[SupComRoute] f=%d u=%d %s search: %s, %d points, rejoins %d, short %d (waits %d, rounds %d)", gs->frameNum, owner->id, asFresh? "fresh": "replacement", empty? "no route": "route", int(pts.size()), int(rejoins), int(isShort), rfWaits, rfRounds);

	if (asFresh) {
		// R5/R7, PTH-002 R9/R10: an empty fresh search ends the order at once; a route opens the round
		if (empty) {
			RouteEnd("fresh search empty");
			return;
		}
		rfWaits = 0;
		rfShortSeen = false;
		RouteAdopt(id, pts, int(pts.size()));
		return;
	}

	// a replacement route is followed, then the old route on from its next point (after it, when the route rejoins there)
	const auto Spliced = [&](int from) {
		std::vector<float3> r = pts;
		for (int i = from; i < int(rfPts.size()); i++)
			r.push_back(rfPts[i]);
		return r;
	};

	const bool nextFree = !RouteCellOccupied(nextPt);

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
		RouteAdopt(id, Spliced(nextIdx), int(pts.size()));
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
		RouteAdopt(id, Spliced(nextIdx + 1), int(pts.size()));
		return;
	}
	RouteAdopt(id, Spliced(nextIdx), int(pts.size()));
}

bool CSupComGroundMoveType::RouteChooseAim(bool stuck)
{
	routeUnitsBlock = stuck;
	struct Restore { ~Restore() { routeUnitsBlock = false; } } restore;

	if (rfPathID == pathID && pathID != 0) {
		// the engine refines a route under the same id: read it again, keep the place by the nearest point to the old next point
		const float3 oldNext = (rfNext < int(rfPts.size()))? rfPts[rfNext]: owner->pos;
		std::vector<float3> pts;
		std::vector<int> starts;
		pathManager->GetPathWayPoints(pathID, pts, starts);
		RouteCellCentres(pts);
		const bool same = RouteKeepTailOn() && int(pts.size()) == rfEngN && !pts.empty() && pts.back().same(rfEngLast);
		if (!pts.empty() && !(rfScriptPos > 0) && !same) {
			if (RouteKeepTailOn()) {
				rfEngN = int(pts.size());
				rfEngLast = pts.back();
				const int head = int(pts.size());
				for (int i = std::clamp(rfHead, 0, int(rfPts.size())); i < int(rfPts.size()); i++)
					pts.push_back(rfPts[i]);
				rfHead = head;
			}
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
		// R2: right after a new route the unit aims at its next point (no straight-line condition: the route's own first leg; a
		// point on the unit itself is passed over)
		aim = rfNext;
		while (aim + 1 < n && rfPts[aim].SqDistance2D(pos) <= Square(SQUARE_SIZE))
			aim++;
	}
	if (aim < 0) {
		// R2: the farthest of the next look_points points within look_range reachable in a straight line
		const int last = std::min(n - 1, rfNext + LookPoints() - 1);
		for (int k = last; k >= rfNext; k--) {
			if (rfPts[k].SqDistance2D(pos) > Square(LookRange()))
				continue;
			if (RouteKeepTailOn() && k < n - 1 && rfPts[k].SqDistance2D(pos) <= Square(SQUARE_SIZE))
				continue;
			if (RouteLineClear(pos, rfPts[k])) {
				aim = k;
				break;
			}
		}
	}
	// a stuck unit with no point by the footprint-wide test: the farthest of the same points by the centre-line test
	if (aim < 0 && stuck && RouteCentreOn()) {
		const int last = std::min(n - 1, rfNext + LookPoints() - 1);
		for (int k = last; k >= rfNext; k--) {
			if (rfPts[k].SqDistance2D(pos) > Square(LookRange()))
				continue;
			if (CentreLineClear(owner, owner->moveDef, pos, rfPts[k], true)) {
				aim = k;
				if (RouteLog())
					LOG("[SupComRoute] f=%d u=%d stuck: centre line to route point %d (unit %.0f,%.0f)", gs->frameNum, owner->id, k, pos.x, pos.z);
				break;
			}
		}
	}
	rfFresh = false;

	// A stuck unit whose next points are all cut off by idle units (they block only now, see routeUnitsBlock): before it stops
	// following (R10), a local search on the square grid round the idle units, as far as the next route points. The path finder does
	// not see idle units, and the steering round them (SupComGoRound) can jam a mover against a group with a way round it
	// (L11108: PTH-002-G 4 of 6, PTH-004-A 5 of 20, PTH-004-F). If a way round exists the unit gets it as a detour at the front
	// of its route; only with none does it stop. Implementation freedom (PTH-007: how the straight-line test is made).
	if (aim < 0 && stuck && RouteDetourOn()) {
		const MoveDef* md = owner->moveDef;
		const int H = DETOUR_HALF;                                   // squares each way from the unit
		const int W = 2 * H + 1;
		const int cx = int(pos.x / SQUARE_SIZE), cz = int(pos.z / SQUARE_SIZE);
		const int thread = ThreadPool::GetThreadNum();
		const MoveTypes::CheckCollisionQuery query(owner);

		// idle units under the footprint now do not block (the unit can leave them)
		const CSolidObject* here[16] = {};
		int nh = 0;
		{
			int sx0, sz0, sx1, sz1;
			FootprintSquares(md, pos, sx0, sz0, sx1, sz1);
			for (int sz = sz0; sz <= sz1; sz++)
				for (int sx = sx0; sx <= sx1; sx++) {
					const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
					if (o != nullptr && o->moveDef != nullptr && Blocks(o, owner) && std::find(here, here + nh, o) == here + nh && nh < 16)
						here[nh++] = o;
				}
		}
		const auto Centre = [&](int gx, int gz) { return float3((cx - H + gx + 0.5f) * SQUARE_SIZE, pos.y, (cz - H + gz + 0.5f) * SQUARE_SIZE); };
		const auto Open = [&](int gx, int gz) {
			const int mx = cx - H + gx, mz = cz - H + gz;
			if (mx < 0 || mz < 0 || mx > mapDims.mapxm1 || mz > mapDims.mapym1)
				return false;
			const float3 c = Centre(gx, gz);
			if (FootprintBlocked(c))
				return false;
			int sx0, sz0, sx1, sz1;
			FootprintSquares(md, c, sx0, sz0, sx1, sz1);
			if ((CMoveMath::RangeIsBlocked(sx0, sx1, sz0, sz1, &query, thread) & CMoveMath::BLOCK_STRUCTURE) != 0)
				return false;
			for (int sz = sz0; sz <= sz1; sz++)
				for (int sx = sx0; sx <= sx1; sx++) {
					const CSolidObject* o = groundBlockingObjectMap.GroundBlocked(sx, sz);
					if (o != nullptr && o->moveDef != nullptr && Blocks(o, owner) && std::find(here, here + nh, o) == here + nh)
						return false;
				}
			return true;
		};

		// breadth-first, 8 neighbours, in a fixed order (deterministic on every client)
		std::vector<int> from(W * W, -2);
		std::vector<int> queue;
		queue.reserve(W * W);
		const int start = H * W + H;
		from[start] = -1;
		queue.push_back(start);
		static constexpr int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
		static constexpr int DZ[8] = {0, 0, 1, -1, 1, -1, 1, -1};
		for (size_t qi = 0; qi < queue.size(); qi++) {
			const int c = queue[qi], gx = c % W, gz = c / W;
			for (int d = 0; d < 8; d++) {
				const int nx = gx + DX[d], nz = gz + DZ[d];
				if (nx < 0 || nz < 0 || nx >= W || nz >= W)
					continue;
				const int ni = nz * W + nx;
				if (from[ni] != -2 || !Open(nx, nz))
					continue;
				// a diagonal step needs both side squares open (no corner cutting)
				if (d >= 4 && (!Open(nx, gz) || !Open(gx, nz)))
					continue;
				from[ni] = c;
				queue.push_back(ni);
			}
		}

		// the farthest of the next route points reached; with none reached (they lie in or past the idle group, or outside the
		// search square) the reached square nearest the farthest of them, if it is at least two squares nearer than the unit
		// (DESIGN: the route itself then goes on from there)
		const int last = std::min(n - 1, rfNext + LookPoints() - 1);
		int tgt = -1, tgtK = -1;
		for (int k = last; k >= rfNext && tgt < 0; k--) {
			const int gx = int(rfPts[k].x / SQUARE_SIZE) - (cx - H), gz = int(rfPts[k].z / SQUARE_SIZE) - (cz - H);
			if (gx < 0 || gz < 0 || gx >= W || gz >= W || from[gz * W + gx] == -2 || gz * W + gx == start)
				continue;
			tgt = gz * W + gx;
			tgtK = k;
		}
		int reached = 0;
		if (tgt < 0) {
			const float3& far = rfPts[last];
			float best = pos.distance2D(far) - 2.0f * SQUARE_SIZE;
			for (int c = 0; c < W * W; c++) {
				if (from[c] == -2 || c == start)
					continue;
				reached++;
				const float d = Centre(c % W, c / W).distance2D(far);
				if (d < best) { best = d; tgt = c; }
			}
		}
		if (tgt >= 0) {
			// the path back to the unit, then pulled taut: each kept point is the farthest still reached in a unit-free straight line
			std::vector<float3> path;
			for (int c = tgt; c != -1 && c != start; c = from[c])
				path.push_back(Centre(c % W, c / W));
			std::reverse(path.begin(), path.end());
			std::vector<float3> pulled;
			float3 at = pos;
			size_t i = 0;
			while (i < path.size()) {
				size_t far = i;
				for (size_t j = path.size(); j-- > i; ) {
					if (RouteLineClear(at, path[j])) { far = j; break; }
				}
				pulled.push_back(path[far]);
				at = path[far];
				i = far + 1;
			}
			if (!pulled.empty()) {
				rfPts.insert(rfPts.begin() + rfNext, pulled.begin(), pulled.end());
				if (rfNext < rfHead) rfHead += int(pulled.size());
				aim = rfNext;                                          // the first leg of the detour
				if (RouteLog())
					LOG("[SupComRoute] f=%d u=%d stuck: detour round idle units, %d points to %s %d (unit %.0f,%.0f)", gs->frameNum, owner->id, int(pulled.size()), (tgtK >= 0? "route point": "nearest square towards route point"), (tgtK >= 0? tgtK: last), pos.x, pos.z);
			}
		} else if (RouteLog()) {
			LOG("[SupComRoute] f=%d u=%d stuck: no detour (%d squares reached; route points %d..%d, next at %.0f,%.0f, last at %.0f,%.0f; unit %.0f,%.0f)", gs->frameNum, owner->id, reached, rfNext, last, rfPts[rfNext].x, rfPts[rfNext].z, rfPts[last].x, rfPts[last].z, pos.x, pos.z);
		}
	}

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

	// while the follower holds a route, the engine's own end-of-path failure (stock SetNextWayPoint / lastWaypoint: its path may end at
	// the old route's next point) does not end the order: the follower's rules do
	if (rfActive && !rfPts.empty() && pathingFailed)
		pathingFailed = false;

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
			// ended (and possibly the next order started, which resets the follower): nothing more for this order this frame
			if (!rfActive || progressState != Active)
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
		if (!rfActive || progressState != Active || pathID == 0)
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
		if (RouteLog())
			LOG("[SupComRoute] f=%d u=%d recheck: aim %d (%.0f,%.0f) %s (unit %.0f,%.0f)", frame, owner->id, rfAimIdx, rfAim.x, rfAim.z, retarget? "blocked": "clear", pos.x, pos.z);
	}
	if (!retarget && stuck)
		retarget = true;
	if (!retarget && RouteKeepTailOn() && rfAimIdx < n - 1 && rfAim.SqDistance2D(pos) <= Square(SQUARE_SIZE))
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
