/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: FarView (version 2: the cartographic look of RND-005 R3/R5/R10, the player's toggle, auto switch by config). Far above the map, the world pass draws only a low-resolution map mesh textured with a pre-baked
// map image; strategic icons, order lines, selection and Lua overlays are drawn as before (game-build docs/engine/FAR-VIEW-DESIGN.md).
//
// Why:        at the PERF-BENCH-4K FAR camera (37,683 elmos) the new pin's far plane (98,304) puts the whole map in view and the full
//             world pass costs about 41 ms (terrain at full detail, water with reflections, features, decals, grass, particles).
//             From that height every one of those is a few pixels; a pre-baked map image on a coarse mesh looks the same.
// Replaces:   nothing in Lua; its "Lua" side is the engine's own full world pass when off.
// Switch:     mod option kt1_farview (default off until the bench proves it). Unsynced: draw only.
// Config:     gamedata/kt1/farview.lua: switch_height, hysteresis, grid, refresh_frames, water_level, water_color, brightness,
//             texture (labels and pending values there).
// Hooks:      rts/Rendering/WorldDrawer.cpp, CWorldDrawer::Draw: `if (!KT1::FarView::Draw()) { DrawOpaqueObjects(); DrawAlphaObjects(); }`.
//             Lua DrawWorld, DrawMiscObjects (icons, order lines, selection box, map marks) and DrawBelowWaterOverlay still run.
// Height:     the camera's height above the map's average height, or the game camera's zoom when Lua sets it (SetZoom); the engine
//             falls back to height when Lua has not set it for 30 drawn frames.
// Timer:      KT1::FarView (the far draw: mesh refresh and draw).
// Lua:        Spring.KT1.FarView.Stats()   -> { active, far, height, switchHeight, grid, refreshes, drawMs, frames }
//             Spring.KT1.FarView.SetZoom(d) the game camera's zoom distance this frame (unsynced callers only; nil clears it)
//             Spring.KT1.FarView.SetFogTexture(tex, factor) the vision widget's R16F fog mask (signed elmos to the vision edge, + inside;
//                                           shade = mix(factor, 1, clamp(r/2+0.5))); nil clears (unfogged)
//             Spring.KT1.FarView.SetManual(b)  the player's cartographic toggle (RND-005 R1), at any zoom
//             Spring.KT1.FarView.Force(b)   true: far view at any height; false: never; nil: by height (tests, A/B)
// Determinism: draw only. Reads the unsynced heightmap and the camera; writes no sim state.

#include "KT1FarView.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/SelectedUnitsHandler.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Map/ReadMap.h"
#include "Rendering/DepthBufferCopy.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/GL/VAO.h"
#include "Rendering/GL/VBO.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/Misc/GlobalConstants.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"
#include "System/TimeProfiler.h"

namespace KT1 {
namespace FarView {

namespace {

const char* const VS = R"(#version 330
layout(location = 0) in vec3 pos;
uniform mat4 viewProj;
uniform vec2 mapSize;
out vec2 uv;
out float wy;
void main() {
	uv = pos.xz / mapSize;
	wy = pos.y;
	gl_Position = viewProj * vec4(pos, 1.0);
}
)";

const char* const FS = R"(#version 330
in vec2 uv;
in float wy;
uniform sampler2D tex;
uniform float waterLevel;
uniform vec4 waterColor;   // rgb, a = blend weight below the water level
uniform float brightness;
uniform int carto;         // 1: cartographic look (RND-005 R3, R5): height ramp, contour lines, no water surface, no lighting
uniform vec2 hRange;       // map min and max height
uniform vec3 rampDark, rampLight;
uniform float contourStep; // fraction of the height range per contour step
uniform float edgeGain;
uniform sampler2D fogTex;  // gfx_vision.lua mask: R = signed elmos from the nearest vision edge, + inside
uniform float fogFactor;   // shade outside vision
uniform int hasFog;
out vec4 fragColor;
void main() {
	vec3 c;
	if (carto == 1) {
		float h = clamp((wy - hRange.x) / max(hRange.y - hRange.x, 1.0), 0.0, 1.0);
		c = mix(rampDark, rampLight, h);
		// contour: the original filters the quantised height against its 8 neighbours with a gain; here the same line, found per
		// pixel where the quantised height steps (DESIGN: one pass instead of a full-screen filter)
		float q = h / contourStep;
		float w = max(fwidth(q), 1e-4);
		float line = 1.0 - smoothstep(0.0, w, fract(q));
		c -= vec3(clamp(edgeGain * contourStep * line * 0.5, 0.0, 1.0)) * c;
	} else {
		c = texture(tex, uv).rgb * brightness;
		if (wy < waterLevel) c = mix(c, waterColor.rgb, waterColor.a);
	}
	if (hasFog == 1) {
		float lit = clamp(texture(fogTex, uv).r * 0.5 + 0.5, 0.0, 1.0);
		c *= mix(fogFactor, 1.0, lit);
	}
	fragColor = vec4(c, 1.0);
}
)";

struct Cfg {
	float switchHeight = 4000.0f;   // PLACEHOLDER (pending FAR-ZOOM-MAP-VIEW): camera height (or zoom) where the far view begins
	float hysteresis = 0.05f;       // DESIGN: band below the switch before the full view returns (ZOOM-TIER-PLAN: 5%)
	int grid = 128;                 // DESIGN: mesh cells per map side (about 640 elmos a cell on an 80 km map, 33k triangles)
	int refreshFrames = 30;         // DESIGN: drawn frames between mesh height refreshes while far
	float waterLevel = 0.0f;        // DESIGN: the engine's water plane
	float waterColor[4] = {0.10f, 0.22f, 0.32f, 0.0f};   // PLACEHOLDER (pending): tint below water; a = 0 keeps the image's water
	float brightness = 1.0f;        // PLACEHOLDER (pending): multiplier on the baked image
	std::string texture;            // DESIGN: "" = the map's minimap image; else a game-archive image (a higher-resolution bake)
	bool carto = true;              // REQ (RND-005 R10): the build's far view takes the cartographic look; false = the baked image
	bool autoSwitch = true;         // DESIGN deviation (RND-005 R1, R10): switch by zoom; the default waits on the owner
	float rampDark[3] = {0.45f, 0.38f, 0.41f};    // PARITY: G0-FARVIEW cartographic_terrain.builtin_ramp_dark_rgb
	float rampLight[3] = {0.80f, 0.80f, 0.68f};   // PARITY: G0-FARVIEW cartographic_terrain.builtin_ramp_light_rgb
	float contourStep = 0.10f;      // PARITY: G0-FARVIEW cartographic_terrain.builtin_contour_step_percent (10%)
	float edgeGain = 16.0f;         // PARITY: G0-FARVIEW cartographic_terrain.contour_edge_gain
};

Cfg cfg;
bool active = false;       // the rule is on (mod option)
bool farNow = false;       // this frame is drawn far
int force = -1;            // -1 by height, 0 never, 1 always (tests)
bool manual = false;       // the player's cartographic toggle (RND-005 R1: a key action and a map option)
float luaZoom = -1.0f;     // the game camera's zoom, when Lua sets it
int luaZoomAge = 1000;     // drawn frames since Lua last set it
float lastHeight = 0.0f;
uint32_t fogTex = 0;       // the vision widget's fog mask (SetFogTexture); 0 = unfogged
float fogFactor = 1.0f;

Shader::IProgramObject* shader = nullptr;
bool glTried = false;
VBO vbo, ibo;
VAO vao;
GLsizei indexCount = 0;
uint32_t ownTex = 0;
int sinceRefresh = 1 << 20;
int stRefreshes = 0, stFrames = 0;
double stDrawMs = 0.0;

bool InitGL() {
	if (glTried) return shader != nullptr;
	glTried = true;
	shader = shaderHandler->CreateProgramObject("[KT1]", "KT1 FarView");
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(VS, "", GL_VERTEX_SHADER));
	shader->AttachShaderObject(shaderHandler->CreateShaderObject(FS, "", GL_FRAGMENT_SHADER));
	shader->Link();
	if (!shader->IsValid()) { LOG_L(L_WARNING, "[KT1] FarView shader: %s; far view off", shader->GetLog().c_str()); shader = nullptr; return false; }
	shader->Enable(); shader->SetUniform("tex", 0); shader->SetUniform("fogTex", 1); shader->Disable();

	if (!cfg.texture.empty()) {
		CBitmap bmp;
		if (bmp.Load(cfg.texture)) {
			GL::TextureCreationParams tcp;
			tcp.reqNumLevels = 0;   // full mip chain: the whole map is minified a lot
			ownTex = bmp.CreateTexture(tcp);
		} else {
			LOG_L(L_WARNING, "[KT1] FarView: texture %s not found; using the minimap image", cfg.texture.c_str());
		}
	}

	const int n = cfg.grid;
	std::vector<uint32_t> idx;
	idx.reserve((size_t)n * n * 6);
	for (int z = 0; z < n; ++z) {
		for (int x = 0; x < n; ++x) {
			const uint32_t a = z * (n + 1) + x, b = a + 1, c = a + (n + 1), d = c + 1;
			idx.insert(idx.end(), {a, c, b, b, c, d});
		}
	}
	indexCount = (GLsizei)idx.size();
	vbo.Bind(GL_ARRAY_BUFFER);
	vbo.New(sizeof(float) * 3 * (n + 1) * (n + 1), GL_DYNAMIC_DRAW, nullptr);
	vbo.Unbind();
	ibo.Bind(GL_ELEMENT_ARRAY_BUFFER);
	ibo.New(sizeof(uint32_t) * idx.size(), GL_STATIC_DRAW, idx.data());
	ibo.Unbind();

	vao.Bind();
	vbo.Bind(GL_ARRAY_BUFFER);
	ibo.Bind(GL_ELEMENT_ARRAY_BUFFER);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	vao.Unbind();
	vbo.Unbind();
	ibo.Unbind();
	return true;
}

// Point samples of the unsynced corner heightmap at the grid's vertices: (grid+1)^2 reads.
void RefreshMesh() {
	const int n = cfg.grid;
	const float* hm = readMap->GetCornerHeightMapUnsynced();
	const int W = mapDims.mapxp1, H = mapDims.mapyp1;
	std::vector<float> v;
	v.reserve((size_t)(n + 1) * (n + 1) * 3);
	for (int z = 0; z <= n; ++z) {
		const int hz = std::min(H - 1, (int)std::lround((float)z * (H - 1) / n));
		for (int x = 0; x <= n; ++x) {
			const int hx = std::min(W - 1, (int)std::lround((float)x * (W - 1) / n));
			v.push_back((float)(hx * SQUARE_SIZE));
			v.push_back(hm[hz * W + hx]);
			v.push_back((float)(hz * SQUARE_SIZE));
		}
	}
	vbo.Bind(GL_ARRAY_BUFFER);
	vbo.SetBufferSubData(0, sizeof(float) * v.size(), v.data());
	vbo.Unbind();
	sinceRefresh = 0;
	stRefreshes += 1;
}

bool DecideFar() {
	luaZoomAge += 1;
	const float h = (luaZoomAge <= 30 && luaZoom >= 0.0f) ? luaZoom : camera->GetPos().y - readMap->GetCurrAvgHeight();
	lastHeight = h;
	if (force >= 0) return force == 1;
	if (manual) return true;
	if (!cfg.autoSwitch) return false;
	const float on = cfg.switchHeight, off = cfg.switchHeight * (1.0f - cfg.hysteresis);
	return farNow ? (h >= off) : (h >= on);
}

void DrawMesh() {
	const uint32_t tex = ownTex != 0 ? ownTex : readMap->GetMiniMapTexture();
	if (sinceRefresh >= cfg.refreshFrames) RefreshMesh();
	sinceRefresh += 1;

	using namespace GL::State;
	auto st = GL::SubState(Blending(GL_FALSE), DepthTest(GL_TRUE), DepthMask(GL_TRUE), Culling(GL_FALSE));
	shader->Enable();
	shader->SetUniformMatrix4x4("viewProj", false, &camera->GetViewProjectionMatrix().m[0]);
	shader->SetUniform("mapSize", (float)(mapDims.mapx * SQUARE_SIZE), (float)(mapDims.mapy * SQUARE_SIZE));
	shader->SetUniform("waterLevel", cfg.waterLevel);
	shader->SetUniform("waterColor", cfg.waterColor[0], cfg.waterColor[1], cfg.waterColor[2], cfg.waterColor[3]);
	shader->SetUniform("brightness", cfg.brightness);
	shader->SetUniform("carto", cfg.carto ? 1 : 0);
	shader->SetUniform("hRange", readMap->GetCurrMinHeight(), readMap->GetCurrMaxHeight());
	shader->SetUniform("rampDark", cfg.rampDark[0], cfg.rampDark[1], cfg.rampDark[2]);
	shader->SetUniform("rampLight", cfg.rampLight[0], cfg.rampLight[1], cfg.rampLight[2]);
	shader->SetUniform("contourStep", cfg.contourStep);
	shader->SetUniform("edgeGain", cfg.edgeGain);
	const bool fog = fogTex != 0 && glIsTexture(fogTex);
	shader->SetUniform("hasFog", fog ? 1 : 0);
	shader->SetUniform("fogFactor", fogFactor);
	if (fog) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, fogTex); }
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	vao.Bind();
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
	vao.Unbind();
	glBindTexture(GL_TEXTURE_2D, 0);
	if (fog) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); glActiveTexture(GL_TEXTURE0); }
	shader->Disable();
}

class FarViewRule : public Rule {
public:
	FarViewRule() : Rule({"FarView", 2, false, "(engine world pass at full detail)", false}) {}

	void LoadConfig(const LuaTable& t) override {
		cfg.switchHeight = t.GetFloat("switch_height", cfg.switchHeight);
		cfg.hysteresis = std::clamp(t.GetFloat("hysteresis", cfg.hysteresis), 0.0f, 0.5f);
		cfg.grid = std::clamp(t.GetInt("grid", cfg.grid), 8, 512);
		cfg.refreshFrames = std::max(1, t.GetInt("refresh_frames", cfg.refreshFrames));
		cfg.waterLevel = t.GetFloat("water_level", cfg.waterLevel);
		const LuaTable wc = t.SubTable("water_color");
		for (int i = 0; i < 4; ++i) cfg.waterColor[i] = wc.GetFloat(i + 1, cfg.waterColor[i]);
		cfg.brightness = t.GetFloat("brightness", cfg.brightness);
		cfg.texture = t.GetString("texture", cfg.texture);
		cfg.carto = t.GetBool("cartographic", cfg.carto);
		cfg.autoSwitch = t.GetBool("auto_switch", cfg.autoSwitch);
		const LuaTable rd = t.SubTable("ramp_dark"), rl = t.SubTable("ramp_light");
		for (int i = 0; i < 3; ++i) { cfg.rampDark[i] = rd.GetFloat(i + 1, cfg.rampDark[i]); cfg.rampLight[i] = rl.GetFloat(i + 1, cfg.rampLight[i]); }
		cfg.contourStep = std::max(0.001f, t.GetFloat("contour_step", cfg.contourStep));
		cfg.edgeGain = t.GetFloat("contour_edge_gain", cfg.edgeGain);
	}
	void Start() override {
		active = true;
		LOG("[KT1] FarView: on; switch at %.0f elmos, grid %d", cfg.switchHeight, cfg.grid);
	}
	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Stats", LStats);
		LuaPushRawNamedCFunc(L, "SetZoom", LSetZoom);
		LuaPushRawNamedCFunc(L, "Force", LForce);
		LuaPushRawNamedCFunc(L, "SetManual", LSetManual);
		LuaPushRawNamedCFunc(L, "SetFogTexture", LSetFogTexture);
	}

private:
	static int LStats(lua_State* L) {
		lua_newtable(L);
		LuaPushNamedBool(L, "active", active);
		LuaPushNamedBool(L, "far", farNow);
		LuaPushNamedBool(L, "manual", manual);
		LuaPushNamedBool(L, "autoSwitch", cfg.autoSwitch);
		LuaPushNamedBool(L, "cartographic", cfg.carto);
		LuaPushNamedNumber(L, "height", lastHeight);
		LuaPushNamedNumber(L, "switchHeight", cfg.switchHeight);
		LuaPushNamedNumber(L, "grid", cfg.grid);
		LuaPushNamedNumber(L, "refreshes", stRefreshes);
		LuaPushNamedNumber(L, "drawMs", stDrawMs);
		LuaPushNamedNumber(L, "frames", stFrames);
		return 1;
	}
	static int LSetZoom(lua_State* L) {
		if (CLuaHandle::GetHandleSynced(L)) return 0;
		if (lua_isnumber(L, 1)) { luaZoom = (float)lua_tonumber(L, 1); luaZoomAge = 0; }
		else { luaZoom = -1.0f; }
		return 0;
	}
	static int LSetFogTexture(lua_State* L) {
		if (CLuaHandle::GetHandleSynced(L)) return 0;
		fogTex = lua_isnumber(L, 1) ? (uint32_t)lua_tointeger(L, 1) : 0;
		fogFactor = lua_isnumber(L, 2) ? (float)lua_tonumber(L, 2) : 1.0f;
		return 0;
	}
	static int LSetManual(lua_State* L) {
		if (CLuaHandle::GetHandleSynced(L)) return 0;
		manual = lua_toboolean(L, 1);
		return 0;
	}
	static int LForce(lua_State* L) {
		if (CLuaHandle::GetHandleSynced(L)) return 0;
		force = lua_isboolean(L, 1) ? (lua_toboolean(L, 1) ? 1 : 0) : -1;
		return 0;
	}
};

} // namespace

bool Draw() {
	if (!active) return false;
	farNow = DecideFar();
	if (!farNow) return false;
	if (!InitGL()) { active = false; return false; }

	SCOPED_TIMER("KT1::FarView");
	const spring_time t0 = spring_gettime();
	DrawMesh();
	depthBufferCopy->MakeDepthBufferCopy();   // Lua DrawWorld widgets that read the depth copy keep working

	// Kept from the full opaque pass besides terrain and models: selection and Lua's pre-unit layer.
	selectedUnitsHandler.Draw();
	eventHandler.DrawWorldPreUnit();

	stDrawMs += (spring_gettime() - t0).toMilliSecsf();
	stFrames += 1;
	return true;
}

} // namespace FarView

std::unique_ptr<Rule> MakeFarView() { return std::make_unique<FarView::FarViewRule>(); }

} // namespace KT1
