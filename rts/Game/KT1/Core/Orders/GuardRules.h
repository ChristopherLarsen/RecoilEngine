/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: what a patrol, a guard and a guarding builder do at their turn (ord_orders.lua CheckPatrol,
// CheckGuard, CheckAssist, GuardShield and the engineering flow's walk to a free spot; REQ ORD-004 v1, ORD-005 v2,
// ECO-006 v1 R3). Pure decisions; the adapter reads the units and gives the orders. No engine include.

#ifndef KT1_CORE_ORDERS_GUARD_RULES_H
#define KT1_CORE_ORDERS_GUARD_RULES_H

#include <array>
#include <cstdint>

#include "OrdersState.h"

namespace KT1::Core::Orders {

struct OrdersConfig;

// The nearest of the units offered. The Lua keeps two tie rules, both kept here.
struct Nearest {
	UnitId id = NO_UNIT;
	float distance = 0;
	// equal distance: the lower id (patrol and guard sight of an enemy by army, the attack-move builder step)
	void OfferByLowestId(UnitId unit, float d);
	// equal distance: the one offered first, so offer in the engine's cylinder order (patrol repair, guard engage)
	void OfferFirst(UnitId unit, float d);
};

// a unit at or below the threshold share of its health, and not full (ORD-004 v1 R3)
bool NeedsRepair(float health, float maxHealth, float threshold, float epsilon);

struct GuardParams {
	Step check = 1;                  // steps between checks
	float half = 0;                  // half the guard radius, elmos
	float leadSteps = 0;             // follow the guarded unit's place this many steps on
	float behind = 0;                // elmos short of that point
	float movingSpeed = 0;           // elmos per step
	float ringSettle = 0;            // elmos
	float ringPush = 0;
	float ringOnWay = 0;
};
GuardParams MakeGuardParams(const OrdersConfig& config);

// R5 and the group guard's ring place (FRM-005; the ring itself is family GLUE's)
struct FollowFacts {
	float gx = 0, gy = 0, gz = 0;    // the guarded unit
	float vx = 0, vz = 0;            // its velocity, elmos per step
	float ux = 0, uz = 0;            // the guard
	bool repairs = false;            // the guard is a builder that repairs
	bool frontIsMove = false;        // the guard's front order is a move
	float frontX = 0, frontZ = 0;    // that move's point
	bool hasRing = false;            // a ring place beside this guarded unit
	float ringDx = 0, ringDz = 0;
	bool ringSettled = false;
};
struct FollowPlan {
	bool removeOwnFront = false;     // take back the family's own step in front first
	bool move = false;               // insert a follow move to (x, y, z)
	float x = 0, y = 0, z = 0;
	bool ringSettled = false;        // the ring place's new settled flag (write it back when hasRing)
};
FollowPlan PlanFollow(const FollowFacts& facts, const GuardParams& p);

// R3: the assist step a guarding builder should hold. leaveAlone: its own factory (the engine's guard and the economy
// assist it): nothing is changed. Neither flag: take back any assist step and walk.
struct AssistWant {
	bool leaveAlone = false;
	bool wanted = false;
	CommandKind kind = CommandKind::Other;   // Repair, Reclaim or Guard
	int32_t param = NO_UNIT;                 // the order's parameter (a wreck: feature id plus maxUnits)
};
AssistWant WantedAssist(const Task& task, UnitId unit, UnitId guarded, const DefFacts& def, int32_t maxUnits);

// the guard tops up the guarded unit's shield: full health, finished, and the shield has room
bool WantsShield(float health, float maxHealth, bool beingBuilt, bool shieldHasRoom);

// the walk of a builder held out of reach
struct ReachParams {
	Step every = 1;
	Step walkingEvery = 1;
	float stall = 0;                 // steps
	float walkSlack = 0;             // steps
	float walkTimeFactor = 0;
	float progress = 0;              // elmos, as are the rest
	float inside = 0;
	float minOut = 0;
	float clear = 0;
	float tried = 0;
	float out = 0;
	float arcStep = 0;               // radians
	float doneInside = 0;
	float featureRadius = 0;
	float unitRadius = 0;
	float stepsPerSecond = 0;
	int32_t spots = 0;
	int32_t tries = 0;
};
ReachParams MakeReachParams(const OrdersConfig& config);

bool ReachCheckDue(UnitId unit, Step now, bool walking, const ReachParams& p);   // spread over units by id
// keeps the stall record of this target; true: look for a free spot now
bool NoteReach(ReachStall& stall, CommandKind kind, float target, float dist, float reach, Step now, const ReachParams& p);
float SpotRadius(float targetRadius, float reach, const ReachParams& p);
void SpotPoint(int32_t i, float tx, float tz, float r, const ReachParams& p, float& sx, float& sz);
bool SpotTried(const ReachStall& stall, float sx, float sz, const ReachParams& p);
void NoteSpotSearch(ReachStall& stall, float dist, Step now);                     // counts the try, restarts the clock
void NoteSpot(ReachStall& stall, float sx, float sz);

// the way there goes round outside the ring of builders: back out, along the circle, then in to the spot
struct Walk {
	int32_t count = 0;
	std::array<std::array<float, 2>, WALK_ORDERS> points{};   // in driving order, the spot last
	float length = 0;
};
Walk PlanWalk(float ux, float uz, float tx, float tz, float spotX, float spotZ, float dist, float reach,
              const ReachParams& p);
void BeginWalk(ReachStall& stall, float length, float speed, Step now, const ReachParams& p);   // speed: elmos per second
// in reach of the work behind the walk
bool WalkArrived(float ux, float uz, float tx, float tz, float reach, const ReachParams& p);
bool WalkExpired(const ReachStall& stall, Step now);
void EndWalk(ReachStall& stall, Step now);                                        // the next check may pick another spot

} // namespace KT1::Core::Orders

#endif
