/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: the tests behind the table's conditions, and the state the guard rules keep (the nine guard
// gadgets and the refuse rules of ord_orders.lua). Pure functions; the adapter gathers the facts and gives the orders.
// No engine include.
//
// Besides the Allow chain the system has frame work: a transferred factory's queue trim is repeated on the next steps
// (ord_transfer.lua), idle aircraft over water are looked at (mov_air_idle_water.lua), and cargo of a short set-down is
// moved when it leaves the transport (mov_air_unload_gap.lua).

#ifndef KT1_CORE_ORDER_RULES_GUARD_LOGIC_H
#define KT1_CORE_ORDER_RULES_GUARD_LOGIC_H

#include <cstdint>
#include <span>
#include <vector>

#include "OrderRuleRow.h"
#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Common/SortedIdList.h"
#include "Game/KT1/Core/SimHost/UnitColumns.h"

namespace KT1::Core::OrderRules {

using Orders::Step;
using Orders::UnitId;

// ---- state ---------------------------------------------------------------------------------------------------------

struct ReclaimStill {                // ord_reclaim_still.lua rec[b]
	bool seen = false;
	Step last = Orders::NO_STEP;     // the last step asked
	Step since = Orders::NO_STEP;    // first stopped in reach
};
struct IdleAir {                     // mov_air_idle_water.lua air[u], held[u]
	bool held = false;               // the idle mode is held at "fly"
	int32_t mode = 1;                // what is given back (1: land when idle)
};
struct UnloadPending {               // mov_air_unload_gap.lua pending[transport]
	bool active = false;
	float x = 0, z = 0;              // where its cargo goes when it is out
};
struct TransferPending {             // ord_transfer.lua pending[factory]
	bool active = false;
	int32_t team = -1;               // the new team
	int32_t keep = 0;                // queued items that stay (the one in progress)
	int32_t tries = 0;
};

class State {
public:
	void Init(int32_t maxUnits);
	DenseIndex::Slot Track(UnitId unit);
	DenseIndex::Slot SlotOf(UnitId unit) const { return units.index.SlotOf(unit); }
	void Forget(UnitId unit);        // UnitDestroyed
	void BeginStep();                // empties `reissued`

	// the per-row guard against a row asking about the order it re-issues
	bool Reissuing(UnitId unit, int32_t row) const;
	void SetReissuing(UnitId unit, int32_t row, bool on);

	SimHost::UnitColumns units;
	Column<uint64_t> reissuing;      // one bit per row
	Column<ReclaimStill> reclaim;    // by builder
	Column<IdleAir> idleAir;
	Column<UnloadPending> unload;
	Column<TransferPending> transfer;
	SortedIdList landers;            // aircraft that land when idle, ascending
	SortedIdList transfers;          // factories with a pending trim, ascending

	bool settingIdleMode = false;    // the idle-mode order of the frame work (the HeldIdleMode row lets it through)

	std::vector<Orders::OrderIssue> reissued;   // per step: trace column reissued
};

// ---- the tests -----------------------------------------------------------------------------------------------------

bool BeyondMap(float x, float z, float mapX, float mapZ);
// raw line-of-sight bits of the issuer's army: 1 in sight, 2 radar, 4 seen before
bool EnemyUnseen(bool allied, bool hasLosState, int32_t losBits);
bool Unreclaimable(UnitId unit, UnitId target, const Orders::DefFacts& targetDef);
// the kind an engine fight order to a point becomes: attack-move for an armed unit, else move
Orders::CommandKind AttackMoveRewrite(const Orders::DefFacts& def);
// ORD-010 v6 R6: false: refuse. factoryRally: a factory's fight to a point. Internal orders and self-destruct pass.
// An attack-move order counts as a fight, here and for the Carried condition; a unit that accepts attack accepts it.
bool PackAccepts(const Orders::DefFacts& def, Orders::CommandKind kind, bool factoryRally);
bool DefAccepts(const Orders::DefFacts& def, Orders::CommandKind kind);      // ord_edge.lua R6 (spc_accepts)
// the fire state a game fire-state order sets: its parameter, else the next in the cycle
Orders::FireState NextFireState(Orders::FireState current, bool hasParam, float param);
// an engine fire-state order from elsewhere: the engine's hold is hold, any other value free
Orders::FireState FireFromEngine(int32_t engineValue, int32_t engineHold);
bool DiveWanted(bool hasParam, float param, bool submerged);
bool ShortMove(float dx, float dz, float minDistance);                       // MOV-019 v3 R1
// a queued move of `queue` lies within the cancel distance of (x, z)
bool NearQueuedMove(std::span<const Orders::OrderView> queue, float x, float z, float cancel);
// MOV-003 v4 R3: the goal's cell centre, `rest` short of it on the way from (fx, fz); y is the caller's ground height
void RestPoint(float x, float z, float fx, float fz, float cell, float rest, float& outX, float& outZ);

// air set-down short of a held spot (MOV-016 v9 R12)
struct UnloadPoints {
	float landX = 0, landZ = 0;
	float cargoX = 0, cargoZ = 0;
};
bool PlanUnloadPoints(float fx, float fz, float sx, float sz, float landGap, float cargoGap, float same,
                      UnloadPoints& out);   // false: the transport is on the spot
bool HoldsSpot(float ux, float uz, float radius, float sx, float sz, float cargoRadius);

// an attack on a unit known but not seen, within range: attack its position
bool KnownNotSeen(int32_t losBits);
bool KnownTargetInRange(float range, float distSq);

// ord_edge.lua R3: features of an area, nearest first (ties: feature id)
struct AreaFeature {
	int32_t feature = -1;
	float distSq = 0;
};
void SortAreaFeatures(std::span<AreaFeature> features);

// reclaim only while standing still: one asked step; true when the latency has passed
bool Still(float vx, float vz, float limit);   // elmos per step
bool AskReclaim(ReclaimStill& record, bool still, Step now, Step latency);

// ORD-012 v3 R6: queued builds kept by a transfer
int32_t TransferKeep(bool building, int32_t builds);

} // namespace KT1::Core::OrderRules

#endif
