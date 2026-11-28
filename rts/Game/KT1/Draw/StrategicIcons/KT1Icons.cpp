/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: Icons (version 1). Past the switch every unit the viewer may see is one icon, and below it a radar
// contact is, drawn in creation order by one instanced call.
//
// Replaces:   the drawing of LuaUI/Widgets/icon_lex.lua. Its clicks (box, drag and double-click selection) stay in Lua
//             over Spring.KT1.Icons.HitAt and Rects until INPUT moves. Stand-down (game side): KT1.StandDown.
// Switch:     mod option kt1_icons = 0 (default) / 1 / check, family kt1_fam_unitdraw. Unsynced: draw only.
// Config:     data/kt1/icons.json (Core/IconLexicon/IconConfig.h has the keys, defaults and labels)
// Reads:      DrawView and FrameFacts; Alt from the keyboard (not in the facts); the far view's state (forces icons)
// Writes:     every def's engine icon type becomes a transparent one while it draws (DrawWrites); not given back when
//             it is switched off in a run
// Draws:      ScreenEffects, before the Lua DrawScreenEffects: premultiplied, view pixels
// Not here:   the mask_models and rotate switches of the config (both off in the design) do nothing natively
// Check:      the GPU buffer is read back each frame and the differing instances counted
// Timer:      KT1::Icons
// Doc:        game-build docs/engine/rules/iconlexicon.md

#include <memory>

#include "IconPainter.h"
#include "IconRuntime.h"
#include "IconsBridge.h"

#include "Game/KT1/Data/Config.h"
#include "Game/KT1/KT1Register.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

namespace IL = Core::IconLexicon;

constexpr const char* NAME = "Icons";

class IconsRule : public Rule, public DrawSystem {
public:
	// the last two: on by default (no), honours the check-mode contract (yes)
	IconsRule() : Rule({NAME, 1, false, "LuaUI/Widgets/icon_lex.lua", false, true}) {}
	~IconsRule() override { Icons::SetCurrent(nullptr); }

	// Rule
	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		return Data::ParseInto(text, strictness, config);
	}

	void ConfigReloaded() override {
		if (runtime == nullptr) return;
		runtime->Init(config);
		painter.Kill();
		failed = false;
	}

	void Start() override {
		runtime = std::make_unique<Icons::Runtime>();
		runtime->epoch = spring_gettime();
		runtime->Init(config);
		Icons::SetCurrent(runtime.get());
		using Kind = DrawFeed::Kind;
		DrawFeed::Mask kinds = 0;
		for (const Kind kind: {Kind::UnitCreated, Kind::UnitFinished, Kind::UnitRemoved, Kind::UnitEnteredLos, Kind::UnitLeftLos,
		                       Kind::UnitEnteredRadar, Kind::UnitLeftRadar, Kind::UnitGiven, Kind::UnitCloaked,
		                       Kind::UnitDecloaked, Kind::UnitDamaged, Kind::PlayerChanged})
			kinds |= DrawFeed::Bit(kind);
		DrawFeed::Listen(kinds, [this](const DrawFeed::Event& e) { runtime->OnEvent(e, Enabled()); });
		DrawPhases::Add(this);
		LOG("[KT1] %s: %i unit defs, %i atlas icons", NAME, static_cast<int>(runtime->classes.size()) - 1,
		    static_cast<int>(config.atlas.icons.size()));
	}

	void PushLua(lua_State* L) override { PushIconsBridge(L); }

	// DrawSystem
	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::ScreenEffects); }
	int Order() const override { return 0; }
	bool Active() const override { return Enabled(); }

	void Resumed() override {
		IL::IconMarkAll(runtime->state);
		painter.MarkAll();
	}

	void Draw(const DrawContext& ctx) override {
		Icons::Runtime& r = *runtime;
		if (r.paused || (r.options.hidden && r.options.iconsHideWithUi) || !InitGL()) return;
		r.SwapEngineIcons();
		r.Update(ctx);
		painter.Sync(r.state);
		painter.Draw(ctx.facts, SwitchMode() == Mode::Check);
	}

	void Kill() override {
		if (SwitchMode() == Mode::Check && painter.Stats().draws > 0)
			LOG("[KT1] %s: %i draws, %i GPU differences", NAME, painter.Stats().draws, painter.Stats().differences);
		painter.Kill();
		failed = false;
	}

private:
	// built at the first draw; a failure is logged once, and then nothing is drawn and no engine icon is swapped
	bool InitGL() {
		if (painter.Valid()) return true;
		if (failed) return false;
		failed = !painter.Init(config);
		runtime->failed = failed;
		runtime->why = painter.Why();
		if (failed) LOG_L(L_WARNING, "[KT1] %s off: %s", NAME, painter.Why().c_str());
		return !failed;
	}

	IL::IconConfig config;
	std::unique_ptr<Icons::Runtime> runtime;
	Icons::IconPainter painter;
	bool failed = false;
};

} // namespace

KT1_REGISTER_SYSTEM(Icons, 6400, IconsRule)

} // namespace KT1
