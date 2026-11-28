/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: ViewSettings (version 1). Everything that follows the zoom or a view option by setting an engine
// setting or a unit's or feature's draw mask (LUA-FREE D07, FRAME family).
// Replaces:   LuaUI/Widgets/gfx_draw_zoom_gates, gfx_model_cull, gfx_shadow_zoom, gfx_prop_distance, gfx_wreck_cull,
//             gfx_zoomswitch, gfx_water_far, gfx_water_guard, gfx_water_mode_guard, map_zoom_look (they stand down
//             through LuaUI/lib/viewsettings_bridge.lua)
// Switch:     mod option kt1_viewsettings (unset: kt1_fam_frame, else off). Spring.KT1.SetEnabled switches it inside a
//             run: it gives back what it changed in the next frame's Update, before the Lua widgets run again, and takes
//             over in PreDraw, after they have given theirs back.
// Config:     data/kt1/viewsettings.json
// Reads:      the camera, config settings, mod options; unit and feature facts through Host/ViewDoor
// Draws:      nothing
// Timer:      KT1::ViewSettings
// Doc:        game-build docs/engine/rules/viewsettings.md

#include "ViewObjects.h"
#include "ViewShared.h"
#include "ViewWorld.h"

#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Draw/FarView/KT1FarView.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/KT1Register.h"

namespace KT1 {
namespace ViewSet {
namespace {

constexpr const char* NAME = "ViewSettings";

class ViewSettings : public Rule, public DrawSystem {
public:
	ViewSettings() : Rule({NAME, 1, false, "(LuaUI zoom gate and view setting widgets)", false}) {}

	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		const Data::Report report = Data::ParseInto(text, strictness, Shared().config);
		Shared().loaded = Shared().loaded || report.accepted;
		return report;
	}
	void ConfigReloaded() override { restart = true; }
	void Start() override { DrawPhases::Add(this); }

	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::Update) | PhaseBit(DrawPhase::PreDraw); }
	int Order() const override { return 1; }
	bool Active() const override { return true; }   // called while switched off too: to give back what it changed

	void Draw(const DrawContext& ctx) override {
		if (!Shared().started || Shared().view.first) return;
		if (ctx.phase == DrawPhase::Update) {
			if (running && (!Enabled() || restart)) Stop();
			return;
		}
		if (!Enabled()) return;
		const ViewNow now = ReadCamera();
		if (!running) Begin(now);
		if (options.cullModels) FarView::SetZoom(now.zoomDist);   // FarView switches on the game camera's zoom
		objects.Update(now);
		world.Update(now);
	}

	// the game ends: the engine's objects go with it, nothing is given back; the door stops listening
	void Kill() override {
		ViewDoor::Stop();
		running = false;
	}

private:
	void Begin(const ViewNow& now) {
		if (!ready) {
			ready = true;
			options = ReadOptions();
			objects.Init(options);
			world.Init(options);
		}
		running = true;
		restart = false;
		objects.Begin(now);
		objects.Update(now);                       // the live objects arrive: the longest prop distance is known after
		world.Begin(objects.LongestPropCut(), objects.PropCount());
	}

	void Stop() {
		objects.Release();
		world.Release();
		running = false;
	}

	ViewOptions options;
	ViewObjects objects;
	ViewWorld world;
	bool ready = false, running = false, restart = false;
};

} // namespace
} // namespace ViewSet

using ViewSettingsRule = ViewSet::ViewSettings;
KT1_REGISTER_SYSTEM(ViewSettings, 6410, ViewSettingsRule)

} // namespace KT1
