/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxFields: the extra smoke, debris, bubbles and fire of a classified blast (replaces the draw half of
// LuaRules/Gadgets/fx_blasts.lua and the plan of gamedata/fx_blast.lua), fed by the Blast event. Counts per class are the
// Lua's; its frame counts are seconds here and its per frame speeds elmos per second. Engine-free.

#ifndef KT1_FIELD_BLASTS_H
#define KT1_FIELD_BLASTS_H

#include <cstdint>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Draw/FxCommon/FxParticles.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"
#include "Game/KT1/Draw/FxCommon/FxRoster.h"

namespace KT1::FxFields {

constexpr int32_t BLAST_CLASSES = 5;      // PARITY: Core::Fx::BlastClass without None
constexpr int32_t BLAST_FAMILIES = 7;     // PARITY: Core::Fx::BlastFamily without None

struct BlastClassLook {
	int smoke_count = 0;                  // DESIGN
	float smoke_size = 0.0f;              // DESIGN
	float smoke_grow = 0.0f;              // DESIGN
	float smoke_life = 0.0f;              // DESIGN: seconds
	float smoke_rise = 0.0f;              // DESIGN: elmos per s
	int debris_count = 0;                 // DESIGN
	float debris_size = 0.0f;             // DESIGN
	float debris_speed = 0.0f;            // DESIGN: elmos per s
	float debris_life = 0.0f;             // DESIGN: seconds
	float far_dist = 0.0f;                // DESIGN: not drawn beyond this camera distance
	template<class S> void Describe(S& s) {
		s.Field("smoke_count", smoke_count);
		s.Field("smoke_size", smoke_size);
		s.Field("smoke_grow", smoke_grow);
		s.Field("smoke_life", smoke_life);
		s.Field("smoke_rise", smoke_rise);
		s.Field("debris_count", debris_count);
		s.Field("debris_size", debris_size);
		s.Field("debris_speed", debris_speed);
		s.Field("debris_life", debris_life);
		s.Field("far_dist", far_dist);
	}
};

enum class SmokeLook : uint8_t { Smoke, Dark, Steam };

struct BlastFamilyLook {
	float smoke = 1.0f;                   // DESIGN: factor on the class's smoke
	float debris = 1.0f;                  // DESIGN
	bool dark = false;                    // DESIGN: dark smoke (flak)
	template<class S> void Describe(S& s) {
		s.Field("smoke", smoke);
		s.Field("debris", debris);
		s.Field("dark", dark);
	}
};

struct BlastConfig {                      // "blasts"
	int smoke_max = 6144;                 // DESIGN: particles alive
	int spark_max = 6144;                 // DESIGN
	int bubble_max = 2048;                // DESIGN
	int fire_max = 1024;                  // DESIGN
	float gravity = -220.0f;              // DESIGN: elmos per s squared on debris
	float view_pad = 80.0f;               // DESIGN
	float fire_size = 1.6f;               // DESIGN: times the class's smoke size
	float fire_life = 1.1f;               // DESIGN
	float hit_white[3] = {0.92f, 0.91f, 0.89f};   // DESIGN
	float hit_whiten = 0.7f;              // DESIGN: hit smoke this far toward white
	float death_smoke[3] = {0.66f, 0.60f, 0.52f};   // DESIGN
	float death_smoke_alpha = 0.32f;      // DESIGN
	float death_smoke_size = 0.55f;       // DESIGN
	float hit_smoke_alpha = 0.55f;        // DESIGN
	int per_frame = 600;                  // DESIGN: particles per drawn frame
	int blasts_per_frame = 96;            // DESIGN
	float air_above = 24.0f;              // DESIGN: elmos
	float surface = 3.0f;                 // DESIGN
	int bubble_count = 6;                 // DESIGN: per class step (1 .. 5)
	float bubble_size = 2.0f;             // DESIGN
	float bubble_rise = 36.0f;            // DESIGN: elmos per s
	float bubble_life = 1.333f;           // DESIGN
	// DESIGN: by blast class (small arms, tank shell, artillery, building death, experimental death)
	BlastClassLook classes[BLAST_CLASSES] = {
		{1, 5.0f, 7.0f, 1.0f, 15.0f, 3, 1.6f, 66.0f, 0.467f, 2200.0f},
		{3, 5.0f, 7.0f, 0.8f, 15.0f, 5, 2.2f, 102.0f, 0.733f, 4000.0f},
		{5, 18.0f, 30.0f, 2.333f, 24.0f, 12, 2.8f, 138.0f, 1.0f, 6000.0f},   // KT1 lint: allow frame-literal sizes in elmos
		{9, 30.0f, 50.0f, 3.667f, 30.0f, 22, 3.4f, 165.0f, 1.333f, 9000.0f},   // KT1 lint: allow frame-literal sizes in elmos
		{18, 60.0f, 100.0f, 5.0f, 39.0f, 44, 4.5f, 240.0f, 1.667f, 14000.0f},
	};
	// DESIGN: by blast family (cannon, beam, missile, flak, torpedo, plasma, flame)
	BlastFamilyLook families[BLAST_FAMILIES] = {{0.6f, 1.0f, false}, {0.2f, 1.3f, false}, {0.9f, 1.0f, false},
		{1.0f, 1.4f, true}, {0.0f, 0.3f, false}, {0.4f, 0.8f, false}, {1.5f, 0.3f, false}};
	template<class S> void Describe(S& s) {
		s.Field("smoke_max", smoke_max);
		s.Field("spark_max", spark_max);
		s.Field("bubble_max", bubble_max);
		s.Field("fire_max", fire_max);
		s.Field("gravity", gravity);
		s.Field("view_pad", view_pad);
		s.Field("fire_size", fire_size);
		s.Field("fire_life", fire_life);
		s.Field("hit_white", hit_white);
		s.Field("hit_whiten", hit_whiten);
		s.Field("death_smoke", death_smoke);
		s.Field("death_smoke_alpha", death_smoke_alpha);
		s.Field("death_smoke_size", death_smoke_size);
		s.Field("hit_smoke_alpha", hit_smoke_alpha);
		s.Field("per_frame", per_frame);
		s.Field("blasts_per_frame", blasts_per_frame);
		s.Field("air_above", air_above);
		s.Field("surface", surface);
		s.Field("bubble_count", bubble_count);
		s.Field("bubble_size", bubble_size);
		s.Field("bubble_rise", bubble_rise);
		s.Field("bubble_life", bubble_life);
		static constexpr const char* CLASS_KEYS[BLAST_CLASSES] = {"small_arms", "tank_shell", "artillery", "building_death",
			"experimental_death"};
		static constexpr const char* FAMILY_KEYS[BLAST_FAMILIES] = {"cannon", "beam", "missile", "flak", "torpedo", "plasma",
			"flame"};
		for (int i = 0; i < BLAST_CLASSES; ++i) s.Object(CLASS_KEYS[i], [&](auto& o) { classes[i].Describe(o); });
		for (int i = 0; i < BLAST_FAMILIES; ++i) s.Object(FAMILY_KEYS[i], [&](auto& o) { families[i].Describe(o); });
	}
};

struct BlastView {                        // the viewer's side of one blast
	bool effects = true, on = true, limiting = false, guard = true;
	float dist = 0.0f;
	float smokeWeight = 1.0f;             // the faction's
};

struct BlastPlan {
	int32_t smoke = 0, debris = 0, bubbles = 0;
	SmokeLook look = SmokeLook::Smoke;
	const char* skipped = nullptr;        // why nothing is drawn
};

struct BlastStats {
	uint32_t blasts = 0, smoke = 0, debris = 0, bubbles = 0;
	uint32_t skipGuard = 0, skipSwitch = 0, skipOff = 0, skipFar = 0, skipView = 0, skipCap = 0;
};

struct BlastState {
	KT1::FxDraw::ParticlePool smoke, spark, bubble, fire;
	KT1::FxDraw::Random random;
	uint32_t frame = 0;
	int32_t used = 0, blastsThisFrame = 0;
	BlastStats stats;
};

Core::Fx::Medium MediumOf(const BlastConfig& c, float y, float groundHeight);
BlastPlan PlanBlast(const BlastConfig& c, Core::Fx::BlastClass cls, Core::Fx::BlastFamily family, Core::Fx::Medium medium,
	const BlastView& view);
int32_t Admit(const BlastConfig& c, int32_t used, int32_t wanted);
// writes the plan's particles (scaled by what the frame admits)
void EmitBlast(BlastState& s, const BlastConfig& c, const Core::Fx::Blast& e, const BlastPlan& plan,
	Core::Fx::Medium medium, const KT1::FxDraw::FactionLook& faction, float now, uint32_t frame);

} // namespace KT1::FxFields

#endif
