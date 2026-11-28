/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: the large blast's player (replaces LuaUI/fx/nuke_blast.lua; the timeline is the data of
// gamedata/fx_nuke.lua). Every particle is written once, at the blast, with its birth in the future. Variants are the
// commander's death, the strategic and the tactical warhead and the script trees. All layers are data. Engine-free.

#ifndef KT1_FIELD_NUKE_BLAST_H
#define KT1_FIELD_NUKE_BLAST_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Game/KT1/Draw/FxCommon/FxParticles.h"

namespace KT1::FxFields {

struct NukeLayer {
	std::string id;                   // DATA
	std::string pool;                 // DATA: flash, glow, smoke, spark or a textured pool
	std::string shape;                // DATA: single, ring, hemi, column
	int count = 0;                    // DATA
	float t0 = 0.0f;                  // DATA: seconds after the blast
	float dur = 0.0f;                 // DATA: the births spread over this
	float life[2] = {0.0f, 0.0f};     // DATA: least, most (each range below likewise)
	float size0[2] = {0.0f, 0.0f};    // DATA
	float size1[2] = {0.0f, 0.0f};    // DATA
	float radius[2] = {0.0f, 0.0f};   // DATA
	float speed[2] = {0.0f, 0.0f};    // DATA
	float up[2] = {0.0f, 0.0f};       // DATA
	float height[2] = {0.0f, 0.0f};   // DATA: both 0: none
	float y0[2] = {0.0f, 0.0f};       // DATA
	float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	float drag = 0.0f;                // DATA
	float gravity = 0.0f;             // DATA
	float top_shorter = 0.0f;         // DATA
	bool tangent = false;             // DATA
	bool ground = false;              // DATA: not over water
	bool water = false;               // DATA: only over water
	bool far_too = false;             // DATA: kept beyond the far distance
	bool extra = false;               // DATA: dropped while reduced
	bool fog = false;                 // DATA: drawn without sight
	bool unit_only = false;           // DATA: a unit-hit set
	template<class S> void Describe(S& s) {
		s.Field("id", id);
		s.Field("pool", pool);
		s.Field("shape", shape);
		s.Field("count", count);
		s.Field("t0", t0);
		s.Field("dur", dur);
		s.Field("life", life);
		s.Field("size0", size0);
		s.Field("size1", size1);
		s.Field("radius", radius);
		s.Field("speed", speed);
		s.Field("up", up);
		s.Field("height", height);
		s.Field("y0", y0);
		s.Field("color", color);
		s.Field("drag", drag);
		s.Field("gravity", gravity);
		s.Field("top_shorter", top_shorter);
		s.Field("tangent", tangent);
		s.Field("ground", ground);
		s.Field("water", water);
		s.Field("far", far_too);
		s.Field("extra", extra);
		s.Field("fog", fog);
		s.Field("unit_only", unit_only);
	}
};

struct NukeDecal {
	std::string id;                   // DATA
	float t0 = 0.0f;                  // DATA
	float radius = 0.0f;              // DATA
	float life = 0.0f;                // DATA
	float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) {
		s.Field("id", id);
		s.Field("t0", t0);
		s.Field("radius", radius);
		s.Field("life", life);
		s.Field("color", color);
	}
};

struct NukeLight {
	std::string name;                 // DATA: the light set; empty: the default
	float t0 = 0.0f;                  // DATA
	float life = 0.0f;                // DATA
	float radius = 0.0f;              // DATA
	float color[3] = {1.0f, 1.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("t0", t0);
		s.Field("life", life);
		s.Field("radius", radius);
		s.Field("color", color);
	}
};

struct NukeSet {
	std::string name;                 // DATA
	std::string layers;               // DATA: layer ids, "a+b+c"
	std::string decals;               // DATA: the decal set; empty: none
	std::string light;                // DATA: the light set; empty: none
	bool pulse = false;               // DATA: the camera pulse
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("layers", layers);
		s.Field("decals", decals);
		s.Field("light", light);
		s.Field("pulse", pulse);
	}
};

struct NukeVariant {
	std::string name;                 // DATA
	std::string set;                  // DATA
	float scale = 1.0f;               // DATA
	float count = 1.0f;               // DATA
	float tier_mid = 0.0f;            // DATA: flash only past this camera distance; 0: no tiers
	float tier_far = 0.0f;            // DATA: nothing past this
	float unit_hit_radius = 0.0f;     // DATA
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("set", set);
		s.Field("scale", scale);
		s.Field("count", count);
		s.Field("tier_mid", tier_mid);
		s.Field("tier_far", tier_far);
		s.Field("unit_hit_radius", unit_hit_radius);
	}
};

struct NukeDecalSet {
	std::string name;                 // DATA
	std::vector<NukeDecal> decals;    // DATA
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.List("decals", decals, [](auto& o, NukeDecal& d) { d.Describe(o); });
	}
};

struct NukeConfig {                   // game/data/kt1/fxnuke.json
	std::vector<NukeLayer> layers;        // DATA
	std::vector<NukeSet> sets;            // DATA
	std::vector<NukeVariant> variants;    // DATA
	std::vector<NukeDecalSet> decal_sets; // DATA
	std::vector<NukeLight> lights;        // DATA
	std::string shock = "ring2+sring";    // DESIGN: the outer shock rings: paced with the damage bands
	float far_dist = 9000.0f;             // DESIGN: at scale 1
	float reduced = 0.4f;                 // DESIGN: count factor while reduced
	float flash_dim = 0.35f;              // DESIGN: flashes under reduce motion
	float pulse_t0 = 0.0f;                // DESIGN
	float pulse_amp = 18.0f;              // DESIGN
	float pulse_time = 0.45f;             // DESIGN
	float pulse_reach = 5000.0f;          // DESIGN
	template<class S> void Describe(S& s) {
		s.List("layers", layers, [](auto& o, NukeLayer& l) { l.Describe(o); });
		s.List("sets", sets, [](auto& o, NukeSet& n) { n.Describe(o); });
		s.List("variants", variants, [](auto& o, NukeVariant& v) { v.Describe(o); });
		s.List("decal_sets", decal_sets, [](auto& o, NukeDecalSet& d) { d.Describe(o); });
		s.List("lights", lights, [](auto& o, NukeLight& l) { l.Describe(o); });
		s.Field("shock", shock);
		s.Field("far_dist", far_dist);
		s.Field("reduced", reduced);
		s.Field("flash_dim", flash_dim);
		s.Field("pulse_t0", pulse_t0);
		s.Field("pulse_amp", pulse_amp);
		s.Field("pulse_time", pulse_time);
		s.Field("pulse_reach", pulse_reach);
	}
};

struct NukeWave { bool on = false; float r0 = 0.0f, speed = 0.0f, reach = 0.0f, life = 0.0f, t0 = 0.0f; };

struct NukeState {                    // the viewer's side of one blast
	bool water = false, reduced = false, fogged = false, lights = true, decals = true, shake = true, reduceMotion = false,
		overview = false, unitHit = false, hasDist = false;
	float dist = 0.0f;
	NukeWave wave;
};

struct NukePlanLayer { int32_t layer = 0; int32_t count = 0; bool waved = false; };

struct NukePlan {
	bool ok = false;                  // false: unknown variant, or not drawn at this distance
	float scale = 1.0f, flashScale = 1.0f;
	bool water = false, far = false, fogged = false, decals = false, light = false, pulse = false;
	int32_t decalSet = -1, lightRow = -1;
	NukeWave wave;
	std::vector<NukePlanLayer> layers;
};

// the pools a blast writes into, by the layer's pool name
struct NukePools { std::vector<std::string> names; std::vector<KT1::FxDraw::ParticlePool> pools; };

struct NukeEmitted { int32_t total = 0; float last = 0.0f; };   // last: the latest birth plus life, seconds after now

uint32_t NukeSeed(float x, float z, int32_t step);
// the pace of a damage zone delivered in bands (CMB-010 v6 R10); off when it comes at once
NukeWave WaveOf(float radius, int32_t steps, float spreadS, float delayS);
void PlanNuke(const NukeConfig& c, std::string_view variant, const NukeState& state, NukePlan& out);
NukeEmitted EmitNuke(const NukeConfig& c, const NukePlan& plan, const Core::Fx::Vec3& pos, float now, uint32_t seed,
	NukePools& pools);

} // namespace KT1::FxFields

#endif
