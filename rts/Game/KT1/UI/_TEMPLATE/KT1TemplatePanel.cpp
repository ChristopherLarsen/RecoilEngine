/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 interface system: TemplatePanel (version 1). <One sentence: what the panel shows.>
// The template of an interface system (LUA-FREE F09): compiled, never registered (sources.cmake). To copy it:
//   1. copy UI/_TEMPLATE, the Core/_TEMPLATE header it includes and its test to UI/<System>, Core/<System> and
//      Test/<System>; rename TemplatePanel to <System>;
//   2. in sources.cmake use kt1_system(NAMES <System> SOURCES KT1<System>.cpp);
//   3. give KT1_REGISTER_SYSTEM the next free order of the interface range (KT1Register.h);
//   4. add the option fragment gamedata/kt1/options/<system>.lua and write docs/engine/rules/<system>.md.
// The panel keeps a model and rebuilds it only when its input changes. Putting the model on screen is the native
// interface host's job (U01, RmlUi data models): this template has no picture. A click leaves a panel as an order sent
// as a net command, never as a write to the sim.
//
// Replaces:   <widget>.lua, which calls KT1.StandDown(widget, "TemplatePanel", handler)
// Switch:     mod option kt1_templatepanel = 0 (default) / 1 / check. Unsynced.
// Config:     gamedata/kt1/templatepanel.lua (KT1TemplatePanelConfig.h has the keys, defaults and labels)
// Reads:      DrawView and FrameFacts only. No Sim/ include; no pointer kept.
// Updates:    Screen phase, when the selection's change counter moved
// Check:      check mode rebuilds every frame and counts frames whose rows differ from the kept model's
// Timer:      KT1::TemplatePanel
// Doc:        game-build docs/engine/rules/<system>.md

#include "KT1TemplatePanelConfig.h"

#include "Game/KT1/Core/_TEMPLATE/TemplatePanelCore.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaParser.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

constexpr const char* NAME = "TemplatePanel";

class TemplatePanel : public Rule, public DrawSystem {
public:
	TemplatePanel() : Rule({NAME, 1, false, "<the widget it replaces>", false}) {}

	// Rule
	void LoadConfig(const LuaTable& cfg) override { config.max_rows = cfg.GetInt("max_rows", config.max_rows); }
	void Start() override { DrawPhases::Add(this); }

	// DrawSystem
	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::Screen); }
	bool Active() const override { return Enabled(); }
	void Resumed() override { model.Forget(); }

	void Draw(const DrawContext& ctx) override {
		if (!ready) {
			model.Init(ctx.view.UnitCapacity(), config.max_rows);
			fresh.Init(ctx.view.UnitCapacity(), config.max_rows);
			ready = true;
		}
		const uint32_t changes = ctx.facts.SelectionChanges();
		if (model.Stale(changes)) Rebuild(ctx, model);
		else if (SwitchMode() == Mode::Check) Compare(ctx);
	}

	void Kill() override {
		if (SwitchMode() == Mode::Check) LOG("[KT1] %s: %i frames compared, %i differ", NAME, compared, differed);
		ready = false;
		compared = differed = 0;
	}

private:
	// KT1 hot
	static void Rebuild(const DrawContext& ctx, Core::Template::PanelModel& into) {
		into.DefIds().clear();
		UnitView u;
		for (const int id : ctx.facts.Selected()) {
			if (ctx.view.Unit(id, u)) into.DefIds().push_back(u.defId);
		}
		into.Rebuild(ctx.facts.SelectionChanges());
	}

	void Compare(const DrawContext& ctx) {
		Rebuild(ctx, fresh);
		++compared;
		if (fresh.Rows() != model.Rows()) ++differed;
	}

	TemplatePanelConfig config;
	Core::Template::PanelModel model;
	Core::Template::PanelModel fresh;   // check mode only
	int compared = 0;
	int differed = 0;
	bool ready = false;
};

} // namespace

// ORDER: 8490 is only the template's; a copy takes the next free order of the interface range (KT1Register.h)
KT1_REGISTER_SYSTEM(TemplatePanel, 8490, TemplatePanel)

} // namespace KT1
