/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: SimProbe (version 1). The sim host's test system: it uses every part of the host and changes nothing in a
// game, except that it refuses one command id no unit has (game-build docs/engine/rules/simhost.md).
//
// Replaces:   nothing. LuaRules/Gadgets/dev_kt1_simprobe.lua records the same things from the Lua call-ins in check mode
// Switch:     mod option kt1_simprobe: 0 off (default), 1 on, check (on, and the gadget compares both)
// Config:     gamedata/kt1/simprobe.lua
//               deny_command     the command id its validator refuses (DESIGN 39990, a test id; 0: none)
//               report_every_s   seconds between [SIMPROBE] lines (DESIGN 10)
// Phases:     FrameStart (order 0): state from the events, and the check stream; the four later phases only count
// Events:     all
// State:      per unit: damage taken, unit events; both in the sync checksum
// Display:    one [SIMPROBE] log line per report, through DisplayEvents
// Timer:      KT1::SimProbe
// Lua:        Spring.KT1.SimProbe.Take / Commands / State / Counts / DenyCommand (KT1SimProbeLua.cpp)
// Determinism: events in raise order; float sum in the order raised; no id walk outside DenseIndex::AscendingIds.

#include "KT1SimProbe.h"

#include <cstdio>

#include "Game/KT1/KT1Register.h"
#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Host/AllowChain.h"
#include "Game/KT1/Host/DisplayEvents.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/SyncState.h"
#include "Lua/LuaParser.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/CommandAI/Command.h"
#include "Sim/Units/Unit.h"

namespace KT1 {
namespace SimProbe {

using Core::SimHost::ALL_EVENTS;
using Core::SimHost::Event;
using Core::SimHost::Phase;

namespace {
constexpr size_t MAX_RECORDS = 1 << 20;   // DESIGN: check-mode records held for a gadget that does not take them
}

SimProbeRule::SimProbeRule() : Rule({"SimProbe", 1, true, "nothing (LuaRules/Gadgets/dev_kt1_simprobe.lua checks it)", false}) {}

void SimProbeRule::LoadConfig(const LuaTable& cfg) {
	denyCommand = cfg.GetInt("deny_command", 39990);
	reportEvery = Core::Rate::Every(cfg.GetFloat("report_every_s", 10.0f));
	state.Init(MAX_UNITS);
	RegisterPhases();
	RegisterValidators();
	SyncState::Add("SimProbe", "damage", state.units.index, state.damage);
	SyncState::Add("SimProbe", "events", state.units.index, state.events);
}

void SimProbeRule::RegisterPhases() {
	Phases::Add("SimProbe", Phase::FrameStart, 0, ALL_EVENTS, [this](const SimContext& c) { FrameStart(c); });
	for (const Phase p : {Phase::AfterCommands, Phase::AfterMove, Phase::AfterWeapons, Phase::FrameEnd})
		Phases::Add("SimProbe", p, 0, ALL_EVENTS, [this](const SimContext& c) { Count(c); });
}

void SimProbeRule::RegisterValidators() {
	const auto saw = [this](Veto v) { ++asked[static_cast<size_t>(v)]; };
	AllowChain::AddCommand("SimProbe", 0, [this, saw](const CUnit* u, const ::Command& c, int, bool, bool) {
		saw(Veto::Command);
		if (CheckMode()) {
			if (commands.size() < MAX_RECORDS) {
				commands.push_back(u->id);
				commands.push_back(c.GetID());
			} else {
				++dropped;
			}
		}
		return denyCommand == 0 || c.GetID() != denyCommand;
	});
	AllowChain::AddUnitCreation("SimProbe", 0, [saw](const UnitDef*, const CUnit*, const BuildInfo*, bool&) { saw(Veto::Creation); return true; });
	AllowChain::AddUnitTransfer("SimProbe", 0, [saw](const CUnit*, int, bool) { saw(Veto::Transfer); return true; });
	AllowChain::AddBuildStep("SimProbe", 0, [saw](const CUnit*, const CUnit*, float) { saw(Veto::BuildStep); return true; });
	AllowChain::AddCaptureStep("SimProbe", 0, [saw](const CUnit*, const CUnit*, float) { saw(Veto::CaptureStep); return true; });
	AllowChain::AddFeatureBuildStep("SimProbe", 0, [saw](const CUnit*, const CFeature*, float) { saw(Veto::FeatureStep); return true; });
	AllowChain::AddWeaponTarget("SimProbe", 0, [saw](unsigned, unsigned, unsigned, unsigned, float*) { saw(Veto::Target); return true; });
	AllowChain::AddPreDamage("SimProbe", 0, [saw](const PreDamage&, float&, float&) { saw(Veto::PreDamage); });
}

void SimProbeRule::FrameStart(const SimContext& c) {
	c.events.ForEachRaised([&](const Event& e) {
		++seen[static_cast<size_t>(c.phase)];
		state.Apply(e);
		if (!CheckMode()) return;
		if (stream.size() < MAX_RECORDS) stream.push_back(e);
		else ++dropped;
	});
}

void SimProbeRule::Count(const SimContext& c) {
	for (size_t t = 0; t < Core::SimHost::EVENT_TYPES; ++t)
		seen[static_cast<size_t>(c.phase)] += c.events.Size(static_cast<Core::SimHost::EventType>(t));
	if (c.phase == Phase::FrameEnd && c.frame % reportEvery == 0) Report(c.frame);
}

void SimProbeRule::Report(int frame) {
	char line[Core::SimHost::DISPLAY_TEXT];
	std::snprintf(line, sizeof(line), "[SIMPROBE] f=%d seen=%llu/%llu/%llu/%llu/%llu cmds=%llu predmg=%llu units=%d",
		frame, (unsigned long long)seen[0], (unsigned long long)seen[1], (unsigned long long)seen[2],
		(unsigned long long)seen[3], (unsigned long long)seen[4],
		(unsigned long long)asked[static_cast<size_t>(Veto::Command)],
		(unsigned long long)asked[static_cast<size_t>(Veto::PreDamage)], state.units.index.Size());
	DisplayEvents::Log(line);
}

} // namespace SimProbe

KT1_REGISTER_SYSTEM(SimProbe, 2000, SimProbe::SimProbeRule)

} // namespace KT1
