/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: TemplateRule (version 1). <One sentence: what the rule does in the game.>
// The template of a synced system (LUA-FREE F09): compiled, never registered (sources.cmake). To copy it:
//   1. copy Rules/_TEMPLATE, the Core/_TEMPLATE headers it includes and its test to Rules/<System>, Core/<System> and
//      Test/<System>; rename TemplateRule to <System>;
//   2. in sources.cmake use kt1_system(NAMES <System> SOURCES KT1<System>.cpp);
//   3. give KT1_REGISTER_SYSTEM the next free order of the system's phase (KT1Register.h);
//   4. add the option fragment gamedata/kt1/options/<system>.lua and write docs/engine/rules/<system>.md;
//   5. python -I tools/engine/core_tests.py, then python -I tools/engine/kt1_lint.py --fork <tree>.
//
// Replaces:   LuaRules/Gadgets/<gadget>.lua (it runs when this rule is off or in check mode; it calls KT1.StandDown)
// Switch:     mod option kt1_templaterule = 0 (default) / 1 / check
// Config:     gamedata/kt1/templaterule.lua (KT1TemplateRuleConfig.h has the keys, defaults and labels)
// Phases:     FrameStart  tracks units from the events
//             AfterUnits  visits the units due this step
//             FrameEnd    publishes Snapshot<Core::Template::Dot>
// Events:     UnitFinished, UnitDestroyed
// State:      per unit: charge, due step (Core/_TEMPLATE/TemplateRuleCore.h); both in the sync checksum when live
// Display:    none from the sim step; a sound, effect or log line would go through DisplayEvents (never in check mode)
// Allow:      none; a validator is added with AllowChain::Add<CallIn>(NAME, Checking(), order, fn)
// Check:      the contract in KT1Register.h (RuleInfo.checkSafe): the Lua stays authoritative; this rule keeps its own
//             state outside the sync checksum, publishes nothing, and gives Host/Check each value it computes
//             (Check::Native "charge", unit id). The replaced gadget, in check mode, answers for the same visit:
//             Spring.KT1.Check.Answer("TemplateRule", "charge", unitID, 0, 0, charge)
// Timer:      KT1::TemplateRule
// Lua (synced handles only):
//   Spring.KT1.TemplateRule.Charge(unitID)             -> charge, or nothing for an untracked unit
// Rule:       <the rule, step by step, as the requirement states it; cite the requirement id and version>
// Determinism: due ids ascending (DueWheel); events in raise order; float maths in Core in the Lua's order.
// Doc:        game-build docs/engine/rules/<system>.md

#include "KT1TemplateRuleConfig.h"

#include "Game/KT1/Core/Common/Rate.h"
#include "Game/KT1/Core/_TEMPLATE/TemplateRuleCore.h"
#include "Game/KT1/Core/_TEMPLATE/TemplateSnapshot.h"
#include "Game/KT1/Host/Check.h"
#include "Game/KT1/Host/Phases.h"
#include "Game/KT1/Host/Snapshot.h"
#include "Game/KT1/Host/SyncState.h"
#include "Game/KT1/KT1Register.h"
#include "Game/KT1/KT1Registry.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Sim/Misc/GlobalConstants.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"

namespace KT1 {
namespace {

using Core::SimHost::Event;
using Core::SimHost::EventType;
using Core::SimHost::MaskOf;
using Core::SimHost::Phase;

constexpr const char* NAME = "TemplateRule";

class TemplateRule : public Rule {
public:
	// the last two: on by default (no), honours the check-mode contract (yes)
	TemplateRule() : Rule({NAME, 1, true, "LuaRules/Gadgets/<gadget>.lua", false, true}) {}

	// game setup, only for an enabled rule: read the config, size the state, register phases and synced columns
	void LoadConfig(const LuaTable& cfg) override {
		config.period_s = cfg.GetFloat("period_s", config.period_s);
		config.gain = cfg.GetFloat("gain", config.gain);
		config.cap = cfg.GetFloat("cap", config.cap);
		params = {Core::Rate::Every(config.period_s), config.gain, config.cap};
		state.Init(MAX_UNITS, params);
		RegisterPhases();
		if (Checking()) return;   // shadow state: not synced state, and nothing for the draw side
		snapshot.Init(MAX_UNITS);
		SnapshotBoard::Get().Publish(snapshot);
		SyncState::Add(NAME, "charge", state.units.index, state.charge);
		SyncState::Add(NAME, "due", state.units.index, state.dueStep);
	}

	void PushLua(lua_State* L) override { LuaPushRawNamedCFunc(L, "Charge", LCharge); }

private:
	bool Checking() const { return SwitchMode() == Mode::Check; }

	void RegisterPhases() {
		const auto mask = MaskOf(EventType::UnitFinished) | MaskOf(EventType::UnitDestroyed);
		Phases::Add(NAME, Phase::FrameStart, 0, mask, [this](const SimContext& c) { Track(c); });
		Phases::Add(NAME, Phase::AfterUnits, 0, 0, [this](const SimContext& c) { Visit(c); });
		if (!Checking()) Phases::Add(NAME, Phase::FrameEnd, 0, 0, [this](const SimContext& c) { Publish(c); });
	}

	// KT1 hot
	void Track(const SimContext& c) {
		c.events.ForEachRaised([this](const Event& e) {
			if (e.type == EventType::UnitFinished) state.Add(e.id, params);
			else state.Remove(e.id);
		});
	}

	// KT1 hot
	void Visit(const SimContext& c) {
		const auto due = state.TakeDue();
		state.Visit(due, c.ids, params);
		if (!Checking()) return;
		for (const int32_t id : due) {
			const Core::DenseIndex::Slot slot = state.units.index.SlotOf(id);
			if (slot != Core::DenseIndex::NONE && c.ids.Contains(id)) Check::Native(NAME, "charge", id, 0, 0, state.charge[slot]);
		}
	}

	// the draw side's copy of this step: publish only what a draw system reads
	// KT1 hot
	void Publish(const SimContext& c) {
		snapshot.Begin(c.frame);
		for (const int32_t id : state.units.index.AscendingIds()) {
			const CUnit* u = unitHandler.GetUnit(id);
			if (u == nullptr || !c.ids.Contains(id)) continue;
			const Core::DenseIndex::Slot slot = state.units.index.SlotOf(id);
			snapshot.Put(id, state.serial[slot], {u->pos.x, u->pos.y, u->pos.z, state.charge[slot]});
		}
		snapshot.Commit();
	}

	// the running rule for a synced caller, else nullptr
	static TemplateRule* Self(lua_State* L) {
		TemplateRule* r = static_cast<TemplateRule*>(Registry::Get().Find(NAME));
		return (r != nullptr && r->Enabled() && CLuaHandle::GetHandleSynced(L)) ? r : nullptr;
	}

	static int LCharge(lua_State* L) {
		TemplateRule* r = Self(L);
		if (r == nullptr) return 0;
		const Core::DenseIndex::Slot slot = r->state.units.index.SlotOf(luaL_checkint(L, 1));
		if (slot == Core::DenseIndex::NONE) return 0;
		lua_pushnumber(L, r->state.charge[slot]);
		return 1;
	}

	TemplateRuleConfig config;
	Core::Template::RuleParams params;
	Core::Template::RuleState state;
	Snapshot<Core::Template::Dot> snapshot;
};

} // namespace

// ORDER: 4990 is only the template's; a copy takes the next free order of its phase (KT1Register.h)
KT1_REGISTER_SYSTEM(TemplateRule, 4990, TemplateRule)

} // namespace KT1
