/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: move tracking (ord_orders.lua TrackMoves and its helpers; REQ PTH-003 v2 R1, R7, PTH-004 v3 R3, R5):
// stale and give-up timing, the reachability fill's schedule and budget, the redirect of an unreachable goal, and the
// footprint probes. Pure functions; the adapter asks the engine (positions, fills, move tests). No engine include.
//
// Time thresholds stay seconds x steps per second in float, compared with whole step differences as the Lua compares
// frames; only periods are whole step counts (Rate::Every). Speeds from the engine are elmos per step.

#ifndef KT1_CORE_ORDERS_MOVE_TRACKING_H
#define KT1_CORE_ORDERS_MOVE_TRACKING_H

#include <cstdint>

#include "OrdersState.h"

namespace KT1::Core::Orders {

struct OrdersConfig;

struct TrackParams {
	Step period = 1;                 // steps between passes
	Step refill = 1;                 // steps between refills
	float giveUp = 0;                // the rest in steps, as float
	float aheadRecent = 0;
	float stuckStill = 0;
	float stuckNoGain = 0;
	float stopStill = 0;
	float trustStill = 0;
	float retryWindow = 0;
	float refillMax = 0;
	float arrive = 0;                // the arrival radius, elmos
	float endSlack = 0;              // arrival radius plus the slack, elmos
	float progressMin = 0;           // elmos
	float gain = 0;                  // elmos
	float movingSpeed = 0;           // elmos per step
	float ahead = 0;
	float aheadRadius = 0;
	float redirectNear = 0;
	float redirectSmallFill = 0;     // squares
	float tieTolerance = 0;
	float beyond = 0;                // BeyondElmo
	float refillNear = 0;            // elmos
	float refillMoved = 0;           // elmos
	int32_t fillSquares = 0;
	int32_t fillBudget = 0;          // 0: none
	int32_t tiePoints = 0;
	int32_t retryMax = 0;
	bool schedule = true;
	bool skipFarOrderFill = true;
	bool skipFarRefill = true;
};
// Rate must be set (the host sets it before any system starts)
TrackParams MakeTrackParams(const OrdersConfig& config);

// a pass runs every period, and on the step after a ground mover was given a move
bool TrackingDue(Step now, bool anyFresh, const TrackParams& p);

// the move's life
MoveTrack BeginTrack(uint32_t tag, float gx, float gz, Step now, float sx, float sz);
MoveTrack RedirectedTrack(uint32_t tag, float gx, float gz, float aimX, float aimZ, Step now, Step start, int32_t tries);
void NoteGain(MoveTrack& m, float dist, Step now, const TrackParams& p);
void NoteSpeed(MoveTrack& m, float speed, Step now, const TrackParams& p);
void ForcedStop(MoveTrack& m, float x, float z, Step now);   // stunned, carried or under construction: the clocks wait
// keeps the last position of progress; true when the unit has not moved progressMin for the give-up time (the caller
// then asks whether anything ahead of it is moving)
bool WaitedByPosition(MoveTrack& m, float x, float z, Step now, const TrackParams& p);
bool IsStuck(const MoveTrack& m, float dist, Step now, const TrackParams& p);   // standing without gaining, not owed a fill
bool EndsAtRedirectStop(const MoveTrack& m, float dist, Step now, const TrackParams& p);
bool ShortOfGoal(float x, float z, float gx, float gz, const TrackParams& p);   // farther than arrival radius plus slack
// the trace columns stale_due and giveup_due: the first step the timer can end the move; NO_STEP while it cannot
Step StaleDue(const MoveTrack& m, const TrackParams& p);
Step GiveUpDue(const MoveTrack& m, const TrackParams& p);

// a friend ahead (PTH-002 R2): false when the goal is nearer than 1 elmo, else the centre of the look ahead
bool AheadPoint(float x, float z, float gx, float gz, const TrackParams& p, float& px, float& pz);
// that friend is "making progress": it moves now, or its own tracked move moved recently
bool CountsAsProgressing(float speed, const MoveTrack* other, Step now, const TrackParams& p);

// the fill's schedule
float BeyondElmo(const OrdersConfig& config);
bool OrderFillBeyond(float x, float z, float gx, float gz, float beyondElmo);   // Chebyshev distance over it
Step NextRefill(Step now, UnitId unit, Step period);        // within a period, on the unit's own phase
bool FillWanted(const MoveTrack& m, Step now, bool outsideStructures, const TrackParams& p);
inline bool IsOrderFill(const MoveTrack& m) { return m.nextReach == NO_STEP && m.reachAt == NO_STEP; }
// no fill for a goal beyond the flood: recorded as the undecided verdict it would give (order and refill)
void RecordSkippedFill(MoveTrack& m, UnitId unit, Step now, float x, float z, const TrackParams& p);
bool RefillDue(const MoveTrack& m, Step now, float x, float z, const ChangeGrid& changes, bool terrainDeforms,
               const TrackParams& p);
void PostponeRefill(MoveTrack& m, UnitId unit, Step now, const TrackParams& p);
bool MakeUpFillWanted(const MoveTrack& m, Step now, bool outsideStructures, const TrackParams& p);
bool BudgetLeft(const State& state, const TrackParams& p);
void NoteFill(MoveTrack& m, UnitId unit, Step now, float x, float z, Reach verdict, const TrackParams& p);

// the redirect
struct RedirectQuery {
	bool naval = false;
	Reach verdict = Reach::Undecided;
	bool hasNearest = false;
	float nx = 0, nz = 0;            // the fill's nearest reachable point
	float gx = 0, gz = 0;
	bool hasVisited = false;
	float visited = 0;               // squares the fill visited
	float stillSteps = 0;            // standing this long; 0 while queued behind a friend
};
// IfBoxed: yes only when the unit is boxed in where it stands (the caller probes only then)
enum class Redirect : uint8_t { No, Yes, IfBoxed };
Redirect RedirectUnreachable(const RedirectQuery& q, const TrackParams& p);
// the fill's nearest point is where the unit already stands while the goal is far: unused unless the unit is boxed in
bool StaleIfNotBoxed(float ux, float uz, bool hasNearest, float nx, float nz, float gx, float gz, float endSlack);
bool TieNear(float px, float pz, float gx, float gz, float r, float tolerance);
// a move gone at once with the unit where it was ordered and the goal not near
bool EngineDropped(float ageSteps, float ux, float uz, bool hasStart, float sx, float sz, float gx, float gz,
                   const TrackParams& p);
void TiePoint(int32_t k, float gx, float gz, float r, const TrackParams& p, float& px, float& pz);
// the redirect's move point: past the stop point by the arrival radius, unless the engine ends a move at its goal
void RedirectAim(float x, float z, float nx, float nz, bool exactGoal, const TrackParams& p, float& tx, float& tz);
// a redirected move gone at once while the unit has not come near its point: give it again
bool RetryDroppedRedirect(const MoveTrack& m, bool away, bool noCommand, Step now, const TrackParams& p);

// footprint probes (PTH-004 v3 R5)
void BoxProbe(int32_t k, int32_t directions, float x, float z, float radius, float& px, float& pz);
// |px - vx| < hx + vhx and the same on z
bool FootprintsOverlap(float px, float pz, float hx, float hz, float vx, float vz, float vhx, float vhz);
bool InYard(float x, float z, float vx, float vz, float vhx, float vhz);       // within the longer half side, either facing
bool InFootprint(float x, float z, float vx, float vz, float vhx, float vhz);

} // namespace KT1::Core::Orders

#endif
