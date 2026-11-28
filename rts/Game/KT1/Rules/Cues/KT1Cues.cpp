/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Cues (version 1). Sound cues from simulation events (LUA-FREE D10, FRAME §4.5).
//
// Replaces:   in LuaRules/Gadgets/snd_events.lua: the cue limit, shots, impacts, movement start and stop, the build loops
//             and the delivery to the sound manager. The gadget stays for the facts only Lua holds (abilities, capture,
//             fuel, layers, shields) and raises them through GG.SoundEvent, which calls this rule.
// Switch:     mod option kt1_cues, else kt1_fam_frame; 0 (default) / 1 / check
// Config:     data/kt1/cues.json (Core/Cues/CueRow.h has the keys, defaults and labels)
// Phases:     FrameStart  tracks units; opens the step's budget; movement (one share), build loops
//             FrameEnd    shots and impacts in the order the engine raised them
// Writes:     none to the sim. Cues leave as display events (Core/Cues/CueEvents.h); the audio side plays them
//             (Audio/Cues/CueAudio.cpp).
// State:      outputs only; nothing in the sync checksum
// Check:      each cue is given to Host/Check under its name (unit id, x); loop changes under "loop"
// Lua:        Spring.KT1.Cues (Audio/Cues/CueBridge.h)
// Timer:      KT1::Cues
// Doc:        game-build docs/engine/rules/cues.md

#include "CueSim.h"

#include "Game/KT1/Audio/Cues/CueAudio.h"    // KT1 lint: allow sim-draw-include cue API
#include "Game/KT1/Audio/Cues/CueBridge.h"   // KT1 lint: allow sim-draw-include cue API
#include "Game/KT1/Data/DataEngine.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Weapons/WeaponDefHandler.h"

namespace KT1 {
namespace Cues {
namespace {

using Core::SimHost::EventType;
using Core::SimHost::MaskOf;
using Core::SimHost::Phase;

class CuesRule : public Rule {
public:
	CuesRule() : Rule({NAME, 1, true, "LuaRules/Gadgets/snd_events.lua", false, true}) {}
	~CuesRule() override { SetActiveSim(nullptr); }

	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		return Data::ParseInto(text, strictness, config);
	}

	// game setup, before LuaRules and any unit; the config is read by then (the rule has only the .json file)
	void Start() override {
		sim.Init(config, MAX_UNITS, weaponDefHandler->NumWeaponDefs());
		const auto units = MaskOf(EventType::UnitCreated) | MaskOf(EventType::UnitDestroyed);
		const auto hits = MaskOf(EventType::ProjectileCreated) | MaskOf(EventType::Explosion);
		Phases::Add(NAME, Phase::FrameStart, 0, units, [this](const SimContext& c) { sim.Step(c); });
		Phases::Add(NAME, Phase::FrameEnd, 0, hits, [this](const SimContext& c) { sim.Events(c); });
		SetActiveSim(&sim);
		CueAudio::Install();
	}

	void PushLua(lua_State* L) override { PushCuesBridge(L); }

private:
	Core::Cues::CueConfig config;
	CueSim sim;
};

} // namespace
} // namespace Cues

// ORDER: after movement; reads state only
using CuesRuleType = Cues::CuesRule;
KT1_REGISTER_SYSTEM(Cues, 4500, CuesRuleType)

} // namespace KT1
