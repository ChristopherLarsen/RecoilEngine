/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule: Ribbons (version 1). Shot ribbons drawn by the engine: the trail strips of weapon projectiles in view, in place of the
// Lua shot follower's ribbon work (game-build docs/engine/NATIVE-RIBBONS-SPEC.md, docs/engine/rules/ribbons.md).
//
// Replaces:   the ribbon part (core, dash, thread) of LuaRules/Gadgets/gfx_shots.lua; puffs, bodies, prims and wakes stay there.
//             With the rule on, gfx_shots drops those parts from its recipes (KT1.IsActive("Ribbons")).
// Switch:     mod option kt1_ribbons (default off). Unsynced: draw only.
// Config:     gamedata/kt1/ribbons.lua (the follower plan's numbers: caps by effect scale, distance bands, short and guard life,
//             heavy-fight lightening, pool sizes)
// Settings:   KardashevFxShots (0 none, 1 short, 2 full), KardashevFxScale (caps row), and the heavy-fight overrides
//             KardashevRibbonExtrasMax, KardashevRibbonLifeMax, KardashevRibbonLifeK, KardashevRibbonCap; read at start and every
//             `settings_every` frames; Lua may push them (SetLimits).
// Unit data:  weapon customParams kt1_ribbons_<key> (written by game/gamedata/unitdefs_post.lua from the follower recipe,
//             gamedata/fx_shots.lua T.Recipe): priority, highonly, shortok, and per strip s in core, dash, thread:
//             kt1_ribbons_<s>_tex (texture path), _base (hot: additive; smoke: alpha), _tile (elmos per repeat), _w0, _w1 (half-widths at
//             birth and death), _life (s), _length (elmos: the trail lives as long as the shot takes to fly it), _cull (elmos of camera
//             distance; absent: the band rule), _colour ("r g b a"), _waver ("amp hz"); core also _essential.
// Timer:      KT1::Ribbons (the per-frame plan and the draw together)
// Lua:        Spring.KT1.Ribbons.Stats()        -> { ribbons, segments, added, draws, shots, shotsSum, capped, pools,
//                                                  planMs, drawMs, frames, drawFrames, heavy }
//                                                  (counts of the last frame; ms and frames add up since the start)
//             Spring.KT1.Ribbons.SetLimits(t)   t = { limiting, extrasMax, lifeMax, lifeK, cap, setting, scale }: any subset
//             Spring.KT1.Ribbons.Config()       -> the plan numbers in use
// Rule:       the follower's, step for step (gfx_shots.lua GameFrame and Bodies; LuaUI/fx/shots.lua S.*): each game frame, the shots in
//             the camera view with a ribbon recipe that the viewer may see get a band by camera distance, are eligible by the setting,
//             the guard and the band, and are capped by priority then distance. A drawn shot lays a segment every `stride` frames
//             (life over points); dash and thread ride on the same points unless heavy or past their cull. Each drawn frame a head
//             segment joins the last point to the shot. A shot gone from view lays nothing more; its segments fade by age.
// Determinism: draw only. Reads projectile positions; writes no sim state; no synced RNG.
// Draw:       one pool per (texture, base); a ring of segment instances on the GPU (only written slots are uploaded) and a head
//             buffer rewritten each drawn frame; one instanced draw per buffer, the follower's GLSL (cfx_ribbons_gl4.lua RIBBON_VS/FS).

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "Game/KT1/KT1Registry.h"
#include "Game/KT1/KT1Register.h"
#include "Lua/LuaParser.h"
#include "Lua/LuaUtils.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/GL/VAO.h"
#include "Rendering/GL/VBO.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "Sim/Weapons/WeaponDef.h"
#include "Sim/Weapons/WeaponDefHandler.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"

namespace KT1 {
namespace {

constexpr float FPS = 30.0f;     // GAME_SPEED: frames per game second (the follower's FPS)
constexpr int CORE = 0, DASH = 1, THREAD = 2, NSTRIP = 3;
const char* const STRIP[NSTRIP] = {"core", "dash", "thread"};

// One instance: a segment (cfx_ribbons_gl4.lua STRIDE 16 floats).
struct Seg { float p0t[4]; float p1t[4]; float shape[4]; float color[4]; };

const char* const VS = R"(#version 330
layout (location = 0) in vec2 corner;
layout (location = 1) in vec4 p0t;
layout (location = 2) in vec4 p1t;
layout (location = 3) in vec4 shape;
layout (location = 4) in vec4 color;
uniform mat4 viewProj;
uniform vec3 camPos;
uniform float now;
uniform float tile;
uniform float pxWorld;
out vec2 vUV;
out vec4 vColor;
out float vAge;
void main() {
	float life = max(shape.z, 0.001);
	if (now - p1t.w >= life) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
	vec3 a = p0t.xyz, b = p1t.xyz;
	float t = mix(p0t.w, p1t.w, corner.x);
	float age = clamp((now - t) / life, 0.0, 1.0);
	vec3 p = mix(a, b, corner.x);
	vec3 d = b - a;
	float len = length(d);
	vec3 dir = len > 0.001 ? d / len : vec3(0.0, 1.0, 0.0);
	vec3 side = cross(dir, camPos - p);
	side = length(side) > 0.0001 ? normalize(side) : vec3(1.0, 0.0, 0.0);
	float w = mix(shape.x, shape.y, sqrt(age));
	float wMin = 0.5 * pxWorld * length(camPos - p);
	float wDraw = max(w, wMin);
	vUV = vec2((shape.w + corner.x * len) / tile, corner.y * 0.5 + 0.5);
	vColor = vec4(color.rgb, color.a * w / max(wDraw, 1e-4));
	w = wDraw;
	vAge = age;
	gl_Position = viewProj * vec4(p + side * corner.y * w, 1.0);
}
)";

const char* const FS_HOT = R"(#version 330
uniform sampler2D tex;
in vec2 vUV;
in vec4 vColor;
in float vAge;
out vec4 frag;
void main() {
	float s = textureLod(tex, vec2(fract(vUV.x), vUV.y), 0.0).a;
	float a = s * vColor.a * pow(1.0 - vAge, 1.6);
	if (a < 0.003) discard;
	vec3 c = mix(vColor.rgb, vec3(1.0, 0.97, 0.9), s * s * (1.0 - vAge) * 0.6);
	frag = vec4(c * a, a);
}
)";

const char* const FS_SMOKE = R"(#version 330
uniform sampler2D tex;
in vec2 vUV;
in vec4 vColor;
in float vAge;
out vec4 frag;
void main() {
	float s = textureLod(tex, vec2(fract(vUV.x - vAge * 0.15), vUV.y), 0.0).a;
	float a = s * vColor.a * pow(1.0 - vAge, 1.2) * min(vAge * 12.0 + 0.15, 1.0);
	if (a < 0.004) discard;
	frag = vec4(vColor.rgb * (0.8 + 0.35 * s) * (1.0 - 0.2 * vAge), a);
}
)";

struct StripDef {
	bool on = false;
	int pool = -1;
	float w0 = 0, w1 = 0, life = 0, length = 0, cull = -1;   // length 0: none; cull < 0: the band rule
	float col[4] = {1, 1, 1, 1};
	float waverAmp = 0, waverHz = 0;
};
struct WeaponRibbon {
	bool any = false;
	int priority = 9;
	bool highOnly = false, shortOk = false, essential = false;
	StripDef s[NSTRIP];
};

struct Emitter {
	float px = 0, py = 0, pz = 0, u = 0, life = 0;
	int pf = 0, wd = -1;
	int seen = -1;           // the last frame it was in view
	bool started = false;    // laid a segment (the guard keeps a started ribbon going)
	bool drawing = false;    // drawn this frame (gets a head)
};

struct Pool {
	std::string tex, base;
	float tile = 64;
	bool add = true;
	uint32_t texID = 0;
	int max = 2048;
	std::vector<Seg> ring;
	int next = 0, used = 0;
	int lo = -1, hi = -1;            // slots written since the last upload (a run; a wrap uploads the tail then starts anew)
	std::vector<std::pair<int, int>> runs;
	float lastEnd = 0;
	std::vector<Seg> heads;
	VBO vbo, headVbo;
	VAO vao, headVao;
	bool glReady = false;

	void Add(const Seg& s) {
		const int slot = next;
		ring[slot] = s;
		if (lo < 0) { lo = hi = slot; }
		else if (slot == hi + 1) { hi = slot; }
		else { runs.emplace_back(lo, hi); lo = hi = slot; }
		next = (slot + 1) % max;
		used = std::min(used + 1, max);
		lastEnd = std::max(lastEnd, s.p1t[3] + s.shape[2]);
	}
};

struct Cap { float scale; int ribbons, points; };

class Ribbons : public Rule {
public:
	Ribbons() : Rule({"Ribbons", 1, false, "LuaRules/Gadgets/gfx_shots.lua (ribbon parts)", false}) {}

	void LoadConfig(const LuaTable& cfg) override {
		caps.clear();
		const LuaTable ct = cfg.SubTable("caps");
		for (int i = 1; ct.KeyExists(i); ++i) {
			const LuaTable c = ct.SubTable(i);
			caps.push_back({c.GetFloat("scale", 1), c.GetInt("ribbons", 200), c.GetInt("points", 12)});
		}
		if (caps.empty()) caps = {{0.5f, 60, 6}, {1, 200, 12}, {2, 600, 20}};
		nearD = cfg.GetFloat("near", 1500); midD = cfg.GetFloat("mid", 3500); farD = cfg.GetFloat("far", 6000);
		shortLife = cfg.GetFloat("short_life", 0.5f); guardLife = cfg.GetFloat("guard_life", 0.5f);
		const LuaTable h = cfg.SubTable("heavy");
		extrasMax = h.GetInt("extras", 40); lifeMax = h.GetInt("life", 120); lifeK = h.GetFloat("lifeK", 0.5f);
		poolMax = cfg.GetInt("pool_max", 8192); extraPoolMax = cfg.GetInt("extra_pool_max", 2048);
		headMax = cfg.GetInt("head_max", 2048);
		settingsEvery = std::max(1, cfg.GetInt("settings_every", 30));
		defaultPriority = cfg.GetInt("default_priority", 9);
		const LuaTable dt = cfg.SubTable("default_tex");
		texHot = dt.GetString("hot", "bitmaps/sp_fx/sp_ribbon_hot.tga");
		texSmoke = dt.GetString("smoke", "bitmaps/sp_fx/sp_ribbon_smoke.tga");
	}

	void Start() override {
		const int n = weaponDefHandler->NumWeaponDefs();
		defs.assign(n, WeaponRibbon{});
		int withRibbon = 0;
		for (int i = 0; i < n; ++i) {
			const WeaponDef* wd = weaponDefHandler->GetWeaponDefByID(i);
			if (wd == nullptr) continue;
			WeaponRibbon& w = defs[i];
			w.priority = (int)Num(wd, "priority", (float)defaultPriority);
			w.highOnly = Flag(wd, "highonly"); w.shortOk = Flag(wd, "shortok"); w.essential = Flag(wd, "core_essential");
			for (int s = 0; s < NSTRIP; ++s) {
				const std::string k = std::string(STRIP[s]) + "_";
				StripDef& d = w.s[s];
				const std::string* base = Str(wd, k + "base");
				if (base == nullptr) continue;
				const std::string* tex = Str(wd, k + "tex");
				const bool smoke = (*base == "smoke");
				d.on = true;
				d.pool = PoolOf(tex ? *tex : (smoke ? texSmoke : texHot), *base, Num(wd, k + "tile", smoke ? 48.0f : 64.0f), s == CORE ? poolMax : extraPoolMax);
				d.w0 = Num(wd, k + "w0", 1); d.w1 = Num(wd, k + "w1", d.w0); d.life = Num(wd, k + "life", 0.25f);
				d.length = Num(wd, k + "length", 0); d.cull = Num(wd, k + "cull", -1);
				Vec(wd, k + "colour", d.col, 4);
				float wv[2] = {0, 0}; Vec(wd, k + "waver", wv, 2); d.waverAmp = wv[0]; d.waverHz = wv[1];
			}
			w.any = w.s[CORE].on;
			withRibbon += w.any ? 1 : 0;
		}
		ReadSettings();
		LOG("[KT1] Ribbons: %d weapon defs with a ribbon, %d pools", withRibbon, (int)pools.size());
	}

	void DrawWorld() override {
		if (globalRendering->drawFrame == lastDrawFrame) return;
		lastDrawFrame = globalRendering->drawFrame;
		const spring_time t0 = spring_gettime();
		if (gs->frameNum != lastFrame) { Plan(gs->frameNum); lastFrame = gs->frameNum; }
		const spring_time t1 = spring_gettime();
		Heads();
		Draw();
		planMs += (t1 - t0).toMilliSecsf();
		drawMs += (spring_gettime() - t1).toMilliSecsf();
		drawFrames += 1;
	}

	void PushLua(lua_State* L) override {
		LuaPushRawNamedCFunc(L, "Stats", LStats);
		LuaPushRawNamedCFunc(L, "SetLimits", LSetLimits);
		LuaPushRawNamedCFunc(L, "Config", LConfig);
		LuaPushRawNamedCFunc(L, "Debug", LDebug);
	}

private:
	// ---- definitions
	static const std::string* Str(const WeaponDef* wd, const std::string& key) {
		const auto it = wd->customParams.find("kt1_ribbons_" + key);
		return (it == wd->customParams.end() || it->second.empty()) ? nullptr : &it->second;
	}
	static float Num(const WeaponDef* wd, const std::string& key, float def) {
		const std::string* v = Str(wd, key);
		if (v == nullptr) return def;
		char* end = nullptr; const float f = std::strtof(v->c_str(), &end);
		return end == v->c_str() ? def : f;
	}
	static bool Flag(const WeaponDef* wd, const std::string& key) {
		const std::string* v = Str(wd, key);
		return v != nullptr && *v != "0" && *v != "false";
	}
	static void Vec(const WeaponDef* wd, const std::string& key, float* out, int n) {
		const std::string* v = Str(wd, key);
		if (v == nullptr) return;
		const char* p = v->c_str();
		for (int i = 0; i < n; ++i) {
			char* end = nullptr; const float f = std::strtof(p, &end);
			if (end == p) break;
			out[i] = f; p = end;
		}
	}
	int PoolOf(const std::string& tex, const std::string& base, float tile, int max) {
		for (size_t i = 0; i < pools.size(); ++i)
			if (pools[i]->tex == tex && pools[i]->base == base) return (int)i;
		auto p = std::make_unique<Pool>();
		p->tex = tex; p->base = base; p->tile = tile; p->add = (base != "smoke"); p->max = std::max(16, max);
		p->ring.assign(p->max, Seg{});
		pools.push_back(std::move(p));
		return (int)pools.size() - 1;
	}

	// ---- settings (configHandler; Lua's SetLimits wins while set)
	void ReadSettings() {
		setting = lockSetting ? setting : std::clamp(configHandler->GetIntSafe("KardashevFxShots", 1), 0, 2);
		scale = lockScale ? scale : configHandler->GetFloatSafe("KardashevFxScale", 1.0f);
		if (!lockHeavy) {
			extrasNow = configHandler->GetIntSafe("KardashevRibbonExtrasMax", extrasMax);
			lifeMaxNow = configHandler->GetIntSafe("KardashevRibbonLifeMax", lifeMax);
			lifeKNow = configHandler->GetFloatSafe("KardashevRibbonLifeK", lifeK);
		}
		capOverride = lockCap ? capOverride : configHandler->GetIntSafe("KardashevRibbonCap", -1);
	}
	const Cap* CapsNow() const {
		if (scale <= 0.0f || caps.empty()) return nullptr;
		const Cap* pick = &caps[0];
		for (const Cap& c : caps) if (scale >= c.scale) pick = &c;
		return pick;
	}
	int Band(float d2) const {
		if (d2 < nearD * nearD) return 1;
		if (d2 < midD * midD) return 2;
		if (d2 < farD * farD) return 3;
		return 4;
	}

	// ---- the plan of one game frame (gfx_shots.lua GameFrame)
	struct Shot { int id; Emitter* e; const CProjectile* p; float d2; int band; int prio; bool want; };

	void Plan(int f) {
		if (f % settingsEvery == 0) ReadSettings();
		const float now = f / FPS;
		const CCamera* cam = camera;
		const float3 cp = cam->GetPos();
		shots.clear();
		for (const CProjectile* p : projectileHandler.GetActiveProjectiles(true).GetData()) {
			if (p == nullptr || !p->weapon || p->deleteMe) continue;
			const WeaponDef* wd = static_cast<const CWeaponProjectile*>(p)->GetWeaponDef();
			if (wd == nullptr || wd->id < 0 || wd->id >= (int)defs.size() || !defs[wd->id].any) continue;
			if (!CProjectileDrawer::CanDrawProjectile(p, p->GetAllyteamID())) continue;
			if (!cam->InView(p->pos, p->GetDrawRadius())) continue;
			Emitter& e = live[p->id];
			if (e.wd != wd->id || e.seen < f - 1) {          // new, or an id reused: start here
				const float3& sp = p->preFrameTra.t;
				e = Emitter{}; e.wd = wd->id; e.px = sp.x; e.py = sp.y; e.pz = sp.z; e.pf = f;
			}
			e.seen = f;
			const float3 d = p->pos - cp;
			const float d2 = d.dot(d);
			shots.push_back({p->id, &e, p, d2, Band(d2), defs[wd->id].priority, false});
		}
		for (auto it = live.begin(); it != live.end(); ) { if (it->second.seen != f) it = live.erase(it); else ++it; }

		const int n = (int)shots.size();
		stShots = n; stRibbons = 0; stCapped = 0; shotsSum += n;
		const Cap* c = CapsNow();
		const bool heavyExtras = n > extrasNow;
		const float lk = n > lifeMaxNow ? lifeKNow : 1.0f;
		// mode (S.Mode)
		const bool modeRibbons = c != nullptr && setting > 0 && !limiting;
		const bool modeKeep = c != nullptr && setting > 0;
		const bool missilesOnly = setting == 1;
		const bool flakOk = c != nullptr && c->scale >= 2.0f;
		const float lifeMul = limiting ? guardLife : 1.0f;
		const float lifeCap = setting == 1 ? shortLife : -1.0f;

		int want = 0;
		for (Shot& s : shots) {
			const WeaponRibbon& w = defs[s.e->wd];
			bool ok = s.band < 3;                                                       // S.Eligible
			if (ok && w.essential) ok = modeRibbons || modeKeep;
			else if (ok) {
				if (w.highOnly && !flakOk) ok = false;
				else if (missilesOnly && !w.shortOk) ok = false;
				else ok = modeRibbons || (modeKeep && s.e->started);
			}
			s.want = ok && c != nullptr;
			want += s.want ? 1 : 0;
		}
		int capN = c ? c->ribbons : 0;
		if (capOverride >= 0) capN = std::min(capN, capOverride);
		if (want > capN) {                                                           // S.Plan: priority, then distance
			std::stable_sort(shots.begin(), shots.end(), [](const Shot& a, const Shot& b) {
				if (a.want != b.want) return a.want;
				if (a.prio != b.prio) return a.prio < b.prio;
				return a.d2 < b.d2;
			});
			for (int i = capN; i < (int)shots.size(); ++i) shots[i].want = false;
			stCapped = want - capN;
		}

		for (Shot& s : shots) {
			Emitter& e = *s.e;
			const WeaponRibbon& w = defs[e.wd];
			const StripDef& core = w.s[CORE];
			bool draw = s.want;
			if (draw && core.cull >= 0 && s.d2 > core.cull * core.cull) draw = false;   // the strip's own camera distance
			e.drawing = draw;
			// the shot where the follower reads it: Lua's GameFrame(f) runs before the projectiles move, so its position is the one at the
			// start of the frame (preFrameTra); this plan runs at the first draw after the frame, when pos is a frame further on.
			const float3& pos = s.p->preFrameTra.t;
			if (!draw) { e.px = pos.x; e.py = pos.y; e.pz = pos.z; e.pf = f; continue; }
			stRibbons += 1;
			const float v = s.p->speed.w * FPS;
			float life = core.life * lifeMul;
			if (lifeCap >= 0.0f && life > lifeCap) life = lifeCap;
			life *= lk;
			if (core.length > 0.0f && v > 1.0f) life = std::max(2.0f / FPS, std::min(life * 3.0f, core.length / v));
			e.life = life;
			const int points = s.band == 2 ? std::max(1, c->points / 2) : c->points;
			const int stride = std::max(1, (int)std::ceil(life * FPS / std::max(1, points)));
			if (f - e.pf < stride) continue;
			const float dx = pos.x - e.px, dy = pos.y - e.py, dz = pos.z - e.pz;
			const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (len > 0.05f) {
				AddSeg(core, e, pos, f, now, life);
				e.started = true;
				for (int k = DASH; k <= THREAD; ++k) {
					const StripDef& d = w.s[k];
					if (!d.on || heavyExtras || !PartIn(d, s)) continue;
					AddSeg(d, e, pos, f, now, ExtraLife(d, v));
				}
			}
			e.u += len;
			e.px = pos.x; e.py = pos.y; e.pz = pos.z; e.pf = f;
		}
		heavyNow = heavyExtras;
		frames += 1;
	}

	bool PartIn(const StripDef& d, const Shot& s) const {
		if (d.cull >= 0) return s.d2 <= d.cull * d.cull;
		return s.band < 3;
	}
	static float ExtraLife(const StripDef& d, float v) {
		if (d.length <= 0.0f) return d.life;
		return v > 1.0f ? std::max(2.0f / FPS, d.length / v) : d.life;
	}
	static float Waver(const StripDef& d, float now) {
		if (d.waverAmp == 0.0f) return 1.0f;
		return 1.0f + d.waverAmp * std::sin(now * d.waverHz * 6.2832f) * std::sin(now * d.waverHz * 2.3f);
	}
	static Seg MakeSeg(const StripDef& d, const Emitter& e, const float3& to, float t0, float t1, float life, float wv) {
		Seg g;
		g.p0t[0] = e.px; g.p0t[1] = e.py; g.p0t[2] = e.pz; g.p0t[3] = t0;
		g.p1t[0] = to.x; g.p1t[1] = to.y; g.p1t[2] = to.z; g.p1t[3] = t1;
		g.shape[0] = d.w0 * wv; g.shape[1] = d.w1 * wv; g.shape[2] = life; g.shape[3] = e.u;
		for (int i = 0; i < 4; ++i) g.color[i] = d.col[i];
		return g;
	}
	void AddSeg(const StripDef& d, const Emitter& e, const float3& to, int f, float now, float life) {
		pools[d.pool]->Add(MakeSeg(d, e, to, e.pf / FPS, now, life, Waver(d, now)));
		stSegments += 1;
	}

	// ---- heads: each drawn ribbon's last stretch, from its last point to the shot now (gfx_shots.lua Bodies)
	void Heads() {
		for (auto& p : pools) p->heads.clear();
		const float now = (gs->frameNum + globalRendering->timeOffset) / FPS;
		for (auto& kv : live) {
			const Emitter& e = kv.second;
			if (!e.drawing) continue;
			const CProjectile* p = projectileHandler.GetProjectileBySyncedID(kv.first);
			if (p == nullptr) continue;
			// the shot now, as the follower: st.x + st.vx * off, st.x the start-of-frame position its points use (preFrameTra), so
			// mix(preFrameTra, pos, off) = GetDrawPos. Points and head in the same frame of reference (7bae97cf87 mixed them: a frame long).
			const float3 to = p->GetDrawPos(globalRendering->timeOffset);
			const WeaponRibbon& w = defs[e.wd];
			for (int k = CORE; k <= THREAD; ++k) {
				const StripDef& d = w.s[k];
				if (!d.on) continue;
				if (k != CORE && heavyNow) continue;
				if (k != CORE) {
					const float3 dd = p->pos - camera->GetPos();
					const float d2 = dd.dot(dd);
					if (d.cull >= 0 ? d2 > d.cull * d.cull : Band(d2) >= 3) continue;
				}
				Pool& pl = *pools[d.pool];
				if ((int)pl.heads.size() >= headMax) continue;
				const float v = p->speed.w * FPS;
				pl.heads.push_back(MakeSeg(d, e, to, e.pf / FPS, now, k == CORE ? e.life : ExtraLife(d, v), Waver(d, now)));
			}
		}
	}

	// ---- draw
	bool InitGL() {
		if (glTried) return shaderHot != nullptr && shaderSmoke != nullptr;
		glTried = true;
		shaderHot = MakeShader("KT1 Ribbons hot", FS_HOT);
		shaderSmoke = MakeShader("KT1 Ribbons smoke", FS_SMOKE);
		if (shaderHot == nullptr || shaderSmoke == nullptr) { LOG_L(L_WARNING, "[KT1] Ribbons: shaders failed; no ribbons drawn"); return false; }
		const float quad[12] = {0, -1, 1, -1, 1, 1, 0, -1, 1, 1, 0, 1};   // two triangles: corner.x along, corner.y across
		quadVbo.Bind(GL_ARRAY_BUFFER);
		quadVbo.New(sizeof(quad), GL_STATIC_DRAW, quad);
		quadVbo.Unbind();
		return true;
	}
	Shader::IProgramObject* MakeShader(const char* name, const char* fs) {
		Shader::IProgramObject* sh = shaderHandler->CreateProgramObject("[KT1]", name);
		sh->AttachShaderObject(shaderHandler->CreateShaderObject(VS, "", GL_VERTEX_SHADER));
		sh->AttachShaderObject(shaderHandler->CreateShaderObject(fs, "", GL_FRAGMENT_SHADER));
		sh->Link();
		if (!sh->IsValid()) { LOG_L(L_WARNING, "[KT1] Ribbons shader %s: %s", name, sh->GetLog().c_str()); return nullptr; }
		sh->Enable(); sh->SetUniform("tex", 0); sh->Disable();
		return sh;
	}
	void SetupVAO(VAO& vao, VBO& inst) {
		vao.Bind();
		quadVbo.Bind(GL_ARRAY_BUFFER);
		glEnableVertexAttribArray(0);
		glVertexAttribDivisor(0, 0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		inst.Bind(GL_ARRAY_BUFFER);
		for (int i = 0; i < 4; ++i) {
			glEnableVertexAttribArray(1 + i);
			glVertexAttribDivisor(1 + i, 1);
			glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Seg), (const void*)(sizeof(float) * 4 * i));
		}
		vao.Unbind();
		inst.Unbind();
		quadVbo.Unbind();
	}
	void InitPool(Pool& p) {
		p.glReady = true;
		CBitmap bmp;
		if (bmp.Load(p.tex)) {
			GL::TextureCreationParams tcp;
			tcp.reqNumLevels = 1;
			tcp.wrapModes = std::array<int32_t, 3>{GL_REPEAT, GL_CLAMP_TO_EDGE, GL_REPEAT};
			p.texID = bmp.CreateTexture(tcp);
		} else {
			LOG_L(L_WARNING, "[KT1] Ribbons: texture %s not found", p.tex.c_str());
		}
		p.vbo.Bind(GL_ARRAY_BUFFER);
		p.vbo.New(sizeof(Seg) * p.max, GL_DYNAMIC_DRAW, p.ring.data());
		p.vbo.Unbind();
		p.headVbo.Bind(GL_ARRAY_BUFFER);
		p.headVbo.New(sizeof(Seg) * headMax, GL_STREAM_DRAW, nullptr);
		p.headVbo.Unbind();
		SetupVAO(p.vao, p.vbo);
		SetupVAO(p.headVao, p.headVbo);
		p.lo = p.hi = -1; p.runs.clear();
	}
	void Upload(Pool& p) {
		if (p.lo < 0 && p.runs.empty()) return;
		if (p.lo >= 0) p.runs.emplace_back(p.lo, p.hi);
		p.vbo.Bind(GL_ARRAY_BUFFER);
		for (const auto& r : p.runs)
			p.vbo.SetBufferSubData(sizeof(Seg) * r.first, sizeof(Seg) * (r.second - r.first + 1), &p.ring[r.first]);
		p.vbo.Unbind();
		p.runs.clear(); p.lo = p.hi = -1;
	}
	void Draw() {
		stDraws = 0;
		if (!InitGL()) return;
		for (auto& pp : pools) { if (!pp->glReady) InitPool(*pp); Upload(*pp); }
		using namespace GL::State;
		{	// alpha pools first, then additive (as the follower's groups)
			auto st = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), DepthTest(GL_TRUE), DepthMask(GL_FALSE), Culling(GL_FALSE));
			DrawPools(false, shaderSmoke);
		}
		{
			auto st = GL::SubState(Blending(GL_TRUE), BlendFunc(GL_ONE, GL_ONE), DepthTest(GL_TRUE), DepthMask(GL_FALSE), Culling(GL_FALSE));
			DrawPools(true, shaderHot);
		}
	}
	void DrawPools(bool add, Shader::IProgramObject* sh) {
		const float now = (gs->frameNum + globalRendering->timeOffset) / FPS;
		const float3 cp = camera->GetPos();
		const float pxWorld = 2.0f * std::tan(camera->GetHalfFov()) / std::max(1, globalRendering->viewSizeY);
		bool begun = false;
		for (auto& pp : pools) {
			Pool& p = *pp;
			if (p.add != add) continue;
			const bool ring = p.used > 0 && now <= p.lastEnd;
			if (!ring && p.heads.empty()) continue;
			if (!begun) {
				begun = true;
				sh->Enable();
				sh->SetUniformMatrix4x4("viewProj", false, &camera->GetViewProjectionMatrix().m[0]);
				sh->SetUniform("camPos", cp.x, cp.y, cp.z);
				sh->SetUniform("now", now);
				sh->SetUniform("pxWorld", pxWorld);
			}
			sh->SetUniform("tile", p.tile);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, p.texID);
			if (ring) {
				p.vao.Bind();
				glDrawArraysInstanced(GL_TRIANGLES, 0, 6, p.used);
				p.vao.Unbind();
				stDraws += 1;
			}
			if (!p.heads.empty()) {
				p.headVbo.Bind(GL_ARRAY_BUFFER);
				p.headVbo.SetBufferSubData(0, sizeof(Seg) * p.heads.size(), p.heads.data());
				p.headVbo.Unbind();
				p.headVao.Bind();
				glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)p.heads.size());
				p.headVao.Unbind();
				stDraws += 1;
			}
		}
		if (begun) {
			glBindTexture(GL_TEXTURE_2D, 0);
			sh->Disable();
		}
	}

	// ---- Lua
	static Ribbons* Self() { return static_cast<Ribbons*>(Registry::Get().Find("Ribbons")); }
	static int LStats(lua_State* L) {
		const Ribbons* r = Self();
		if (r == nullptr || !r->Enabled()) return 0;
		int segs = 0;
		for (const auto& p : r->pools) segs += p->used;
		lua_newtable(L);
		LuaPushNamedNumber(L, "ribbons", r->stRibbons);
		LuaPushNamedNumber(L, "segments", segs);
		LuaPushNamedNumber(L, "added", r->stSegments);
		LuaPushNamedNumber(L, "draws", r->stDraws);
		LuaPushNamedNumber(L, "shots", r->stShots);
		LuaPushNamedNumber(L, "shotsSum", (lua_Number)r->shotsSum);
		LuaPushNamedNumber(L, "capped", r->stCapped);
		LuaPushNamedNumber(L, "pools", (lua_Number)r->pools.size());
		LuaPushNamedNumber(L, "planMs", r->planMs);
		LuaPushNamedNumber(L, "drawMs", r->drawMs);
		LuaPushNamedNumber(L, "frames", r->frames);
		LuaPushNamedNumber(L, "drawFrames", r->drawFrames);
		LuaPushNamedBool(L, "heavy", r->heavyNow);
		return 1;
	}
	static int LSetLimits(lua_State* L) {
		Ribbons* r = Self();
		if (r == nullptr || !lua_istable(L, 1)) return 0;
		lua_getfield(L, 1, "limiting"); if (!lua_isnil(L, -1)) r->limiting = lua_toboolean(L, -1); lua_pop(L, 1);
		lua_getfield(L, 1, "extrasMax"); if (lua_isnumber(L, -1)) { r->extrasNow = lua_tointeger(L, -1); r->lockHeavy = true; } lua_pop(L, 1);
		lua_getfield(L, 1, "lifeMax"); if (lua_isnumber(L, -1)) { r->lifeMaxNow = lua_tointeger(L, -1); r->lockHeavy = true; } lua_pop(L, 1);
		lua_getfield(L, 1, "lifeK"); if (lua_isnumber(L, -1)) { r->lifeKNow = lua_tonumber(L, -1); r->lockHeavy = true; } lua_pop(L, 1);
		lua_getfield(L, 1, "cap"); if (lua_isnumber(L, -1)) { r->capOverride = lua_tointeger(L, -1); r->lockCap = true; } lua_pop(L, 1);
		lua_getfield(L, 1, "setting"); if (lua_isnumber(L, -1)) { r->setting = std::clamp((int)lua_tointeger(L, -1), 0, 2); r->lockSetting = true; } lua_pop(L, 1);
		lua_getfield(L, 1, "scale"); if (lua_isnumber(L, -1)) { r->scale = lua_tonumber(L, -1); r->lockScale = true; } lua_pop(L, 1);
		return 0;
	}
	// Spring.KT1.Ribbons.Debug() -> one drawing emitter (the lowest id): its last point and time, the head end and the shot's positions,
	// the frame and draw time, its life; for look checks (RIB-LOOK). Nil when none draws.
	static int LDebug(lua_State* L) {
		const Ribbons* r = Self();
		if (r == nullptr || !r->Enabled()) return 0;
		int best = -1;
		for (const auto& kv : r->live) if (kv.second.drawing && (best < 0 || kv.first < best)) best = kv.first;
		if (best < 0) return 0;
		const Emitter& e = r->live.at(best);
		const CProjectile* p = projectileHandler.GetProjectileBySyncedID(best);
		lua_newtable(L);
		LuaPushNamedNumber(L, "id", best);
		LuaPushNamedNumber(L, "px", e.px); LuaPushNamedNumber(L, "pz", e.pz); LuaPushNamedNumber(L, "pf", e.pf);
		LuaPushNamedNumber(L, "life", e.life); LuaPushNamedNumber(L, "u", e.u);
		LuaPushNamedNumber(L, "frame", gs->frameNum); LuaPushNamedNumber(L, "off", globalRendering->timeOffset);
		LuaPushNamedNumber(L, "planFrame", r->lastFrame);
		if (p != nullptr) {
			const float3 d = p->GetDrawPos(globalRendering->timeOffset);
			LuaPushNamedNumber(L, "posx", p->pos.x); LuaPushNamedNumber(L, "posz", p->pos.z);
			LuaPushNamedNumber(L, "prex", p->preFrameTra.t.x); LuaPushNamedNumber(L, "prez", p->preFrameTra.t.z);
			LuaPushNamedNumber(L, "drawx", d.x); LuaPushNamedNumber(L, "drawz", d.z);
			LuaPushNamedNumber(L, "dposx", p->drawPos.x); LuaPushNamedNumber(L, "dposz", p->drawPos.z);
			LuaPushNamedNumber(L, "speed", p->speed.w);
		}
		return 1;
	}
	static int LConfig(lua_State* L) {
		const Ribbons* r = Self();
		if (r == nullptr) return 0;
		lua_newtable(L);
		LuaPushNamedNumber(L, "setting", r->setting);
		LuaPushNamedNumber(L, "scale", r->scale);
		LuaPushNamedNumber(L, "extrasMax", r->extrasNow);
		LuaPushNamedNumber(L, "lifeMax", r->lifeMaxNow);
		LuaPushNamedNumber(L, "lifeK", r->lifeKNow);
		LuaPushNamedNumber(L, "cap", r->capOverride);
		LuaPushNamedNumber(L, "near", r->nearD);
		LuaPushNamedNumber(L, "mid", r->midD);
		LuaPushNamedNumber(L, "far", r->farD);
		LuaPushNamedBool(L, "limiting", r->limiting);
		return 1;
	}

	// config
	std::vector<Cap> caps;
	float nearD = 1500, midD = 3500, farD = 6000, shortLife = 0.5f, guardLife = 0.5f, lifeK = 0.5f;
	int extrasMax = 40, lifeMax = 120, poolMax = 8192, extraPoolMax = 2048, headMax = 2048, settingsEvery = 30, defaultPriority = 9;
	std::string texHot, texSmoke;
	// settings in use
	int setting = 1; float scale = 1; int extrasNow = 40, lifeMaxNow = 120; float lifeKNow = 0.5f; int capOverride = -1;
	bool limiting = false, lockSetting = false, lockScale = false, lockHeavy = false, lockCap = false;
	// state
	std::vector<WeaponRibbon> defs;
	std::vector<std::unique_ptr<Pool>> pools;
	std::unordered_map<int, Emitter> live;
	std::vector<Shot> shots;
	int lastFrame = -1; unsigned int lastDrawFrame = ~0u;
	bool heavyNow = false;
	// GL
	bool glTried = false;
	Shader::IProgramObject* shaderHot = nullptr;
	Shader::IProgramObject* shaderSmoke = nullptr;
	VBO quadVbo;
	// stats
	int stShots = 0, stRibbons = 0, stCapped = 0, stDraws = 0; long stSegments = 0, shotsSum = 0;
	double planMs = 0, drawMs = 0; long frames = 0, drawFrames = 0;
};

} // namespace

KT1_REGISTER_SYSTEM(Ribbons, 300, Ribbons)

} // namespace KT1
