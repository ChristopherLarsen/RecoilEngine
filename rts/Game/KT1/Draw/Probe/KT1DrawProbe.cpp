/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw system: DrawProbe (version 1). The draw host's test system: one camera-facing quad on every unit in view,
// through InstanceBatch; the hovered unit white, selected units yellow, others by team.
//
// Replaces:   nothing (a test system, game-build docs/engine/rules/drawhost.md).
// Switch:     mod option kt1_drawprobe = 0 (default) / 1 / check. Check: the GPU buffer is read back each frame and
//             differing instances are counted. Spring.KT1.SetEnabled("DrawProbe", on) switches it inside a run.
//             Unsynced: draw only.
// Reads:      DrawView and FrameFacts only.
// Timer:      KT1::DrawProbe
// Config:     gamedata/kt1/drawprobe.lua: half_size; hovered, selected and teams[1..4] as {r, g, b, a}, 0..255
// Lua (unsynced handles only):
//   Spring.KT1.DrawProbe.Facts()            -> the frame facts: { frame, step, stepFraction, zoomLevel, zoomDist,
//                                              pointer = { x, y, kind, unit, feature, ground = {x,y,z}, overMiniMap },
//                                              selected = {ids}, selectionChanges, visible = {ids} }
//   Spring.KT1.DrawProbe.SetPointer(x, y)   read the pointer there instead of the mouse; no arguments: the mouse again
//   Spring.KT1.DrawProbe.Stats()            -> { frames, instances, visible, uploaded, ranges, differences }
//   Spring.KT1.DrawProbe.Audit(on)          the host's audit of itself (Host/HostAudit.h) on (counts at 0) or off
//   Spring.KT1.DrawProbe.Audit()            -> { read, idle = { calls, primitives, stateChanges, uncounted }, busy = {...} }

#include <algorithm>
#include <cstdint>

#include "Game/KT1/Draw/Common/InstanceBatch.h"
#include "Game/KT1/Host/DrawPhases.h"
#include "Game/KT1/Host/HostAudit.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "System/Log/ILog.h"

namespace KT1 {
namespace {

struct ProbeInstance {
	float x, y, z, halfSize;
	uint8_t rgba[4];
};

constexpr int CHANNELS = 4;         // DESIGN: r, g, b, a
constexpr int CHANNEL_MAX = 255;    // DESIGN: a byte
constexpr int TEAM_COLOURS = 4;     // DESIGN: teams beyond these take the colours again
using Colour = uint8_t[CHANNELS];

struct ProbeConfig {
	float halfSize = 0.5f;              // DESIGN: quad half size as a share of the unit's radius
	Colour hovered = {255, 255, 255, 220};   // DESIGN
	Colour selected = {255, 220, 40, 200};   // DESIGN
	Colour team[TEAM_COLOURS] = {{60, 140, 255, 160}, {255, 80, 60, 160}, {80, 220, 100, 160}, {200, 110, 255, 160}};   // DESIGN
};

// {r, g, b, a}; a channel the table leaves out keeps its default
void ReadColour(const LuaTable& t, Colour& out) {
	for (int i = 0; i < CHANNELS; i++)
		out[i] = static_cast<uint8_t>(std::clamp(t.GetInt(i + 1, out[i]), 0, CHANNEL_MAX));
}

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

void PushIds(lua_State* L, const char* key, const std::vector<int>& ids) {
	lua_pushstring(L, key);
	lua_createtable(L, static_cast<int>(ids.size()), 0);
	for (size_t i = 0; i < ids.size(); i++) {
		lua_pushnumber(L, ids[i]);
		lua_rawseti(L, -2, static_cast<int>(i + 1));
	}
	lua_rawset(L, -3);
}

void PushPointer(lua_State* L, const PointerFacts& p) {
	static const char* const kinds[] = {"none", "sky", "ground", "unit", "feature"};
	lua_pushliteral(L, "pointer");
	lua_createtable(L, 0, 7);
	LuaPushNamedNumber(L, "x", p.x);
	LuaPushNamedNumber(L, "y", p.y);
	LuaPushNamedString(L, "kind", kinds[p.hit.kind]);
	if (p.hit.unitId >= 0) LuaPushNamedNumber(L, "unit", p.hit.unitId);
	if (p.hit.featureId >= 0) LuaPushNamedNumber(L, "feature", p.hit.featureId);
	LuaPushNamedBool(L, "overMiniMap", p.overMiniMap);
	if (p.hit.hasGround) {
		lua_pushliteral(L, "ground");
		lua_createtable(L, 3, 0);
		for (int i = 0; i < 3; i++) {   // DESIGN: x, y, z
			lua_pushnumber(L, p.hit.ground[i]);
			lua_rawseti(L, -2, i + 1);
		}
		lua_rawset(L, -3);
	}
	lua_rawset(L, -3);
}

class DrawProbe : public Rule, public DrawSystem {
public:
	// checkSafe: check only compares the GPU buffer with the CPU store; it reads DrawView and FrameFacts and writes no
	// engine state
	DrawProbe() : Rule({"DrawProbe", 1, false, "nothing (draw host test system)", false, true}) {}

	~DrawProbe() override;

	// Rule
	void LoadConfig(const LuaTable& cfg) override {
		config.halfSize = cfg.GetFloat("half_size", config.halfSize);
		ReadColour(cfg.SubTable("hovered"), config.hovered);
		ReadColour(cfg.SubTable("selected"), config.selected);
		const LuaTable teams = cfg.SubTable("teams");
		for (int t = 0; t < TEAM_COLOURS; t++) ReadColour(teams.SubTable(t + 1), config.team[t]);
	}
	void Start() override { DrawPhases::Add(this); }
	void PushLua(lua_State* L) override;

	// DrawSystem
	const char* Name() const override { return "DrawProbe"; }
	unsigned Phases() const override { return PhaseBit(DrawPhase::WorldAlpha); }
	bool Active() const override { return Enabled(); }
	void Resumed() override { batch.Instances().MarkAll(); }

	void Draw(const DrawContext& ctx) override {
		if (!InitGL(ctx.view.UnitCapacity())) return;
		Collect(ctx);
		frames++;
		visibleCount = static_cast<int>(ctx.facts.VisibleUnits().size());
		const CameraFacts& cam = ctx.facts.Camera();
		using namespace GL::State;
		auto state = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), DepthTest(GL_FALSE),
		                          DepthMask(GL_FALSE), Culling(GL_FALSE));
		shader->Enable();
		shader->SetUniformMatrix4x4("viewProj", false, &cam.viewProj.m[0]);
		shader->SetUniform("camRight", cam.right.x, cam.right.y, cam.right.z);
		shader->SetUniform("camUp", cam.up.x, cam.up.y, cam.up.z);
		batch.Draw(SwitchMode() == Mode::Check);
		shader->Disable();
	}

	void Kill() override {
		if (batch.Stats().draws > 0)
			LOG("[KT1] DrawProbe: %i draws, %i GPU differences", batch.Stats().draws, batch.Stats().differences);
		batch.Kill();
		if (shader != nullptr) shaderHandler->ReleaseProgramObject("[KT1]", "DrawProbe");
		shader = nullptr;
		failed = false;
	}

	static int LuaFacts(lua_State* L);
	static int LuaSetPointer(lua_State* L);
	static int LuaStats(lua_State* L);
	static int LuaAudit(lua_State* L);

private:
	bool InitGL(int unitCapacity) {
		if (batch.Valid()) return true;
		if (failed) return false;
		failed = true;
		shader = shaderHandler->CreateProgramObject("[KT1]", "DrawProbe");
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(VS, "", GL_VERTEX_SHADER));
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(FS, "", GL_FRAGMENT_SHADER));
		shader->Link();
		if (!shader->IsValid()) {
			LOG_L(L_WARNING, "[KT1] DrawProbe shader: %s", shader->GetLog().c_str());
			return false;
		}
		InstanceBatch<ProbeInstance>::Config c;
		c.name = "KT1DrawProbe";
		c.idCapacity = unitCapacity;
		if (!batch.Init(c, {{1, 4, false, offsetof(ProbeInstance, x)}, {2, 4, true, offsetof(ProbeInstance, rgba)}})) return false;   // DESIGN: the shader's layout (vec4, vec4)
		failed = false;
		return true;
	}

	void Collect(const DrawContext& ctx) {   // KT1 hot
		const FrameFacts& facts = ctx.facts;
		InstanceStore<ProbeInstance>& store = batch.Instances();
		store.RemoveUnless([&facts](int id) { return facts.IsVisible(id); });
		UnitView u;
		for (const int id : facts.VisibleUnits()) {
			if (!ctx.view.Unit(id, u)) { store.Remove(id); continue; }
			const uint8_t* c = config.team[u.team % TEAM_COLOURS];
			if (facts.IsSelected(id)) c = config.selected;
			if (facts.Pointer().hit.unitId == id) c = config.hovered;
			store.Set(id, {u.drawMidPos.x, u.drawMidPos.y, u.drawMidPos.z, u.radius * config.halfSize, {c[0], c[1], c[2], c[3]}});
		}
	}

	ProbeConfig config;
	InstanceBatch<ProbeInstance> batch;
	Shader::IProgramObject* shader = nullptr;
	int frames = 0;
	int visibleCount = 0;
	bool failed = false;
};

DrawProbe* probe = nullptr;   // the one instance, for the Lua functions

// the registry destroys the rules between games
DrawProbe::~DrawProbe() {
	if (probe == this) probe = nullptr;
}

int DrawProbe::LuaFacts(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return 0;   // draw-side state never reaches synced code
	const FrameFacts& f = DrawPhases::Facts();
	if (!f.Valid()) return 0;
	lua_createtable(L, 0, 9);
	LuaPushNamedNumber(L, "frame", f.Frame());
	LuaPushNamedNumber(L, "step", f.Step());
	LuaPushNamedNumber(L, "stepFraction", f.StepFraction());
	LuaPushNamedNumber(L, "zoomLevel", f.ZoomLevel());
	LuaPushNamedNumber(L, "zoomDist", f.ZoomDist());
	LuaPushNamedNumber(L, "selectionChanges", f.SelectionChanges());
	PushPointer(L, f.Pointer());
	PushIds(L, "selected", f.Selected());
	PushIds(L, "visible", f.VisibleUnits());
	return 1;
}

int DrawProbe::LuaSetPointer(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return 0;
	if (lua_isnoneornil(L, 1)) DrawPhases::ClearPointer();
	else DrawPhases::SetPointer(luaL_checkint(L, 1), luaL_checkint(L, 2));
	return 0;
}

int DrawProbe::LuaStats(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L) || probe == nullptr) return 0;
	const InstanceBatchStats& s = probe->batch.Stats();
	lua_createtable(L, 0, 6);
	LuaPushNamedNumber(L, "frames", probe->frames);
	LuaPushNamedNumber(L, "instances", s.instances);
	LuaPushNamedNumber(L, "visible", probe->visibleCount);
	LuaPushNamedNumber(L, "uploaded", s.uploaded);
	LuaPushNamedNumber(L, "ranges", s.ranges);
	LuaPushNamedNumber(L, "differences", s.differences);
	return 1;
}

void PushCounts(lua_State* L, const char* key, const HostAuditCounts& c) {
	lua_pushstring(L, key);
	lua_createtable(L, 0, 4);
	LuaPushNamedNumber(L, "calls", c.calls);
	LuaPushNamedNumber(L, "primitives", c.primitives);
	LuaPushNamedNumber(L, "stateChanges", c.stateChanges);
	LuaPushNamedNumber(L, "uncounted", c.uncounted);
	lua_rawset(L, -3);
}

int DrawProbe::LuaAudit(lua_State* L) {
	if (CLuaHandle::GetHandleSynced(L)) return 0;
	if (!lua_isnoneornil(L, 1)) {
		HostAudit::Set(lua_toboolean(L, 1) != 0);
		return 0;
	}
	lua_createtable(L, 0, 3);
	LuaPushNamedBool(L, "read", HostAudit::CanRead());
	PushCounts(L, "idle", HostAudit::Counts(false));
	PushCounts(L, "busy", HostAudit::Counts(true));
	return 1;
}

void DrawProbe::PushLua(lua_State* L) {
	probe = this;
	LuaPushRawNamedCFunc(L, "Facts", LuaFacts);
	LuaPushRawNamedCFunc(L, "SetPointer", LuaSetPointer);
	LuaPushRawNamedCFunc(L, "Stats", LuaStats);
	LuaPushRawNamedCFunc(L, "Audit", LuaAudit);
}

} // namespace

KT1_REGISTER_SYSTEM(DrawProbe, 9000, DrawProbe)

} // namespace KT1
