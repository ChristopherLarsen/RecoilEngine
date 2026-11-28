/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 OrderRules core: the tests behind the table's conditions and the guard rules' state (GuardLogic.h). Each test is the
// replaced Lua's, in float and in the Lua's order. No engine include.

#include "GuardLogic.h"

#include <algorithm>

#include "Game/KT1/Core/Common/DetMath.h"

namespace KT1::Core::OrderRules {

using Orders::AcceptWord;
using Orders::CommandKind;
using Orders::DefFacts;
using Orders::DefFlag;
using Orders::FireState;
namespace Det = KT1::Core::DetMath;

namespace {

constexpr int32_t LOS_IN_SIGHT = 1;       // DESIGN: the engine's raw line-of-sight bit
constexpr int32_t LOS_IN_RADAR = 2;       // DESIGN: the engine's raw radar bit
constexpr int32_t LOS_SEEN_BEFORE = 4;    // DESIGN: the engine's raw previously-seen bit
constexpr float DIVE_PARAM = 1.0f;        // DESIGN: the dive order's parameter for "dive"
constexpr uint8_t POINT_PARAMS = 3;       // DESIGN: an order to a point holds x, y, z

float DistSq(float ax, float az, float bx, float bz) {
	const float dx = ax - bx;
	const float dz = az - bz;
	return dx * dx + dz * dz;
}

// the kinds ord_edge.lua checks against spc_accepts; every other order passes
bool SpecWord(CommandKind kind, AcceptWord& word) {
	switch (kind) {
		case CommandKind::FireState: word = AcceptWord::FireState; return true;
		case CommandKind::Attack: word = AcceptWord::Attack; return true;
		case CommandKind::Patrol: word = AcceptWord::Patrol; return true;
		case CommandKind::Guard: word = AcceptWord::Guard; return true;
		case CommandKind::Repair: word = AcceptWord::Repair; return true;
		case CommandKind::Reclaim: word = AcceptWord::Reclaim; return true;
		case CommandKind::Capture: word = AcceptWord::Capture; return true;
		default: return false;
	}
}

} // namespace

// ---- state ---------------------------------------------------------------------------------------------------------

void State::Init(int32_t maxUnits) {
	units.Init(maxUnits);
	units.Own(reissuing);
	units.Own(reclaim);
	units.Own(idleAir);
	units.Own(unload);
	units.Own(transfer);
	landers.Init(maxUnits);
	transfers.Init(maxUnits);
	settingIdleMode = false;
	reissued.clear();
	reissued.reserve(static_cast<size_t>(std::max<int32_t>(0, maxUnits)));
}

DenseIndex::Slot State::Track(UnitId unit) { return units.AddUnit(unit); }

void State::Forget(UnitId unit) {
	units.RemoveUnit(unit);
	landers.Erase(unit);
	transfers.Erase(unit);
}

void State::BeginStep() { reissued.clear(); }

bool State::Reissuing(UnitId unit, int32_t row) const {
	const DenseIndex::Slot slot = SlotOf(unit);
	if (slot == DenseIndex::NONE || row < 0 || row >= static_cast<int32_t>(MAX_ROWS)) return false;
	return (reissuing[slot] >> row & uint64_t(1)) != 0;
}

void State::SetReissuing(UnitId unit, int32_t row, bool on) {
	if (row < 0 || row >= static_cast<int32_t>(MAX_ROWS)) return;
	const DenseIndex::Slot slot = Track(unit);
	if (slot == DenseIndex::NONE) return;
	const uint64_t bit = uint64_t(1) << row;
	reissuing[slot] = on ? (reissuing[slot] | bit) : (reissuing[slot] & ~bit);
}

// ---- the tests -----------------------------------------------------------------------------------------------------

bool BeyondMap(float x, float z, float mapX, float mapZ) { return x < 0 || z < 0 || x > mapX || z > mapZ; }

bool EnemyUnseen(bool allied, bool hasLosState, int32_t losBits) {
	if (allied) return false;
	return !hasLosState || (losBits & (LOS_IN_SIGHT | LOS_IN_RADAR)) == 0;
}

bool Unreclaimable(UnitId unit, UnitId target, const DefFacts& targetDef) {
	return target == unit || !targetDef.Has(DefFlag::Reclaimable);
}

CommandKind AttackMoveRewrite(const DefFacts& def) { return def.Armed() ? CommandKind::AMove : CommandKind::Move; }

bool PackAccepts(const DefFacts& def, CommandKind kind, bool factoryRally) {
	if (factoryRally || kind == CommandKind::SelfDestruct || !def.Has(DefFlag::HasAccepts)) return true;
	const CommandKind asked = (kind == CommandKind::AMove) ? CommandKind::Fight : kind;
	AcceptWord word = AcceptWord::Count;
	if (!Orders::AcceptWordOf(asked, word)) return true;
	if ((def.accepts & Orders::BitOf(word)) != 0) return true;
	return word == AcceptWord::AttackMove && (def.accepts & Orders::BitOf(AcceptWord::Attack)) != 0;
}

bool DefAccepts(const DefFacts& def, CommandKind kind) {
	AcceptWord word = AcceptWord::Count;
	if (!def.Has(DefFlag::HasSpecAccepts) || !SpecWord(kind, word)) return true;
	return (def.specAccepts & Orders::BitOf(word)) != 0;
}

FireState NextFireState(FireState current, bool hasParam, float param) {
	const int32_t count = static_cast<int32_t>(FireState::Count);
	const int32_t asked = hasParam ? static_cast<int32_t>(Det::Floor(param)) : static_cast<int32_t>(current) + 1;
	return static_cast<FireState>((asked % count + count) % count);   // the Lua's % is never negative
}

FireState FireFromEngine(int32_t engineValue, int32_t engineHold) {
	return (engineValue == engineHold) ? FireState::Hold : FireState::Free;
}

bool DiveWanted(bool hasParam, float param, bool submerged) { return hasParam ? param == DIVE_PARAM : !submerged; }

bool ShortMove(float dx, float dz, float minDistance) { return dx * dx + dz * dz < minDistance * minDistance; }

bool NearQueuedMove(std::span<const Orders::OrderView> queue, float x, float z, float cancel) {
	for (const Orders::OrderView& order : queue) {
		if (order.kind != CommandKind::Move || order.numParams < POINT_PARAMS) continue;
		if (DistSq(order.params[0], order.params[2], x, z) < cancel * cancel) return true;
	}
	return false;
}

void RestPoint(float x, float z, float fx, float fz, float cell, float rest, float& outX, float& outZ) {
	float cx = (Det::Floor(x / cell) + Det::HALF) * cell;
	float cz = (Det::Floor(z / cell) + Det::HALF) * cell;
	const float dx = cx - fx;
	const float dz = cz - fz;
	const float len = Det::Dist2(dx, dz);
	if (len > rest) {
		cx = cx - dx / len * rest;
		cz = cz - dz / len * rest;
	}
	outX = cx;
	outZ = cz;
}

bool PlanUnloadPoints(float fx, float fz, float sx, float sz, float landGap, float cargoGap, float same,
                      UnloadPoints& out) {
	const float dx = sx - fx;
	const float dz = sz - fz;
	const float len = Det::Dist2(dx, dz);
	if (len < same) return false;
	const float ux = dx / len;
	const float uz = dz / len;
	out = {sx - ux * landGap, sz - uz * landGap, sx - ux * cargoGap, sz - uz * cargoGap};
	return true;
}

bool HoldsSpot(float ux, float uz, float radius, float sx, float sz, float cargoRadius) {
	const float both = radius + cargoRadius;
	return DistSq(ux, uz, sx, sz) < both * both;
}

bool KnownNotSeen(int32_t losBits) {
	return (losBits & LOS_IN_SIGHT) == 0 && (losBits & (LOS_IN_RADAR | LOS_SEEN_BEFORE)) != 0;
}

bool KnownTargetInRange(float range, float distSq) { return range > 0 && distSq <= range * range; }

void SortAreaFeatures(std::span<AreaFeature> features) {
	std::sort(features.begin(), features.end(), [](const AreaFeature& a, const AreaFeature& b) {
		return (a.distSq != b.distSq) ? a.distSq < b.distSq : a.feature < b.feature;
	});
}

bool Still(float vx, float vz, float limit) { return vx * vx + vz * vz <= limit * limit; }

bool AskReclaim(ReclaimStill& record, bool still, Step now, Step latency) {
	// moving, or a step not asked on the step before: the latency restarts
	if (!still || !record.seen || now - record.last > 1) record.since = now;
	record.seen = true;
	record.last = now;
	return now - record.since >= latency;
}

int32_t TransferKeep(bool building, int32_t builds) { return (building && builds > 0) ? 1 : 0; }

} // namespace KT1::Core::OrderRules
