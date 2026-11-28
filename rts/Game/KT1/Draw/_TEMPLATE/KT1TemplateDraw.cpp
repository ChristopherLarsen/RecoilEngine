/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: TemplateDraw (version 1). <One sentence: what it draws.>
// The template of a draw system (LUA-FREE F09): compiled, never registered (sources.cmake). To copy it:
//   1. copy Draw/_TEMPLATE, the Core/_TEMPLATE headers it includes and its test to Draw/<System>, Core/<System> and
//      Test/<System>; rename TemplateDraw to <System>;
//   2. in sources.cmake use kt1_system(NAMES <System> SOURCES KT1<System>.cpp);
//   3. give KT1_REGISTER_SYSTEM the next free order of the draw range (KT1Register.h);
//   4. in the game add data/kt1/<system>.json and the option fragment gamedata/kt1/options/<system>.lua, and write
//      docs/engine/rules/<system>.md.
//
// Replaces:   the drawing of <gadget or widget>.lua. Stand-down (game side): the replaced Lua calls
//             KT1.StandDown(gadget, "TemplateDraw", handler). With the rule on it is removed; with kt1_capture=1 it
//             stays loaded and its Draw* call-ins return early while Spring.KT1.Enabled("TemplateDraw") is true, so one
//             run can capture the same frame with the switch off, then on (the pixel proof).
// Switch:     mod option kt1_templatedraw = 0 (default) / 1 / check. Spring.KT1.SetEnabled("TemplateDraw", on) switches
//             it inside a run; it draws a correct frame on the first frame after being switched on. Unsynced: draw only.
// Config:     data/kt1/templatedraw.json (TemplateDrawConfig.h has the keys, defaults and labels). Read again when the
//             file changes; every value is used each frame, so nothing is rebuilt. A system that replaces a .lua
//             config adds LoadConfig, which only sets values (KT1Rule.h)
// Reads:      DrawView (Snapshot<Core::Template::Dot>) and FrameFacts only. No Sim/ include; no pointer kept.
// Draws:      WorldAlpha: one camera-facing quad per published unit in view, one instanced draw, only changes uploaded
// Check:      check mode (RuleInfo.checkSafe) reads the GPU buffer back each frame and counts differing instances; it
//             draws beside the Lua, so pixel proofs compare the switch off and on, never check
// Timer:      KT1::TemplateDraw
// Doc:        game-build docs/engine/rules/<system>.md

#include <cstddef>

#include "TemplateDrawConfig.h"

#include "Game/KT1/Core/_TEMPLATE/TemplateDrawCore.h"
#include "Game/KT1/Data/Config.h"
#include "Game/KT1/Draw/Common/InstanceBatch.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/KT1Register.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

constexpr const char* NAME = "TemplateDraw";

// one instance, laid out as the vertex shader reads it
struct DotInstance {
	float x, y, z, halfSize;
	std::array<uint8_t, 4> rgba;
};

const char* const VS = R"(#version 330
layout (location = 0) in vec2 corner;
layout (location = 1) in vec4 posSize;
layout (location = 2) in vec4 color;
uniform mat4 viewProj;
uniform vec3 camRight;
uniform vec3 camUp;
out vec4 vColor;
void main() {
	vColor = color;
	vec3 p = posSize.xyz + (camRight * corner.x + camUp * corner.y) * posSize.w;
	gl_Position = viewProj * vec4(p, 1.0);
}
)";

const char* const FS = R"(#version 330
in vec4 vColor;
out vec4 frag;
void main() { frag = vColor; }
)";

class TemplateDraw : public Rule, public DrawSystem {
public:
	// the last two: on by default (no), honours the check-mode contract (yes)
	TemplateDraw() : Rule({NAME, 1, false, "<the drawing it replaces>", false, true}) {}

	// Rule
	Data::Report ReadConfig(std::string_view text, Data::Strictness strictness) override {
		return Data::ParseInto(text, strictness, config);
	}
	void ConfigValues(Data::Values& v) const override {
		v.Add("half_size", config.halfSize);
		v.Add("full_charge", config.fullCharge);
		v.Add("colour", config.colour.data(), config.colour.size());
	}
	void Start() override { DrawPhases::Add(this); }

	// DrawSystem
	const char* Name() const override { return NAME; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::WorldAlpha); }
	int Order() const override { return 0; }
	bool Active() const override { return Enabled(); }
	void Resumed() override { batch.Instances().MarkAll(); }

	void Draw(const DrawContext& ctx) override {
		const Snapshot<Core::Template::Dot>* dots = ctx.view.Snapshots<Core::Template::Dot>();
		if (dots == nullptr || !dots->Ready() || !InitGL(ctx.view.UnitCapacity())) return;
		Collect(ctx, *dots);
		const CameraFacts& cam = ctx.facts.Camera();
		using namespace GL::State;
		auto state = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), DepthTest(GL_TRUE),
		                          DepthMask(GL_FALSE), Culling(GL_FALSE));
		shader->Enable();
		shader->SetUniformMatrix4x4("viewProj", false, &cam.viewProj.m[0]);
		shader->SetUniform("camRight", cam.right.x, cam.right.y, cam.right.z);
		shader->SetUniform("camUp", cam.up.x, cam.up.y, cam.up.z);
		batch.Draw(SwitchMode() == Mode::Check);
		shader->Disable();
	}

	void Kill() override {
		if (SwitchMode() == Mode::Check && batch.Stats().draws > 0)
			LOG("[KT1] %s: %i draws, %i GPU differences", NAME, batch.Stats().draws, batch.Stats().differences);
		batch.Kill();
		if (shader != nullptr) shaderHandler->ReleaseProgramObject("[KT1]", NAME);
		shader = nullptr;
		failed = false;
	}

private:
	// once per game; a failure is logged once and the system then draws nothing
	bool InitGL(int unitCapacity) {
		if (batch.Valid()) return true;
		if (failed) return false;
		failed = true;
		shader = shaderHandler->CreateProgramObject("[KT1]", NAME);
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(VS, "", GL_VERTEX_SHADER));
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(FS, "", GL_FRAGMENT_SHADER));
		shader->Link();
		if (!shader->IsValid()) {
			LOG_L(L_WARNING, "[KT1] %s shader: %s", NAME, shader->GetLog().c_str());
			return false;
		}
		InstanceBatch<DotInstance>::Config c;
		c.name = "KT1TemplateDraw";
		c.idCapacity = unitCapacity;
		// DESIGN: the instance layout, the shader's locations 1 (position and size) and 2 (colour)
		if (!batch.Init(c, {{1, 4, false, offsetof(DotInstance, x)}, {2, 4, true, offsetof(DotInstance, rgba)}})) return false;
		failed = false;
		return true;
	}

	// KT1 hot
	void Collect(const DrawContext& ctx, const Snapshot<Core::Template::Dot>& dots) {
		const FrameFacts& facts = ctx.facts;
		InstanceStore<DotInstance>& store = batch.Instances();
		store.RemoveUnless([&facts, &dots](int id) { return facts.IsVisible(id) && dots.Current(id) != nullptr; });
		std::array<uint8_t, 4> colour;
		for (size_t i = 0; i < colour.size(); ++i) colour[i] = static_cast<uint8_t>(config.colour[i]);
		for (const int id : facts.VisibleUnits()) {
			const Core::Template::Dot* current = dots.Current(id);
			if (current == nullptr) continue;
			const Core::Template::Dot d = Core::Template::Blend(dots.Previous(id), *current, facts.StepFraction());
			std::array<uint8_t, 4> rgba = colour;
			rgba.back() = Core::Template::Alpha(d.charge, config.fullCharge, rgba.back());
			store.Set(id, {d.x, d.y, d.z, config.halfSize, rgba});
		}
	}

	TemplateDrawConfig config;
	InstanceBatch<DotInstance> batch;
	Shader::IProgramObject* shader = nullptr;
	bool failed = false;
};

} // namespace

// ORDER: 7990 is only the template's; a copy takes the next free order of the draw range (KT1Register.h)
KT1_REGISTER_SYSTEM(TemplateDraw, 7990, TemplateDraw)

} // namespace KT1
