/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: MinimapUnits (version 1). The minimap's unit layer: every unit the viewer sees as a small silhouette
// of its class, radar contacts as grey shapes, in the world icons' order and colours.
//
// Replaces:   the drawing of LuaUI/Widgets/icon_minimap.lua (WG.iconMinimap.SetEnabled and Enabled become
//             Spring.KT1.MinimapUnits.SetShown and Shown). Stand-down (game side): KT1.StandDown.
// Switch:     mod option kt1_minimapunits = 0 (default) / 1 / check, family kt1_fam_unitdraw. Unsynced: draw only.
//             It draws from the icon set: with the system Icons off, it draws nothing and the engine's icons stay.
// Config:     the icon set's, data/kt1/icons.json ("minimap": sizes and refresh_hz)
// Reads:      DrawView; the icon set's creation order, classes and HUD options (Draw/StrategicIcons/IconSet.h)
// Writes:     this session's SupComMiniMapUnitIcons (0 while it draws) and MiniMapRefreshRate (refresh_hz, or mod
//             option mm_rate; 0: the engine's choice); both given back when the game ends
// Draws:      MiniMap, before the Lua DrawInMiniMap: made anew each time the engine redraws the minimap
// Lua:        Spring.KT1.MinimapUnits.SetShown(on), Shown(), Count()
// Check:      the GPU buffer is read back each draw and the differing instances counted
// Timer:      KT1::MinimapUnits
// Doc:        game-build docs/engine/rules/iconlexicon.md

#include <cstdlib>
#include <string>
#include <vector>

#include "MinimapLayer.h"
#include "MinimapPainter.h"

#include "Game/GameSetup.h"
#include "Game/KT1/Draw/StrategicIcons/IconSet.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/float3.h"

namespace KT1 {
namespace {

namespace IL = Core::IconLexicon;

constexpr const char* NAME = "MinimapUnits";
constexpr const char* ENGINE_ICONS = "SupComMiniMapUnitIcons";
constexpr const char* REFRESH = "MiniMapRefreshRate";

class MinimapUnitsRule;
MinimapUnitsRule* current = nullptr;
bool shownByLua = true;

class MinimapUnitsRule : public Rule, public DrawSystem {
public:
	// the last two: on by default (no), honours the check-mode contract (yes)
	MinimapUnitsRule() : Rule({NAME, 1, false, "LuaUI/Widgets/icon_minimap.lua", false, true}) {}
	~MinimapUnitsRule() override {
		current = nullptr;
		shownByLua = true;
	}

	// Rule
	void Start() override {
		current = this;
		DrawPhases::Add(this);
	}
	void PushLua(lua_State* L) override;

	// DrawSystem
	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::MiniMap); }
	int Order() const override { return 0; }
	bool Active() const override { return Enabled(); }

	void Draw(const DrawContext& ctx) override {
		const IL::HudOptions& options = Icons::Options();
		const bool wanted = Icons::Running() && shownByLua && !options.hidden && !options.minimapHidden;
		if (!wanted || ctx.miniMapW <= 0 || ctx.miniMapH <= 0 || !InitGL()) {
			EngineIcons(true);
			return;
		}
		SetRefresh();
		const IL::MinimapView view{float3::maxxpos + 1.0f, float3::maxzpos + 1.0f, static_cast<float>(ctx.miniMapW),
		                           static_cast<float>(ctx.miniMapH)};
		Build(ctx.view, view);
		painter.Draw(state, Icons::Config(), view, SwitchMode() == Mode::Check);
		EngineIcons(false);
	}

	void Kill() override {
		if (SwitchMode() == Mode::Check && painter.Stats().draws > 0)
			LOG("[KT1] %s: %i draws, %i GPU differences", NAME, painter.Stats().draws, painter.Stats().differences);
		EngineIcons(true);
		if (rateSet) configHandler->Set(REFRESH, rateWas, true);
		rateSet = false;
		painter.Kill();
		failed = false;
	}

	const IL::MinimapState& State() const { return state; }

private:
	// built at the first draw; a failure is logged once
	bool InitGL() {
		if (painter.Valid()) return true;
		if (failed) return false;
		const IL::IconConfig& config = Icons::Config();
		failed = !painter.Init(config);
		if (failed) {
			LOG_L(L_WARNING, "[KT1] %s off: %s", NAME, painter.Why().c_str());
			return false;
		}
		const int units = config.view.max_instances;
		state.Init(units);
		facts.reserve(units);
		return true;
	}

	// KT1 hot
	void Build(const DrawView& from, const IL::MinimapView& view) {
		const ViewerView who = from.Viewer();
		paints.resize(from.TeamCount());
		for (int team = 0; team < static_cast<int>(paints.size()); ++team)
			paints[team] = Icons::TeamPaint(from, who, team, true);
		facts.clear();
		IL::IconUnitFacts unit;
		for (const int id : from.UnitIds()) {
			if (Icons::ReadUnit(from, who, id, unit)) facts.push_back(unit);
		}
		IL::BuildMinimap(state, Icons::Config(), Icons::Set(), facts, Icons::Classes(), paints, view);
	}

	// the engine's own minimap icons: transparent quads while this layer draws, so their pass is skipped
	void EngineIcons(bool on) {
		if (on == engineIcons) return;
		engineIcons = on;
		configHandler->Set(ENGINE_ICONS, on ? 1 : 0, true);   // true: this session only, never saved
	}

	// the minimap's redraw rate, once: the owner accepts a few frames a second (2026-11-21, presentation only)
	void SetRefresh() {
		if (rateRead) return;
		rateRead = true;
		int rate = Icons::Config().minimap.refresh_hz;
		const auto& options = CGameSetup::GetModOptions();
		const auto it = options.find("mm_rate");
		if (it != options.end()) rate = static_cast<int>(std::atof(it->second.c_str()));
		if (rate <= 0) return;
		rateWas = configHandler->GetIntSafe(REFRESH, 0);
		configHandler->Set(REFRESH, rate, true);
		rateSet = true;
	}

	IL::MinimapState state;
	std::vector<IL::IconUnitFacts> facts;
	std::vector<IL::Paint> paints;
	MinimapUnits::MinimapPainter painter;
	bool failed = false;
	bool engineIcons = true;
	bool rateRead = false, rateSet = false;
	int rateWas = 0;
};

int LSetShown(lua_State* L) {
	MinimapUnits::SetShown(lua_toboolean(L, 1));
	return 0;
}

int LShown(lua_State* L) {
	lua_pushboolean(L, MinimapUnits::Shown());
	return 1;
}

int LCount(lua_State* L) {
	lua_pushnumber(L, static_cast<int>(MinimapUnits::State().instances.size()));
	return 1;
}

void MinimapUnitsRule::PushLua(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return;
	LuaPushRawNamedCFunc(L, "SetShown", LSetShown);
	LuaPushRawNamedCFunc(L, "Shown", LShown);
	LuaPushRawNamedCFunc(L, "Count", LCount);
}

} // namespace

namespace MinimapUnits {

const IL::MinimapState& State() {
	static const IL::MinimapState none;
	return current != nullptr ? current->State() : none;
}

void SetShown(bool shown) { shownByLua = shown; }
bool Shown() { return current != nullptr && shownByLua; }

} // namespace MinimapUnits

KT1_REGISTER_SYSTEM(MinimapUnits, 6500, MinimapUnitsRule)

} // namespace KT1
