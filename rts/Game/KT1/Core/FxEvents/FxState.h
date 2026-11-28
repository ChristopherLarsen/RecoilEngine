/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: the synced effect system's state (spec columns events and wreck_impulse, FxWreck.h) and the facts it
// holds per def. Core logic (FxLogic.h) writes events, spawns and writes into FxOutput; Rules/FxEvents carries them out
// (DisplayEvents::Emit and Effect, SimContext::Write) and clears it. No engine include.

#ifndef KT1_FX_STATE_H
#define KT1_FX_STATE_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxBudget.h"
#include "Game/KT1/Core/FxEvents/FxConfig.h"
#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Core/FxEvents/FxWorld.h"
#include "Game/KT1/Core/FxEvents/FxWreck.h"
#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::Core::Fx {

struct FxOutput {                                    // of this step, in the order made
	std::vector<SimHost::DisplayEvent> events;       // spec column events: kind and every field
	std::vector<EffectSpawn> spawns;
	std::vector<WreckWrite> wreckWrites;
	std::vector<int32_t> releaseWrecks;              // back to the engine's own feature physics
	std::vector<int32_t> hideFeatures, showFeatures; // look only: the unit wreck under a death's blasts

	template<class T> void Raise(const T& payload) {
		SimHost::DisplayEvent e;
		e.SetPayload(payload);
		events.push_back(e);
	}
	void Clear() {
		events.clear();
		spawns.clear();
		wreckWrites.clear();
		releaseWrecks.clear();
		hideFeatures.clear();
		showFeatures.clear();
	}
};

// ---------------------------------------------------------------- per def, built at Start

struct GatedScorch {                                 // a look's scorch row (WVFX-*-001 R11)
	bool on = false, leaves = false;
	float gate = 0.0f, size = 0.0f, sizePlus = 0.0f, life = 0.0f, lifePlus = 0.0f, radius = 0.0f;   // radius: elmos
	EffectRef puff;
};

struct SynWeapon {
	EffectRef hit;                                   // spawned from the explosion; none: NO_EFFECT
	EffectRef confirm[static_cast<int>(Tier::Count)];     // the unit-hit confirmation by tier; none: NO_EFFECT
	int8_t unitHitTier = -1;                         // fx_unit_hit: the unit set replaces the ground hit; -1: no
	int32_t contactSteps = 0;                        // fx_contact_every: a beam's hit sets per shooter
	uint8_t depthTier = 0;                           // 1 .. 4: the depth look class; 0: no
	GatedScorch gated;
	bool decal = false;                              // a detonation's own mark, every time
	float decalSize = 0.0f, decalLife = 0.0f;
	bool scorchWatched = false;                      // large enough and no beam (gfx_scorch.lua)
	BlastClass blastClass = BlastClass::None;        // None: no blast (also: owned elsewhere)
	BlastFamily blastFamily = BlastFamily::None;
	Faction faction = Faction::Human;
	EffectRef blastCore;
	bool beamBlast = false;                          // the blast family is beam: rate limited per cell
	ScriptTree script = ScriptTree::None;
	bool depthLaunch = false;                        // its projectiles raise DepthLaunch
	WreckPushWeapon push;
};

struct SynUnitDef {
	DeathKind deathKind = DeathKind::Mobile;
	uint8_t buildSlot = 0;                           // 0: none
	bool giant = false;
	bool structure = false;                          // speed 0 and cannot fly (the criteria chain)
	float footprintRadius = 0.0f;                    // U
};

struct SynFeatureDef { bool unitWreck = false; };

struct BuildSlot {                                   // BuildSlotRow resolved
	std::vector<EffectRef> effects;
	int32_t period = 0;                              // steps
	bool pool = false;
};

struct FxDefs {
	std::vector<SynWeapon> weapons;                  // by weapon def id
	std::vector<SynUnitDef> units;                   // by unit def id
	std::vector<SynFeatureDef> features;             // by feature def id
	std::vector<EffectRef> deathSets;                // by DeathPlan::sets index
	BuildSlot buildSlots[BUILD_SLOTS + 1];           // by slot; [0] unused
	DeathPlan death;
	WreckParams wreck;
	EffectRef giantCore;
	int32_t giantScorchWeapon = -1;
};

// ---------------------------------------------------------------- state

struct PendingSet { int32_t at = 0; int32_t set = -1; int32_t unit = -1, defId = -1; Vec3 pos; };
struct HideWait { float x = 0.0f, z = 0.0f, reach = 0.0f; int32_t show = 0, ends = 0; };
struct HiddenFeature { int32_t feature = -1, show = 0; };
struct KeyStep { int64_t key = 0; int32_t step = 0; };     // a rate limit's last step, sorted by key

struct FxState {
	Budget budget;

	// fx_core
	std::vector<int32_t> lastConfirm;                // by unit id: the step of the last confirmation; -1: none
	std::vector<Vec3> lastGated;                     // by weapon def: the last gated mark's place
	std::vector<uint8_t> hasGated;
	std::vector<KeyStep> contact;                    // beam hit sets: attacker * 4096 + weapon
	std::vector<KeyStep> beamBlast;                  // beam blasts: weapon and ground cell
	uint32_t intercepts = 0;
	uint32_t spawnedTotal = 0;

	// per step limits
	int32_t limitStep = -1;
	int32_t abilityUsed = 0, scriptUsed = 0, scorchUsed = 0, blastUsed = 0;

	// fx_build_sets: units under construction
	DenseIndex building;
	Column<uint8_t> slot;

	// fx_death_sets
	DenseIndex damaged;                              // units a hit put below a threshold
	Column<int32_t> stateNext[DEATH_STATES];         // per damage state: the step of its next emitters
	std::vector<PendingSet> pending;                 // in the order made

	// gfx_criteria_fx (synced): wrecks waiting for the main blast
	std::vector<HideWait> hideWaits;
	std::vector<HiddenFeature> hidden;

	WreckState wrecks;
	StrategicState strategic;
};

} // namespace KT1::Core::Fx

#endif
