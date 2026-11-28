/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 ShotFollower: what a followed shot looks like (replaces the recipes LuaUI/fx/shots.lua and gamedata/fx_shots.lua
// build for gfx_shots.lua). One row per look, found by Core::Fx::WeaponFx::look. All of it is data: defaults are empty or
// neutral. Lengths elmos, times seconds, speeds elmos per second. A part that is not there has `on` false or an empty
// list. Cut-offs are view depths; negative: none. Engine-free.

#ifndef KT1_SHOT_RECIPE_H
#define KT1_SHOT_RECIPE_H

#include <string>
#include <vector>

namespace KT1::ShotFollower {

struct RecipeColour {
	float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) { s.Field("colour", rgba); }
};

struct RibbonPart : RecipeColour {    // the main ribbon, a dash or a thread
	bool on = false;                  // DATA
	std::string look;                 // DATA: the ribbon pool
	float w0 = 0.0f;                  // DATA
	float w1 = 0.0f;                  // DATA
	float life = 0.0f;                // DATA
	float length = 0.0f;              // DATA: 0: by life
	float flight = 0.0f;              // DATA: 0: not set
	float cutoff = -1.0f;             // DATA
	bool has_cutoff = false;          // DATA
	bool full = false;                // DATA
	bool smoke = false;               // DATA
	bool essential = false;           // DATA: the body is the ribbon: drawn at Medium too
	float waver_amp = 0.0f;           // DATA
	float waver_hz = 0.0f;            // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("on", on);
		s.Field("look", look);
		s.Field("w0", w0);
		s.Field("w1", w1);
		s.Field("life", life);
		s.Field("length", length);
		s.Field("flight", flight);
		s.Field("cutoff", cutoff);
		s.Field("has_cutoff", has_cutoff);
		s.Field("full", full);
		s.Field("smoke", smoke);
		s.Field("essential", essential);
		s.Field("waver_amp", waver_amp);
		s.Field("waver_hz", waver_hz);
	}
};

struct RampColour {
	float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) { s.Field("rgba", rgba); }
};

struct Birth {
	float at_s = 0.0f;                // DATA: after the event
	int count = 0;                    // DATA
	template<class S> void Describe(S& s) {
		s.Field("at_s", at_s);
		s.Field("count", count);
	}
};

// one layer of a pooled impact, muzzle or trail: particles of the pool `code`
struct PoolLayer {
	std::string code;                 // DATA: the pool (layers of one code share it)
	std::string role;                 // DATA: palette role of the colour
	std::string file;                 // DATA: the texture
	float dark = 0.0f;                // DATA
	int grid_cols = 1;                // DATA
	int grid_rows = 1;                // DATA
	bool flat = false;                // DATA
	bool stretch = false;             // DATA
	bool straight = false;            // DATA
	std::vector<RampColour> ramp;     // DATA: five keys over life, or none
	float gain = 1.0f;                // DATA
	std::vector<Birth> births;        // DATA
	float life[2] = {0.0f, 0.0f};     // DATA: least, most
	float size0[2] = {0.0f, 0.0f};    // DATA
	float size1[2] = {0.0f, 0.0f};    // DATA
	float spread = 0.0f;              // DATA
	float radius = 0.0f;              // DATA
	float speed = 0.0f;               // DATA
	float along = 0.0f;               // DATA
	float gravity = 0.0f;             // DATA
	float drag = 0.0f;                // DATA
	float cutoff = -1.0f;             // DATA
	float spin = 0.0f;                // DATA
	float elong = 0.0f;               // DATA
	template<class S> void Describe(S& s) {
		s.Field("code", code);
		s.Field("role", role);
		s.Field("file", file);
		s.Field("dark", dark);
		s.Field("grid_cols", grid_cols);
		s.Field("grid_rows", grid_rows);
		s.Field("flat", flat);
		s.Field("stretch", stretch);
		s.Field("straight", straight);
		s.List("ramp", ramp, [](auto& o, RampColour& c) { c.Describe(o); });
		s.Field("gain", gain);
		s.List("births", births, [](auto& o, Birth& b) { b.Describe(o); });
		s.Field("life", life);
		s.Field("size0", size0);
		s.Field("size1", size1);
		s.Field("spread", spread);
		s.Field("radius", radius);
		s.Field("speed", speed);
		s.Field("along", along);
		s.Field("gravity", gravity);
		s.Field("drag", drag);
		s.Field("cutoff", cutoff);
		s.Field("spin", spin);
		s.Field("elong", elong);
	}
};

struct PuffStream : RecipeColour {    // puffs laid along the path
	std::string pool;                 // DATA: smoke, glow, bubble
	float every_s = 0.0f;             // DATA: between puffs; 0: by rate
	float rate = 0.0f;                // DATA: per second along the path
	float pack_speed = 0.0f;          // DATA: the speed the rate is given at
	float s0 = 0.0f;                  // DATA
	float s1 = 0.0f;                  // DATA
	float life = 0.0f;                // DATA
	float life_jitter = 0.0f;         // DATA
	float rise = 0.0f;                // DATA
	float gravity = 0.0f;             // DATA
	std::string medium;               // DATA: air, water; empty: both
	bool body = false;                // DATA: the stream is the shot's body
	float cutoff = -1.0f;             // DATA
	bool layer_cutoff = false;        // DATA: it carries its own cut-offs
	std::vector<PoolLayer> pfx;       // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("pool", pool);
		s.Field("every_s", every_s);
		s.Field("rate", rate);
		s.Field("pack_speed", pack_speed);
		s.Field("s0", s0);
		s.Field("s1", s1);
		s.Field("life", life);
		s.Field("life_jitter", life_jitter);
		s.Field("rise", rise);
		s.Field("gravity", gravity);
		s.Field("medium", medium);
		s.Field("body", body);
		s.Field("cutoff", cutoff);
		s.Field("layer_cutoff", layer_cutoff);
		s.List("pfx", pfx, [](auto& o, PoolLayer& l) { l.Describe(o); });
	}
};

struct Burst : RecipeColour {         // entering water, an impact's shrink, release and waves
	std::string pool;                 // DATA
	int count = 0;                    // DATA
	float delay = 0.0f;               // DATA
	float gap = 0.0f;                 // DATA
	float every_s = 0.0f;             // DATA
	float radius = 0.0f;              // DATA
	float life = 0.0f;                // DATA
	float s0 = 0.0f;                  // DATA
	float s1 = 0.0f;                  // DATA
	float speed = 0.0f;               // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("pool", pool);
		s.Field("count", count);
		s.Field("delay", delay);
		s.Field("gap", gap);
		s.Field("every_s", every_s);
		s.Field("radius", radius);
		s.Field("life", life);
		s.Field("s0", s0);
		s.Field("s1", s1);
		s.Field("speed", speed);
	}
};

struct WakePart : RecipeColour {
	bool on = false;                  // DATA
	std::string look;                 // DATA
	float w0 = 0.0f;                  // DATA
	float w1 = 0.0f;                  // DATA
	float life = 0.0f;                // DATA
	float max_depth = 0.0f;           // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("on", on);
		s.Field("look", look);
		s.Field("w0", w0);
		s.Field("w1", w1);
		s.Field("life", life);
		s.Field("max_depth", max_depth);
	}
};

struct PrimPart : RecipeColour {      // the eight-sided slug (LuaRules/fx_prim.lua)
	bool on = false;                  // DATA
	float length = 0.0f;              // DATA
	float radius = 0.0f;              // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("on", on);
		s.Field("length", length);
		s.Field("radius", radius);
	}
};

struct BodyPart : RecipeColour {
	bool on = false;                  // DATA
	std::string look;                 // DATA: capsule, glow, bomb
	std::string medium;               // DATA
	float length = 0.0f;              // DATA
	float width = 0.0f;               // DATA
	float pulse = 0.0f;               // DATA
	float amp = 0.0f;                 // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("on", on);
		s.Field("look", look);
		s.Field("medium", medium);
		s.Field("length", length);
		s.Field("width", width);
		s.Field("pulse", pulse);
		s.Field("amp", amp);
	}
};

struct ArcPart : RecipeColour {
	bool on = false;                  // DATA
	float w0 = 0.0f;                  // DATA
	float w1 = 0.0f;                  // DATA
	float life = 0.0f;                // DATA
	float length = 0.0f;              // DATA
	int pieces = 0;                   // DATA
	float jag = 0.0f;                 // DATA
	int branches = 0;                 // DATA
	float every_s = 0.0f;             // DATA
	float core[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	template<class S> void Describe(S& s) {
		RecipeColour::Describe(s);
		s.Field("on", on);
		s.Field("w0", w0);
		s.Field("w1", w1);
		s.Field("life", life);
		s.Field("length", length);
		s.Field("pieces", pieces);
		s.Field("jag", jag);
		s.Field("branches", branches);
		s.Field("every_s", every_s);
		s.Field("core", core);
	}
};

struct ImpactPart {                   // an energy family's own impact
	bool on = false;                  // DATA
	bool implode = false;             // DATA
	bool pulse = false;               // DATA
	Burst shrink, release, waves;
	float stain_size = 0.0f;          // DATA: 0: no stain
	float stain_life = 0.0f;          // DATA
	template<class S> void Describe(S& s) {
		s.Field("on", on);
		s.Field("implode", implode);
		s.Field("pulse", pulse);
		s.Object("shrink", [&](auto& o) { shrink.Describe(o); });
		s.Object("release", [&](auto& o) { release.Describe(o); });
		s.Object("waves", [&](auto& o) { waves.Describe(o); });
		s.Field("stain_size", stain_size);
		s.Field("stain_life", stain_life);
	}
};

struct ShotRecipe {
	std::string name;                 // DATA: the look's key
	int priority = 9;                 // DATA: cap order, lower first
	bool missile = false;             // DATA
	bool flak = false;                // DATA
	int high_only = -1;               // DATA: -1 as the class, 0 no, 1 yes
	bool tiers = false;               // DATA: the far band keeps the core only
	bool layered = false;             // DATA: parts follow only their own cut-offs
	bool family = false;              // DATA: an energy family (arcs, waves, implosions, its own impact)
	RibbonPart ribbon, dash, thread;
	std::vector<PoolLayer> muzzle, land, unit_hit, fragments;   // DATA
	WakePart wake;
	PuffStream puffs;                 // pool empty: none
	std::vector<PuffStream> extra_puffs;   // DATA
	std::vector<Burst> enter_water;   // DATA
	PrimPart prim;
	BodyPart body;
	ArcPart arc;
	Burst waves;                      // every_s 0: none
	Burst implode;                    // every_s 0: none
	float core_size = 0.0f;           // DATA: 0: none
	float core_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // DATA
	ImpactPart impact;
	float glow_radius = 0.0f;         // DATA: the post pass's glow source; 0: none
	float glow_amount = 0.0f;         // DATA
	template<class S> void Describe(S& s) {
		s.Field("name", name);
		s.Field("priority", priority);
		s.Field("missile", missile);
		s.Field("flak", flak);
		s.Field("high_only", high_only);
		s.Field("tiers", tiers);
		s.Field("layered", layered);
		s.Field("family", family);
		s.Object("ribbon", [&](auto& o) { ribbon.Describe(o); });
		s.Object("dash", [&](auto& o) { dash.Describe(o); });
		s.Object("thread", [&](auto& o) { thread.Describe(o); });
		const auto layer = [](auto& o, PoolLayer& l) { l.Describe(o); };
		s.List("muzzle", muzzle, layer);
		s.List("land", land, layer);
		s.List("unit_hit", unit_hit, layer);
		s.List("fragments", fragments, layer);
		s.Object("wake", [&](auto& o) { wake.Describe(o); });
		s.Object("puffs", [&](auto& o) { puffs.Describe(o); });
		s.List("extra_puffs", extra_puffs, [](auto& o, PuffStream& p) { p.Describe(o); });
		s.List("enter_water", enter_water, [](auto& o, Burst& b) { b.Describe(o); });
		s.Object("prim", [&](auto& o) { prim.Describe(o); });
		s.Object("body", [&](auto& o) { body.Describe(o); });
		s.Object("arc", [&](auto& o) { arc.Describe(o); });
		s.Object("waves", [&](auto& o) { waves.Describe(o); });
		s.Object("implode", [&](auto& o) { implode.Describe(o); });
		s.Field("core_size", core_size);
		s.Field("core_colour", core_colour);
		s.Object("impact", [&](auto& o) { impact.Describe(o); });
		s.Field("glow_radius", glow_radius);
		s.Field("glow_amount", glow_amount);
	}
};

} // namespace KT1::ShotFollower

#endif
