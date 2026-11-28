/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 core: the sound cues' state and rules (LUA-FREE H01, FRAME §3-5; replaces the synced half of snd_events and its
// shield and zoom-gate rules). No engine include. Cues are outputs only: nothing here is read back by the simulation and
// nothing is in the sync checksum, so the synced half is compute-only in check mode.
// Facts come in, cues come out: the adapter (or the system that owns a fact) calls the rule for it, then admits and sends
// what it returns. Lua's order of one step: the ability and fuel checks, the movement share, then the build loops.
//
// Check column "cue_log": (step, unit, cue id, position), in order (CueState::log).

#ifndef KT1_CORE_CUE_STATE_H
#define KT1_CORE_CUE_STATE_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "CueRow.h"

namespace KT1::Core::Cues {

// the config in sim units, made once at game setup (Core::Rate)
struct CueParams {
	float cuesPerStep = 0.0f;
	float movingSpeed = 0.0f;        // elmos per step
	int32_t moveCheckSteps = 1;
	int32_t beamGapSteps = 0;
	int32_t captureStallSteps = 1;
	int32_t buildCheckSteps = 1;
	int32_t shieldHitGapSteps = 0;
	float fuelLowFraction = 0.0f;
	float fuelFallEpsilon = 0.0f;
	float fuelMoveEpsilon = 0.0f;
	float healthEpsilon = 0.0f;
	float captureEpsilon = 0.0f;
};

CueParams MakeParams(const CueConfig& config);

struct CueLogEntry {
	int32_t step = 0;
	int32_t unit = 0;
	int32_t cue = -1;                // CueState::CueId
	float pos[3] = {0.0f, 0.0f, 0.0f};
};

struct CueUnitKind {
	bool mobile = false;             // speed over 0 and not a building
	bool fuelled = false;            // it has a fuel capacity
	bool builder = false;            // a builder with a build speed
};

struct FuelFacts {
	float fuel = 0.0f;               // seconds of flight left
	float capacity = 0.0f;
	float lowFraction = -1.0f;       // below 0: the config's
	bool onPad = false;
	float health = 0.0f, maxHealth = 0.0f;
};

constexpr uint8_t FUEL_LOW = 1, FUEL_EMPTY = 2, FUEL_REFUEL = 4, FUEL_REPAIR = 8;   // DESIGN: bits FuelStep returns

enum class CaptureCue : uint8_t { None, Start, Stall };
enum class LayerCue : uint8_t { None, Dive, Surface };
enum class ShieldCue : uint8_t { None, Up, Break, Fall };

constexpr int8_t SHIELD_UNSEEN = -2;   // DESIGN: no state seen of this shield yet

struct CueState {
	DenseIndex units;
	Column<uint8_t> mobile, fuelled, builder;
	Column<uint8_t> moving, loopOn;
	Column<uint8_t> fuelSeen, fuelLow, fuelEmpty, refuel, repair, healthSeen;
	Column<float> fuelValue, healthValue;
	Column<int8_t> layer;            // -1 not seen, 0 surface or other, 1 submerged
	Column<int8_t> shieldUp;         // SHIELD_UNSEEN, 0 down, 1 up
	Column<int32_t> shieldHitStep;
	Column<int32_t> captor;          // by target: the capturing unit; -1: none
	Column<float> captureProgress;
	Column<int32_t> captureGrew;     // step of the last growth
	Column<uint8_t> captureState;    // 0 none, 1 growing, 2 stalled

	std::vector<int32_t> beamStep;           // by weapon def: step of its last beam impact cue
	std::vector<int32_t> rowStep;            // by row: step of its last raise
	std::vector<int32_t> rowCount;           // by row: raises in the current second
	std::vector<std::string> cueNames;       // cue id -> name, in the order first seen (config rows first)
	std::vector<CueLogEntry> log;

	int32_t step = 0;
	float budget = 0.0f;             // cues still admitted this step
	uint32_t sent = 0, cut = 0;      // totals (game rules params snd_sent, snd_cut)

	void Init(int32_t maxUnits, int32_t weaponDefs, const CueConfig& config);
	void AddUnit(int32_t id, const CueUnitKind& kind);
	bool RemoveUnit(int32_t id);     // true: its build loop was on and a stop is to be sent
};

int32_t CueId(CueState& state, std::string_view name);
void BeginStep(CueState& state, const CueParams& params, int32_t step);
bool Admit(CueState& state);                      // false: over the limit, counted as cut
void Log(CueState& state, int32_t unit, int32_t cue, const float (&pos)[3]);
bool RowMatches(const CueRow& row, std::string_view on, std::string_view unitClass, std::string_view weapon, TeamRelation relation);
bool RowAdmit(CueState& state, const CueConfig& config, int32_t row);
bool BeamAdmit(CueState& state, const CueParams& params, int32_t weaponDef);
bool MoveDue(const CueParams& params, int32_t id, int32_t step);   // this step's share of the units
bool MoveChanged(CueState& state, int32_t id, bool moving);        // true: send move_start or move_stop
bool LoopChanged(CueState& state, int32_t id, bool on);            // true: send the loop event
uint8_t FuelStep(CueState& state, const CueParams& params, int32_t id, const FuelFacts& facts);   // FUEL_ bits
void FuelForget(CueState& state, int32_t id);                      // the unit has no fuel entry any more
CaptureCue CaptureStep(CueState& state, const CueParams& params, int32_t target, int32_t captor, float progress);
void CaptureEnd(CueState& state, int32_t target);                  // captured: no stall cue
LayerCue LayerStep(CueState& state, int32_t id, bool submerged);
// up: 1 up, 0 down, -1 gone; collapsed: why a down shield is down. The first state seen plays nothing.
ShieldCue ShieldStateStep(CueState& state, int32_t id, int up, bool collapsed);
bool ShieldHitAdmit(CueState& state, const CueParams& params, int32_t id);
// the audio side: is this cue held back at this camera height above the ground (cut 0: never)
bool ZoomHeld(float height, float cut);

} // namespace KT1::Core::Cues

#endif
