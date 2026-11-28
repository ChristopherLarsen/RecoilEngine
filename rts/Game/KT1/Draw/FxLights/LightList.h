/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 FxLights: the muzzle flash lights of a frame (replaces LuaUI/fx/muzzle.lua and the planning of gfx_fx_lights.lua):
// which weapons flash and how, the merge and budget of one step's shots, and the lists the system draws (engine point
// lights and ground glows; spec column lights = position, radius, colour). Engine-free.

#ifndef KT1_FX_LIGHT_LIST_H
#define KT1_FX_LIGHT_LIST_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/FxCommon/FxShapes.h"

namespace KT1::FxLights {

using Core::Fx::Vec3;

struct MuzzleTypeRow {
	std::string weapon_type;          // DESIGN: the engine weapon type
	float color[3] = {1.0f, 1.0f, 1.0f};   // DESIGN
	float radius = 0.0f;              // DESIGN: elmos
	float life_s = 0.0f;              // DESIGN
	bool beam_color = false;          // DESIGN: take the beam's own colour, brightened toward white
	template<class S> void Describe(S& s) {
		s.Field("weapon_type", weapon_type);
		s.Field("color", color);
		s.Field("radius", radius);
		s.Field("life_s", life_s);
		s.Field("beam_color", beam_color);
	}
};

struct LightsConfig {                 // game/data/kt1/fxlights.json
	// DESIGN: by engine weapon type; a type without a row takes the Cannon row (bombs, torpedoes, shields, melee: none)
	std::vector<MuzzleTypeRow> types = {
		{"Cannon", {1.00f, 0.66f, 0.32f}, 180.0f, 0.167f, false},
		{"EmgCannon", {1.00f, 0.88f, 0.48f}, 140.0f, 0.167f, false},
		{"LaserCannon", {0.45f, 0.85f, 1.00f}, 160.0f, 0.167f, true},
		{"BeamLaser", {0.45f, 0.85f, 1.00f}, 180.0f, 0.2f, true},
		{"LightningCannon", {0.62f, 0.74f, 1.00f}, 220.0f, 0.2f, false},
		{"MissileLauncher", {1.00f, 0.84f, 0.56f}, 160.0f, 0.2f, false},
		{"StarburstLauncher", {1.00f, 0.80f, 0.50f}, 240.0f, 0.2f, false},
		{"Flame", {1.00f, 0.46f, 0.14f}, 140.0f, 0.167f, false},
		{"DGun", {1.00f, 0.52f, 0.90f}, 240.0f, 0.2f, false},
	};
	float radius_min = 80.0f;         // DESIGN: elmos
	float radius_max = 440.0f;        // DESIGN
	float damage_ref = 100.0f;        // DESIGN: a shot of this damage gets the type's radius; reach grows with its root
	float glow_min = 0.7f;            // DESIGN: ground glow strength by weapon size
	float glow_max = 1.6f;            // DESIGN
	float ground_light = 0.25f;       // DESIGN: owner ruling 2026-11-09: glow and light drawn at this share
	float attack = 0.15f;             // DESIGN: share of a flash's life spent rising
	float decay_power = 1.5f;         // DESIGN
	float light_end = 0.2f;           // DESIGN: share of an engine light's colour left at the end
	float merge = 24.0f;              // DESIGN: elmos: closer flashes of one step become one, brighter
	float merge_bright = 0.5f;        // DESIGN: each merged shot adds this
	float bright_max = 2.0f;          // DESIGN
	int shots_max = 256;              // DESIGN: shots of one step looked at
	int glow_budget = 48;             // DESIGN: ground glows started per step
	int glows_alive = 1024;           // DESIGN: glows alive
	float at_owner = 48.0f;           // DESIGN: elmos beyond the owner's radius and one step of flight that count as just fired
	float light_lift = 14.0f;         // DESIGN: elmos above the muzzle
	float glow_scale = 1.3f;          // DESIGN: glow radius as a share of the light's
	float glow_alpha = 1.0f;          // DESIGN
	float light_gain = 1.6f;          // DESIGN: engine light colour gain
	float light_specular = 0.5f;      // DESIGN
	float quad_lift = 2.0f;           // DESIGN: the glow quad above the ground
	int engine_lights = 8;            // DESIGN: at most this many engine lights (and the engine's own limits)
	std::string gate = "fx_lights";   // DESIGN: zoom gate
	template<class S> void Describe(S& s) {
		s.List("types", types, [](auto& o, MuzzleTypeRow& r) { r.Describe(o); });
		s.Field("radius_min", radius_min);
		s.Field("radius_max", radius_max);
		s.Field("damage_ref", damage_ref);
		s.Field("glow_min", glow_min);
		s.Field("glow_max", glow_max);
		s.Field("ground_light", ground_light);
		s.Field("attack", attack);
		s.Field("decay_power", decay_power);
		s.Field("light_end", light_end);
		s.Field("merge", merge);
		s.Field("merge_bright", merge_bright);
		s.Field("bright_max", bright_max);
		s.Field("shots_max", shots_max);
		s.Field("glow_budget", glow_budget);
		s.Field("glows_alive", glows_alive);
		s.Field("at_owner", at_owner);
		s.Field("light_lift", light_lift);
		s.Field("glow_scale", glow_scale);
		s.Field("glow_alpha", glow_alpha);
		s.Field("light_gain", light_gain);
		s.Field("light_specular", light_specular);
		s.Field("quad_lift", quad_lift);
		s.Field("engine_lights", engine_lights);
		s.Field("gate", gate);
	}
};

struct Flash {                        // a weapon def's flash
	bool on = false;
	KT1::FxDraw::Rgb color;
	float radius = 0.0f, life = 0.0f, glow = 1.0f;
	float decayPerStep = 1.0f;        // the engine light's colour factor per step
};

struct ShotLight { Vec3 pos; int32_t weaponDef = -1; };                 // a new shot, at its muzzle
struct PlannedLight { Vec3 pos; int32_t weaponDef = -1; float bright = 1.0f; };

struct Light {                        // one light of the list
	Vec3 pos;
	KT1::FxDraw::Rgb color;           // gain, share and brightness applied
	float radius = 0.0f, born = 0.0f, life = 0.0f;
};

struct LightStats { uint32_t shots = 0, flashes = 0, merged = 0, engine = 0, glows = 0, skipped = 0, dropped = 0; };

struct LightList {
	std::vector<Flash> flashes;                                         // by weapon def id
	std::vector<Light> engineLights;                                    // alive, at most the engine limit
	KT1::FxDraw::InstanceList<KT1::FxDraw::GroundQuad> glows;
	std::vector<PlannedLight> plan;
	LightStats stats;
};

Flash FlashOf(const LightsConfig& c, const Core::Fx::WeaponFx& weapon, float stepSeconds);
float Envelope(const LightsConfig& c, float age);                       // age 0 .. 1
Vec3 MuzzleOf(const Vec3& pos, const Vec3& velPerStep);                 // one step back along the velocity
bool JustFired(const LightsConfig& c, const Vec3& shot, const Vec3& velPerStep, const Vec3& owner, float ownerRadius);
// merges shots closer than c.merge with the same flash; over the budget the nearest to `camera` win. Returns the dropped.
int32_t PlanFlashes(const LightsConfig& c, const LightList& list, std::span<const ShotLight> shots, int32_t budget,
	const Vec3& camera, std::vector<PlannedLight>& out);
void InitLights(LightList& list, const LightsConfig& c, int32_t engineLimit);
void AddFlashes(LightList& list, const LightsConfig& c, std::span<const PlannedLight> plan, float now);
void ExpireLights(LightList& list, float now);

} // namespace KT1::FxLights

#endif
