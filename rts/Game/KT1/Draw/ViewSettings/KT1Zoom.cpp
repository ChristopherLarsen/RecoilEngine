/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: Zoom (version 1). The zoom level and the zoom gates, for every draw system and the widgets that
// still run (LUA-FREE D07, FRAME §4.3). It writes no engine setting and no draw mask: ViewSettings does that.
// Replaces:   the level of LuaUI/Widgets/api_zoom_level.lua, which stays as a forwarder until its readers move
// Switch:     mod option kt1_zoom, default on
// Config:     data/kt1/viewsettings.json (shared with ViewSettings)
// Timer:      KT1::Zoom
// Doc:        game-build docs/engine/rules/viewsettings.md

#include "ViewNow.h"
#include "ViewShared.h"
#include "ZoomBridge.h"

#include "Game/Camera.h"
#include "Game/KT1/Data/DataEngine.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/KT1Register.h"
#include "Map/ReadMap.h"
#include "Rendering/SupComZoomLevel.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace ViewSet {

ViewShared& Shared() {
	static ViewShared shared;
	return shared;
}

namespace {

namespace View = Core::View;

constexpr const char* NAME = "Zoom";

bool WholeMapInView() {
	const float x = static_cast<float>(mapDims.mapx * SQUARE_SIZE), z = static_cast<float>(mapDims.mapy * SQUARE_SIZE);
	return camera->InView(float3(0, 0, 0)) && camera->InView(float3(x, 0, 0)) &&
	       camera->InView(float3(0, 0, z)) && camera->InView(float3(x, 0, z));
}

class Zoom : public Rule, public DrawSystem {
public:
	Zoom() : Rule({NAME, 1, false, "(LuaUI api_zoom_level: the level)", true}) { Shared() = ViewShared{}; }

	// no file of its own: the shared one is read by ViewSettings, or at Start when that is off
	void Start() override {
		ViewShared& s = Shared();
		if (!s.loaded) s.config = Data::Load<View::ViewSettingsConfig>("viewsettings");
		s.loaded = true;
		Compile();
		DrawPhases::Add(this);
	}
	void PushLua(lua_State* L) override { PushZoomBridge(L); }

	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::Update); }
	int Order() const override { return 0; }
	bool Active() const override { return Enabled(); }

	void Draw(const DrawContext&) override {
		ViewShared& s = Shared();
		const ViewNow now = ReadCamera();
		View::ViewInputs in;
		in.distance = now.zoomDist;
		in.height = now.dist;
		in.engineLevel = SupComZoomLevel::Get();
		in.wholeMap = View::WholeMapCheckNeeded(s.config.bounds, now.dist, in.engineLevel) && WholeMapInView();
		const View::DetailConfig& d = s.config.detail;
		const int value = configHandler->IsSet(d.setting) ? configHandler->GetInt(d.setting) : d.setting_default;
		in.detail = View::DetailIndex(d, value);
		s.view.log.Clear();                       // the writes of one frame
		View::UpdateView(s.view, s.config, in);
	}

private:
	void Compile() {
		ViewShared& s = Shared();
		s.view = View::ViewState{};
		if (!View::CompileRows(s.config, s.view.table))
			LOG_L(L_WARNING, "[KT1] %s: bad rows in viewsettings.json (more than %d, or an unknown unit)", NAME, View::MAX_GATES);
		for (int i = 0; i < s.view.table.count; ++i) {
			const std::string& option = s.config.rows[i].option;
			if (!option.empty() && ModOption(option.c_str()) == "0") s.view.disabled |= uint64_t(1) << i;
		}
		s.started = true;
	}
};

} // namespace
} // namespace ViewSet

// ORDER: first of the view systems; the others read its level
using ViewZoomRule = ViewSet::Zoom;
KT1_REGISTER_SYSTEM(Zoom, 6400, ViewZoomRule)

} // namespace KT1
