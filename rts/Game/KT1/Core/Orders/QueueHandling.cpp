/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: queue handling beside the turn (LUA-FREE S12), ported from ord_orders.lua as the Lua has it (port, then
// correct): CommandFallback's waypoint step, UnitCmdDone's cut point, the transport load and set-down clocks
// (AllowUnitTransportLoad/Unload, UnitCommand), the ferry route and the self-destruct countdown. Every number is the Lua
// number (float), in the Lua's order; a Lua `x ^ 2` is `x * x`. No engine include.

#include "QueueHandling.h"

#include <algorithm>
#include <cmath>

#include "OrdersConfig.h"
#include "Game/KT1/Core/Common/DetMath.h"
#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Orders {

namespace {

constexpr float SAME_ELMO = 1e-3f;      // DESIGN (the Lua's 1e-3): a goal-cell record matches the point within this
constexpr float CLOCK_EPS = 1e-6f;      // DESIGN (the Lua's - 1e-6 in every clock compare)
constexpr int32_t HALF_DIV = 2;         // DESIGN: ASK_HALF is half of the engine's slow update

float Sps() { return static_cast<float>(Rate::StepsPerSecond()); }
float Sq(float x) { return x * x; }
float Abs(float x) { return x < 0 ? -x : x; }

// Lua H.Every(s) = math.max(1, math.floor(s * H.FPS + 0.5))
int32_t Every(float seconds) {
	return std::max<int32_t>(1, static_cast<int32_t>(std::floor(seconds * Sps() + DetMath::HALF)));
}

} // namespace

// ---- the waypoint step -----------------------------------------------------------------------------------------------

WaypointParams MakeWaypointParams(const OrdersConfig& c) {
	WaypointParams p;
	const float sps = Sps();
	p.arrive = c.ArrivalElmo();
	p.noProgress = c.waypoint.no_progress_s * sps;          // Lua: WPC_NO_PROGRESS * FPS
	p.progress = c.waypoint.progress_elmo;
	p.nearRadii = c.waypoint.near_radii;
	p.anywhereFactor = c.waypoint.anywhere_factor;
	p.restSpeed = c.waypoint.rest_speed_elmo_s / sps;       // Lua: vx * vx + vz * vz < 1e-4 (0.01 elmo per frame at 30)
	p.restSteps = c.waypoint.rest_s * sps;                  // Lua: w.still > FPS
	p.goalFloorShare = c.waypoint.goal_floor_share;
	p.goalShare = c.waypoint.goal_share;
	p.same = SAME_ELMO;
	p.stepAll = c.waypoint.step_all;
	return p;
}

// Lua: for _, r in ipairs(GG.GoalCellOrdered[u]) do if math.abs(r[1] - x) < 1e-3 and math.abs(r[2] - z) < 1e-3 then ... break
bool FindGoalCell(std::span<const GoalCellRecord> records, float restX, float restZ, float same, GoalCellRecord& out) {
	for (const GoalCellRecord& r : records) {
		if (Abs(r.restX - restX) < same && Abs(r.restZ - restZ) < same) {
			out = r;
			return true;
		}
	}
	return false;
}

// mov_goal_cell.lua: rec[#rec + 1] = {...}; if #rec > 32 then table.remove(rec, 1) end
void AddGoalCell(std::vector<GoalCellRecord>& records, const GoalCellRecord& record, int32_t keep) {
	records.push_back(record);
	if (static_cast<int32_t>(records.size()) > keep) records.erase(records.begin());
}

// Lua gadget:CommandFallback, cmdID == CMD_WAYPOINT (REQ ORD-001 v1 R2, MOV-020)
WaypointAnswer RunWaypoint(WaypointStep& w, const WaypointFacts& f, const WaypointParams& p) {
	WaypointAnswer a;
	const float R = f.radius;
	const float d2 = Sq(f.ux - f.orderedX) + Sq(f.uz - f.orderedZ);
	if (d2 <= R * R) {   // arrived (the Lua keeps the step's record here)
		a.finished = true;
		return a;
	}
	if (!w.active || w.x != f.x || w.z != f.z) {
		w = WaypointStep{};
		w.active = true;
		w.x = f.x;
		w.z = f.z;
		w.since = f.now;
		a.setGoal = true;
		// Lua: (wpAdv[defID] or ORD_STEP_ALL) and R or math.max(R - off, R * 0.4) * 0.8
		a.goalRadius = (f.hasAdvance || p.stepAll) ? R : std::max(R - f.offset, R * p.goalFloorShare) * p.goalShare;
	}
	const float dist = std::sqrt(d2);
	if (!w.hasBest || dist < w.best - p.progress) {
		w.hasBest = true;
		w.best = dist;
		w.bestAt = f.now;
	}
	const float since = static_cast<float>(f.now - w.bestAt);
	if (since >= p.noProgress && (dist <= p.nearRadii * R || since >= p.anywhereFactor * p.noProgress)) {
		w = WaypointStep{};
		a.finished = true;
		return a;
	}
	if (f.vx * f.vx + f.vz * f.vz < p.restSpeed * p.restSpeed) {
		w.still = w.still + 1;
		if (static_cast<float>(w.still) > p.restSteps) {
			w = WaypointStep{};
			a.finished = true;
			return a;
		}
	} else {
		w.still = 0;
	}
	return a;
}

// Lua gadget:UnitCmdDone for a move (after the blocked-route return, which the adapter checks)
bool PlanWaypointStep(const MoveDoneFacts& f, std::span<const GoalCellRecord> records, const WaypointParams& p, WaypointDue& due) {
	if (f.internal || (f.ownMove && !f.moreMoves)) return false;
	const float R = f.radius;
	float ox = f.x, oz = f.z, oy = f.y;
	GoalCellRecord rec;
	const bool found = FindGoalCell(records, f.x, f.z, p.same, rec);
	if (found) { ox = rec.x; oz = rec.z; }
	if (!(Sq(f.ux - ox) + Sq(f.uz - oz) > R * R)) return false;
	due.active = true;
	if (f.toOrdered) {
		if (found) { ox = rec.x; oy = rec.y; oz = rec.z; }
		due.x = ox; due.y = oy; due.z = oz;
	} else {
		due.x = f.x; due.y = f.y; due.z = f.z;
	}
	return true;
}

// ---- transports ------------------------------------------------------------------------------------------------------

TransportParams MakeTransportParams(const OrdersConfig& c) {
	TransportParams p;
	const float sps = Sps();
	p.stepsPerSecond = sps;
	p.askGap = c.transport.ask_gap_s * sps;        // Lua: f > s.last + FPS
	p.dropHold = c.transport.drop_hold_s * sps;
	p.reverseHold = c.transport.reverse_hold_s * sps;
	p.askHalf = Every(c.transport.slow_update_s) / HALF_DIV;   // Lua ASK_HALF = math.floor(H.Every(0.5) / 2)
	p.reverseOrder = c.transport.reverse_order;
	return p;
}

// Lua TransportValue(tDef, key): the code's value, else V.ord's; OV.unload_interval[code]
TransportTimes TimesFor(const OrdersConfig& c, const DefFacts& t) {
	TransportTimes times;
	times.loadS = c.transport.load_s;
	times.unloadS = c.transport.unload_s;
	float accMu = c.transport.drop_accuracy_mu;
	if (t.transportRow >= 0 && static_cast<size_t>(t.transportRow) < c.transport.codes.size()) {
		const TransportCode& code = c.transport.codes[t.transportRow];
		if (code.load_s >= 0) times.loadS = code.load_s;
		if (code.unload_s >= 0) times.unloadS = code.unload_s;
		if (code.drop_accuracy_mu >= 0) accMu = code.drop_accuracy_mu;
		times.intervalS = code.unload_interval_s;
	}
	times.dropAccuracy = accMu * c.elmos_per_mu;
	return times;
}

// Lua gadget:UnitCommand for a load or unload order
void NoteTransportOrder(OrderClock& clock, ClockKind kind, Step now, bool pickup, bool queuedBehind) {
	if (kind == ClockKind::None) return;
	if (kind == ClockKind::Load && !pickup && clock.kind == ClockKind::Load && clock.pickup) return;
	if (queuedBehind) return;
	clock = OrderClock{kind, now, pickup};
}

// Lua OrderStart(t, kind, f)
Step OrderStart(const OrderClock& clock, ClockKind kind, Step now, bool unloadAll) {
	if (!unloadAll) return now;
	if (clock.kind == kind && clock.given != NO_STEP && clock.given <= now) return clock.given;
	return now;
}

namespace {

// Lua: if not s or s.cargo ~= c or f > s.last + gap then s = { cargo = c, start = st, fromOrder = st < f } end; s.last = f
void OpenGate(GateClock& g, UnitId cargo, Step now, Step orderStart, float gap, bool interval, float intervalS) {
	const bool stale = !g.active || (!interval && (g.hasInterval || g.cargo != cargo)) || (interval && !g.hasInterval)
		|| static_cast<float>(now) > static_cast<float>(g.last) + gap;
	if (stale) {
		g = GateClock{};
		g.active = true;
		g.cargo = interval ? NO_UNIT : cargo;
		g.start = orderStart;
		g.fromOrder = orderStart < now;
		g.hasInterval = interval;
		g.intervalS = intervalS;
	}
	g.last = now;
}

// Lua: f - s.start >= seconds * FPS - (half) - 1e-6
bool Elapsed(const GateClock& g, Step now, float seconds, bool useHalf, const TransportParams& p) {
	const float half = useHalf ? static_cast<float>(p.askHalf) : 0;
	return static_cast<float>(now - g.start) >= seconds * p.stepsPerSecond - half - CLOCK_EPS;
}

} // namespace

// Lua gadget:AllowUnitTransportLoad, from the clock on (the adapter checks the docked and pickup-reach refusals first)
bool AskLoad(GateClock& gate, UnitId cargo, Step now, Step orderStart, float loadS, const TransportParams& p) {
	OpenGate(gate, cargo, now, orderStart, p.askGap, false, 0);
	if (!Elapsed(gate, now, loadS, gate.fromOrder, p)) return false;
	gate = GateClock{};
	return true;
}

// Lua gadget:AllowUnitTransportUnload, the set-down clock (after the holds). intervalS < 0: none
bool AskUnload(GateClock& gate, UnitId cargo, Step now, Step orderStart, float unloadS, float intervalS, const TransportParams& p) {
	if (intervalS >= 0) {
		// Lua: f > s.last + (1 + iv) * FPS
		const float gap = (1 + intervalS) * p.stepsPerSecond;
		OpenGate(gate, cargo, now, orderStart, gap, true, intervalS);
		const float wait = gate.setDown > 0 ? intervalS : unloadS;
		if (!Elapsed(gate, now, wait, gate.setDown == 0 && gate.fromOrder, p)) return false;
		gate.setDown = gate.setDown + 1;
		gate.start = now;
		return true;   // the gate stays: the next unit waits the interval
	}
	OpenGate(gate, cargo, now, orderStart, p.askGap, false, 0);
	if (!Elapsed(gate, now, unloadS, gate.fromOrder, p)) return false;
	gate = GateClock{};
	return true;
}

// Lua: if not h or f > h.last + FPS then h = { since = f } end; h.last = f; return f - h.since < hold * FPS
bool Holding(HoldClock& hold, Step now, float holdSteps, const TransportParams& p) {
	if (!hold.active || static_cast<float>(now) > static_cast<float>(hold.last) + p.askGap) {
		hold = HoldClock{true, now, now};
	}
	hold.last = now;
	return static_cast<float>(now - hold.since) < holdSteps;
}

// Lua OffDrop: (x - cmd.params[1]) ^ 2 + (z - cmd.params[3]) ^ 2 > acc * acc
bool OffDropPoint(float x, float z, float orderX, float orderZ, float accuracy) {
	return Sq(x - orderX) + Sq(z - orderZ) > accuracy * accuracy;
}

// ---- the ferry (ORD-007 v2 R4, R5) -----------------------------------------------------------------------------------

FerryParams MakeFerryParams(const OrdersConfig& c) {
	FerryParams p;
	p.period = Every(c.ferry.period_s);                     // Lua: f % H.Every(0.5) == 0
	p.leaveDelay = c.ferry.leave_delay_s * Sps();           // Lua: FERRY_LEAVE_DELAY * FPS
	p.gather = c.ferry.gather_mu * c.elmos_per_mu;
	p.dropRadius = c.ferry.drop_radius_mu * c.elmos_per_mu;
	p.returnDistance = c.ferry.return_mu * c.elmos_per_mu;
	return p;
}

// Lua Waiting(t, fr, mx, mz)
void NoteWaiting(FerryRoute& route, std::span<const UnitId> guarding, Step now) {
	for (const UnitId u : guarding) {
		const auto it = std::lower_bound(route.arrived.begin(), route.arrived.end(), u,
			[](const FerryArrival& a, UnitId id) { return a.unit < id; });
		if (it == route.arrived.end() || it->unit != u) {
			route.arrived.insert(it, FerryArrival{u, now});
			route.lastArrival = now;
		}
	}
	// those no longer guarding leave the list
	route.arrived.erase(std::remove_if(route.arrived.begin(), route.arrived.end(), [&](const FerryArrival& a) {
		return !std::binary_search(guarding.begin(), guarding.end(), a.unit);
	}), route.arrived.end());
	std::vector<FerryArrival> order = route.arrived;
	std::stable_sort(order.begin(), order.end(), [](const FerryArrival& a, const FerryArrival& b) {
		if (a.step != b.step) return a.step < b.step;
		return a.unit < b.unit;
	});
	route.waiting.clear();
	for (const FerryArrival& a : order) route.waiting.push_back(a.unit);
}

// Lua Ferry: if fr.trip then ... end
TripState CheckTrip(const FerryRoute& route, bool queueEmpty, int32_t carried) {
	if (!route.onTrip) return TripState::NotOnTrip;
	if (!queueEmpty) return TripState::Busy;
	if (carried > 0) return TripState::Unload;
	return TripState::Done;
}

// Lua: full or f - (fr.lastArrival or 0) >= FERRY_LEAVE_DELAY * FPS - 1e-6, with cargo aboard
bool LeaveDue(const FerryRoute& route, int32_t carried, bool full, Step now, const FerryParams& p) {
	if (carried <= 0) return false;
	const Step last = (route.lastArrival == NO_STEP) ? 0 : route.lastArrival;
	return full || static_cast<float>(now - last) >= p.leaveDelay - CLOCK_EPS;
}

// Lua Leave: fr.trip = carried, fr.leftAt = f (the note and the set-down order are the adapter's)
void BeginTrip(FerryRoute& route, std::span<const UnitId> carried, Step now) {
	route.trip.assign(carried.begin(), carried.end());
	route.onTrip = true;
	route.leftAt = now;
}

// Lua: (tx - mx) ^ 2 + (tz - mz) ^ 2 > (4 * MU) ^ 2
bool ReturnNeeded(float x, float z, float markerX, float markerZ, const FerryParams& p) {
	return Sq(x - markerX) + Sq(z - markerZ) > p.returnDistance * p.returnDistance;
}

// ---- self-destruct (ORD-010 v6 R4) -----------------------------------------------------------------------------------

// Lua: Spring.GetGameFrame() + math.floor(OV.ord.selfdestruct_delay * FPS + 0.5)
Step SelfDestructAt(Step now, float delayS) {
	return now + static_cast<Step>(std::floor(delayS * Sps() + DetMath::HALF));
}

} // namespace KT1::Core::Orders
