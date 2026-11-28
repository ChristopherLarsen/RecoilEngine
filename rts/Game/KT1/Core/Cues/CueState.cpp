/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: CueState.h (LUA-FREE D10): the rules of snd_events, in sim units.

#include "CueState.h"

#include <algorithm>
#include <cmath>

#include "Game/KT1/Core/Common/Rate.h"

namespace KT1::Core::Cues {
namespace {

constexpr float BUDGET_SLACK = 0.001f;   // DESIGN: float error allowed when a step's share of cues adds up to a whole one
constexpr int32_t NEVER = -1;            // DESIGN: no step yet

template<typename F> void ForColumns(CueState& s, F f) {
	f(s.mobile); f(s.fuelled); f(s.builder); f(s.moving); f(s.loopOn);
	f(s.fuelSeen); f(s.fuelLow); f(s.fuelEmpty); f(s.refuel); f(s.repair); f(s.healthSeen);
	f(s.fuelValue); f(s.healthValue); f(s.layer); f(s.shieldUp); f(s.shieldHitStep);
	f(s.captor); f(s.captureProgress); f(s.captureGrew); f(s.captureState);
}

DenseIndex::Slot Slot(const CueState& s, int32_t id) { return s.units.SlotOf(id); }

// a flag that follows a condition; true: send the change
bool Flip(Column<uint8_t>& column, DenseIndex::Slot slot, bool value) {
	if (slot == DenseIndex::NONE || (column[slot] != 0) == value) return false;
	column[slot] = value ? 1 : 0;
	return true;
}

// once per episode: raises `bit` when `starts` and not raised yet; `ends` forgets it
uint8_t Episode(uint8_t& raised, bool starts, bool ends, uint8_t bit) {
	if (starts && raised == 0) {
		raised = 1;
		return bit;
	}
	if (ends) raised = 0;
	return 0;
}

uint8_t RepairStep(CueState& s, const CueParams& p, DenseIndex::Slot i, const FuelFacts& f) {
	if (!f.onPad) {
		s.repair[i] = 0;
		s.healthSeen[i] = 0;
		return 0;
	}
	const bool rose = s.healthSeen[i] != 0 && f.health > s.healthValue[i] + p.healthEpsilon;
	const uint8_t bit = Episode(s.repair[i], rose, f.health >= f.maxHealth - p.healthEpsilon, FUEL_REPAIR);
	s.healthValue[i] = f.health;
	s.healthSeen[i] = 1;
	return bit;
}

} // namespace

CueParams MakeParams(const CueConfig& c) {
	CueParams p;
	const float step = Rate::StepSeconds();
	p.cuesPerStep = c.max_per_s * step;
	p.movingSpeed = c.moving_speed * step;
	p.moveCheckSteps = Rate::Every(c.move_check_s);
	p.beamGapSteps = Rate::Steps(c.beam_gap_s);
	p.captureStallSteps = Rate::Every(c.capture_stall_s);
	p.buildCheckSteps = Rate::Every(c.build_check_s);
	p.shieldHitGapSteps = Rate::Steps(c.shield_hit_gap_s);
	p.fuelLowFraction = c.fuel_low_fraction;
	p.fuelFallEpsilon = c.fuel_fall_epsilon;
	p.fuelMoveEpsilon = c.fuel_move_epsilon;
	p.healthEpsilon = c.health_epsilon;
	p.captureEpsilon = c.capture_epsilon;
	return p;
}

void CueState::Init(int32_t maxUnits, int32_t weaponDefs, const CueConfig& config) {
	units.Init(maxUnits);
	ForColumns(*this, [this](auto& column) { column.Init(units); });
	layer.Init(units, -1);
	shieldUp.Init(units, SHIELD_UNSEEN);
	shieldHitStep.Init(units, NEVER);
	captor.Init(units, NEVER);
	beamStep.assign(static_cast<size_t>(std::max<int32_t>(0, weaponDefs)) + 1, NEVER);   // DESIGN: weapon def ids start at 0 or 1
	rowStep.assign(config.rows.size(), NEVER);
	rowCount.assign(config.rows.size(), 0);
	cueNames.clear();
	for (const CueRow& row : config.rows) CueId(*this, row.cue);
	log.clear();
	step = 0;
	budget = 0.0f;
	sent = 0;
	cut = 0;
}

void CueState::AddUnit(int32_t id, const CueUnitKind& kind) {
	const DenseIndex::Slot i = units.Add(id);
	if (i == DenseIndex::NONE) return;
	mobile[i] = kind.mobile ? 1 : 0;
	fuelled[i] = kind.fuelled ? 1 : 0;
	builder[i] = kind.builder ? 1 : 0;
}

bool CueState::RemoveUnit(int32_t id) {
	const DenseIndex::Slot i = units.SlotOf(id);
	if (i == DenseIndex::NONE) return false;
	const bool loop = loopOn[i] != 0;
	const DenseIndex::Move move = units.Remove(id);
	ForColumns(*this, [&move](auto& column) { column.Apply(move); });
	return loop;
}

int32_t CueId(CueState& state, std::string_view name) {
	for (size_t i = 0; i < state.cueNames.size(); ++i) {
		if (state.cueNames[i] == name) return static_cast<int32_t>(i);
	}
	state.cueNames.emplace_back(name);
	return static_cast<int32_t>(state.cueNames.size()) - 1;
}

void BeginStep(CueState& state, const CueParams& params, int32_t step) {
	state.step = step;
	// whole cues left over are lost with their step; the fraction is carried, so a rate under one a step still passes
	const float carried = std::max(0.0f, state.budget - std::floor(state.budget + BUDGET_SLACK));
	state.budget = carried + params.cuesPerStep;
}

bool Admit(CueState& state) {
	if (state.budget < 1.0f - BUDGET_SLACK) {
		++state.cut;
		return false;
	}
	state.budget -= 1.0f;
	++state.sent;
	return true;
}

void Log(CueState& state, int32_t unit, int32_t cue, const float (&pos)[3]) {
	state.log.push_back({state.step, unit, cue, {pos[0], pos[1], pos[2]}});
}

bool RowMatches(const CueRow& row, std::string_view on, std::string_view unitClass, std::string_view weapon, TeamRelation relation) {
	const EventMapRow& m = row.map;
	if (m.on != on) return false;
	if (!m.unit_class.empty() && m.unit_class != unitClass) return false;
	if (!m.weapon.empty() && m.weapon != weapon) return false;
	const TeamRelation wanted = ParseRelation(m.relation);
	if (wanted == TeamRelation::Any || wanted == relation) return true;
	return wanted == TeamRelation::Allied && relation == TeamRelation::Own;   // the viewer's own team is allied
}

bool RowAdmit(CueState& state, const CueConfig& config, int32_t row) {
	if (row < 0 || row >= static_cast<int32_t>(state.rowStep.size())) return false;
	const CueRow& r = config.rows[row];
	int32_t& last = state.rowStep[row];
	if (r.min_gap_s > 0.0f && last >= 0 && state.step - last < Rate::Steps(r.min_gap_s)) return false;
	const int32_t perSecond = Rate::StepsPerSecond();
	if (last < 0 || last / perSecond != state.step / perSecond) state.rowCount[row] = 0;
	if (r.max_per_s > 0.0f && static_cast<float>(state.rowCount[row]) >= r.max_per_s) return false;
	++state.rowCount[row];
	last = state.step;
	return true;
}

bool BeamAdmit(CueState& state, const CueParams& params, int32_t weaponDef) {
	if (weaponDef < 0 || weaponDef >= static_cast<int32_t>(state.beamStep.size())) return false;
	int32_t& last = state.beamStep[weaponDef];
	if (last >= 0 && state.step - last < params.beamGapSteps) return false;
	last = state.step;
	return true;
}

bool MoveDue(const CueParams& params, int32_t id, int32_t step) {
	return id % params.moveCheckSteps == step % params.moveCheckSteps;
}

bool MoveChanged(CueState& state, int32_t id, bool moving) { return Flip(state.moving, Slot(state, id), moving); }

bool LoopChanged(CueState& state, int32_t id, bool on) { return Flip(state.loopOn, Slot(state, id), on); }

uint8_t FuelStep(CueState& state, const CueParams& params, int32_t id, const FuelFacts& facts) {
	const DenseIndex::Slot i = Slot(state, id);
	if (i == DenseIndex::NONE || facts.capacity <= 0.0f) return 0;
	const float v = facts.fuel;
	if (state.fuelSeen[i] == 0) {
		state.fuelSeen[i] = 1;
		state.fuelValue[i] = v;
	}
	const float before = state.fuelValue[i];
	const float low = (facts.lowFraction < 0.0f ? params.fuelLowFraction : facts.lowFraction) * facts.capacity;

	uint8_t cues = Episode(state.fuelLow[i], v < low && v < before + params.fuelFallEpsilon, v >= low, FUEL_LOW);
	cues |= Episode(state.fuelEmpty[i], v <= 0.0f, v > 0.0f, FUEL_EMPTY);
	const bool rising = v > before + params.fuelMoveEpsilon;
	const bool over = !rising && (v >= facts.capacity - params.fuelFallEpsilon || v < before - params.fuelMoveEpsilon);
	cues |= Episode(state.refuel[i], rising, over, FUEL_REFUEL);
	state.fuelValue[i] = v;
	return cues | RepairStep(state, params, i, facts);
}

void FuelForget(CueState& state, int32_t id) {
	const DenseIndex::Slot i = Slot(state, id);
	if (i == DenseIndex::NONE) return;
	state.fuelSeen[i] = state.fuelLow[i] = state.fuelEmpty[i] = state.refuel[i] = state.repair[i] = state.healthSeen[i] = 0;
}

CaptureCue CaptureStep(CueState& state, const CueParams& params, int32_t target, int32_t captor, float progress) {
	const DenseIndex::Slot i = Slot(state, target);
	if (i == DenseIndex::NONE) return CaptureCue::None;
	uint8_t& phase = state.captureState[i];     // 0 none, 1 growing, 2 stalled
	state.captor[i] = captor;
	if (progress <= 0.0f) {                     // work that had begun is lost
		const bool stalls = phase == 1;
		CaptureEnd(state, target);
		return stalls ? CaptureCue::Stall : CaptureCue::None;
	}
	CaptureCue cue = CaptureCue::None;
	if (phase == 0) {
		phase = 1;
		state.captureProgress[i] = progress;
		state.captureGrew[i] = state.step;
		cue = CaptureCue::Start;
	}
	if (progress > state.captureProgress[i] + params.captureEpsilon) {
		state.captureGrew[i] = state.step;
		phase = 1;
	}
	state.captureProgress[i] = progress;
	if (phase == 1 && state.step - state.captureGrew[i] >= params.captureStallSteps) {
		phase = 2;
		cue = CaptureCue::Stall;
	}
	return cue;
}

void CaptureEnd(CueState& state, int32_t target) {
	const DenseIndex::Slot i = Slot(state, target);
	if (i == DenseIndex::NONE) return;
	state.captureState[i] = 0;
	state.captor[i] = NEVER;
	state.captureProgress[i] = 0.0f;
}

LayerCue LayerStep(CueState& state, int32_t id, bool submerged) {
	const DenseIndex::Slot i = Slot(state, id);
	if (i == DenseIndex::NONE) return LayerCue::None;
	const int8_t before = state.layer[i];
	state.layer[i] = submerged ? 1 : 0;
	if (before < 0 || before == state.layer[i]) return LayerCue::None;
	return submerged ? LayerCue::Dive : LayerCue::Surface;
}

ShieldCue ShieldStateStep(CueState& state, int32_t id, int up, bool collapsed) {
	const DenseIndex::Slot i = Slot(state, id);
	if (i == DenseIndex::NONE) return ShieldCue::None;
	if (up < 0) {
		state.shieldUp[i] = SHIELD_UNSEEN;
		state.shieldHitStep[i] = NEVER;
		return ShieldCue::None;
	}
	const int8_t before = state.shieldUp[i];
	state.shieldUp[i] = static_cast<int8_t>(up);
	if (before == SHIELD_UNSEEN || before == up) return ShieldCue::None;
	if (up == 1) return ShieldCue::Up;
	return collapsed ? ShieldCue::Break : ShieldCue::Fall;
}

bool ShieldHitAdmit(CueState& state, const CueParams& params, int32_t id) {
	const DenseIndex::Slot i = Slot(state, id);
	if (i == DenseIndex::NONE) return false;
	int32_t& last = state.shieldHitStep[i];
	if (last >= 0 && state.step - last < params.shieldHitGapSteps) return false;
	last = state.step;
	return true;
}

bool ZoomHeld(float height, float cut) { return cut > 0.0f && height > cut; }

} // namespace KT1::Core::Cues
