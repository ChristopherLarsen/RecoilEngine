/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxBuild: the build, repair, reclaim and capture beams, the band on a unit under construction and the welding arcs
// (replaces LuaRules/Gadgets/gfx_build_fx.lua and the drawn half of gamedata/fx_build_logic.lua). Work comes from
// Snapshot<Core::Fx::BuildSample>; spec column instances. The dying down ghost of an unfinished model is SiteGhost.h.
// Engine-free.

#ifndef KT1_BUILD_BEAMS_H
#define KT1_BUILD_BEAMS_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"
#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1::FxBuild {

using Core::Fx::Vec3;
using KT1::FxDraw::Rgb;

constexpr int32_t BEAM_KINDS = 4;     // PARITY: Core::Fx::BuildWork

struct BeamStyle {
	float rgb[3] = {1.0f, 1.0f, 1.0f};    // DESIGN
	float flow = 1.0f;                // DESIGN: -1: mass flowing back
	float width = 2.2f;               // DESIGN
	bool orbs = false;                // DESIGN
	float orb[3] = {1.0f, 1.0f, 1.0f};    // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("rgb", rgb);
		s.Field("flow", flow);
		s.Field("width", width);
		s.Field("orbs", orbs);
		s.Field("orb", orb);
	}
};

struct BeamForm {                     // a faction's beam shape (1 lattice, 2 spray, 3 silk, 4 flow)
	int id = 1;                       // DESIGN
	float width = 1.7f;               // DESIGN: factor
	int strands = 2;                  // DESIGN
	float spread = 1.0f;              // DESIGN
	float wave = 0.0f;                // DESIGN
	float segment = 12.0f;            // DESIGN
	template<class S> void Describe(S& s) {
		s.Field("id", id);
		s.Field("width", width);
		s.Field("strands", strands);
		s.Field("spread", spread);
		s.Field("wave", wave);
		s.Field("segment", segment);
	}
};

struct WeldLook {
	float life = 0.1f;                // DESIGN: seconds per zap
	float rgb[3] = {1.0f, 1.0f, 1.0f};    // DESIGN
	float core = 0.7f;                // DESIGN: ribbon half widths, elmos
	float glow = 3.2f;                // DESIGN
	float core_alpha = 1.0f;          // DESIGN
	float glow_alpha = 0.28f;         // DESIGN
	float jitter = 0.35f;             // DESIGN: share of the target radius a hit point strays
	int sparks = 5;                   // DESIGN: per zap
	float spark_life = 0.28f;         // DESIGN
	float spark_speed = 55.0f;        // DESIGN
	float gravity = 160.0f;           // DESIGN
	float spark_trail = 0.03f;        // DESIGN: seconds of travel a streak shows
	float spark_width = 0.45f;        // DESIGN
	float spark_rgb[3] = {1.0f, 0.93f, 0.78f};   // DESIGN
	int max_sparks = 320;             // DESIGN
	int samples = 8;                  // DESIGN: target pieces a zap samples
	float built_floor = 0.08f;        // DESIGN: the least share of the height a weld point may reach
	float over = 2.0f;                // DESIGN: elmos above the built part
	template<class S> void Describe(S& s) {
		s.Field("life", life);
		s.Field("rgb", rgb);
		s.Field("core", core);
		s.Field("glow", glow);
		s.Field("core_alpha", core_alpha);
		s.Field("glow_alpha", glow_alpha);
		s.Field("jitter", jitter);
		s.Field("sparks", sparks);
		s.Field("spark_life", spark_life);
		s.Field("spark_speed", spark_speed);
		s.Field("gravity", gravity);
		s.Field("spark_trail", spark_trail);
		s.Field("spark_width", spark_width);
		s.Field("spark_rgb", spark_rgb);
		s.Field("max_sparks", max_sparks);
		s.Field("samples", samples);
		s.Field("built_floor", built_floor);
		s.Field("over", over);
	}
};

struct BeamsConfig {                  // game/data/kt1/fxbuild.json "beams"
	int max_beams = 160;              // DESIGN: builders drawn per frame, nearest first
	int max_frames = 48;              // DESIGN: units under construction with the band, nearest first
	float band_alpha = 0.9f;          // DESIGN
	float beam_alpha = 0.95f;         // DESIGN
	float band_width = 1.6f;          // DESIGN
	float surface_share = 0.55f;      // DESIGN: a beam ends this share of the target radius short of its centre
	std::string setting = "KardashevFxBuild";   // DESIGN
	// DESIGN: build, repair, reclaim, capture (build green: fx_style.lua palette.build)
	BeamStyle styles[BEAM_KINDS] = {
		{{0.45f, 1.0f, 0.70f}, 1.0f, 2.6f, false, {1.0f, 1.0f, 1.0f}},
		{{0.55f, 0.85f, 1.0f}, 1.0f, 2.2f, false, {1.0f, 1.0f, 1.0f}},
		{{1.0f, 0.72f, 0.30f}, -1.0f, 2.6f, true, {1.0f, 0.88f, 0.55f}},
		{{0.85f, 0.55f, 1.0f}, 1.0f, 2.2f, false, {1.0f, 1.0f, 1.0f}},
	};
	// DESIGN: by Core::Fx::Faction: Human, Arachnid, The Grey, Hadean
	BeamForm forms[static_cast<int>(Core::Fx::Faction::Count)] = {{1, 1.7f, 2, 1.0f, 0.0f, 12.0f}, {3, 1.9f, 3, 1.0f, 0.15f, 0.0f},
		{2, 1.0f, 0, 2.8f, 0.0f, 0.0f}, {4, 1.5f, 1, 1.3f, 0.3f, 30.0f}};   // KT1 lint: allow frame-literal DESIGN: a segment length in elmos
	WeldLook weld;
	template<class S> void Describe(S& s) {
		s.Field("max_beams", max_beams);
		s.Field("max_frames", max_frames);
		s.Field("band_alpha", band_alpha);
		s.Field("beam_alpha", beam_alpha);
		s.Field("band_width", band_width);
		s.Field("surface_share", surface_share);
		s.Field("setting", setting);
		static constexpr const char* KIND_KEYS[BEAM_KINDS] = {"build", "repair", "reclaim", "capture"};
		static constexpr const char* FORM_KEYS[static_cast<int>(Core::Fx::Faction::Count)] = {"human", "arachnid", "grey", "hadean"};
		for (int i = 0; i < BEAM_KINDS; ++i) s.Object(KIND_KEYS[i], [&](auto& o) { styles[i].Describe(o); });
		for (int i = 0; i < static_cast<int>(Core::Fx::Faction::Count); ++i)
			s.Object(FORM_KEYS[i], [&](auto& o) { forms[i].Describe(o); });
		s.Object("weld", [&](auto& o) { weld.Describe(o); });
	}
};

struct BeamLook {
	bool on = false;
	Rgb colour, orbColour;
	float flow = 1.0f, width = 0.0f;
	bool orbs = false;
	BeamForm form;
};

struct Spark { Vec3 pos, vel; float born = 0.0f; };
struct Zap { Vec3 from, to; float born = 0.0f; int32_t builder = -1; };

struct BuilderIn {                    // one working builder, as seen this frame
	int32_t builder = -1;
	const Core::Fx::BuildSample* work = nullptr;
	Core::Fx::Faction faction = Core::Fx::Faction::Human;
	float dist = 0.0f;                // camera distance
	bool visible = false;
};

struct BuildStats { uint32_t beams = 0, bands = 0, welds = 0, sparks = 0, orbs = 0; float costMs = 0.0f; };

struct BeamsState {
	KT1::FxDraw::InstanceList<KT1::FxDraw::BeamInstance> beams;
	std::vector<Zap> zaps;
	std::vector<Spark> sparks;
	std::vector<int32_t> order;       // scratch: nearest first
	KT1::FxDraw::Random random;
	BuildStats stats;
};

BeamLook LookOfBeam(const BeamsConfig& c, Core::Fx::BuildWork kind, bool limiting, Core::Fx::Faction faction);
bool Welds(Core::Fx::BuildWork kind);
Vec3 SurfaceEnd(const BeamsConfig& c, const Vec3& source, const Vec3& target, float targetRadius);
// the band's height on a unit under construction and the share still to build
void BandOf(float progress, float height, float& bandY, float& left);
Vec3 WeldPoint(const BeamsConfig& c, const Core::Fx::BuildSample& work, KT1::FxDraw::Random& random);
Vec3 SparkAt(const BeamsConfig& c, const Spark& spark, float age);
void InitBeamsState(BeamsState& s, const BeamsConfig& c);
// writes the frame's beams, bands, zaps and sparks
void StepBeams(BeamsState& s, const BeamsConfig& c, std::span<const BuilderIn> builders, bool limiting, float now);

} // namespace KT1::FxBuild

#endif
