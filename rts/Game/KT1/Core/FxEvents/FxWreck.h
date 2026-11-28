/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: wrecks that slide down slopes and are pushed and tumbled by explosions (replaces
// code/LuaRules/Gadgets/fx_wreck_physics.lua and gamedata/wreck_values.lua). No engine include.
//
// Speeds are elmos per second and rates per second here (the Lua's are per frame at 30 Hz); WreckParams turns them into
// per step values once (Core::Rate). Wrecks in motion keep the Lua's order: started order, the last one taking a dropped
// one's place, which is DenseIndex's swap remove.

#ifndef KT1_FX_WRECK_H
#define KT1_FX_WRECK_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/FxEvents/FxTypes.h"
#include "Game/KT1/Core/FxEvents/FxWorld.h"

namespace KT1::Core::Fx {

struct FxOutput;

struct WreckConfig {
	bool enabled = true;                     // DESIGN
	// slide
	float static_friction = 0.30f;           // PLACEHOLDER: a wreck rests where the ground normal's horizontal part is at most this
	float kinetic_friction = 0.20f;          // PLACEHOLDER: a moving wreck slides while the horizontal normal exceeds this
	float drag_step_s = 1.0f / 30.0f;        // KT1 lint: allow frame-literal PLACEHOLDER: the time over which the two drag shares are kept
	float drag = 0.96f;                      // PLACEHOLDER: share of its speed a sliding wreck keeps over drag_step_s
	float water_drag = 0.80f;                // PLACEHOLDER: the same under water
	float stop_speed = 1.5f;                 // PLACEHOLDER: elmos per s; slower on a resting slope, a wreck stops (0.05 a frame)
	float air_height = 0.5f;                 // PLACEHOLDER: higher than this above the ground counts as in the air
	float edge_margin = 8.0f;                // PLACEHOLDER: wrecks stay this far inside the map edge
	// damage
	float explosion_damage_scale = 1.0f;     // DESIGN: share of an explosion's damage a wreck takes (read by ECO's wrecks)
	// push
	float push_min_damage = 150.0f;          // PLACEHOLDER: weaker weapons move no wreck
	float push_min_radius = 24.0f;           // PLACEHOLDER: nor smaller explosions
	float push_ref_damage = 1000.0f;         // PLACEHOLDER: the damage at which a massless wreck gets max_speed
	float push_max_damage_mul = 1.0f;        // PLACEHOLDER: cap of the damage factor
	float push_max_speed = 75.0f;            // PLACEHOLDER: elmos per s at the centre (2.5 a frame)
	float push_mass_ref = 100.0f;            // PLACEHOLDER: a wreck holding this much mass takes half the push
	// tumble
	float tumble_min_damage = 400.0f;        // PLACEHOLDER: explosions this strong throw wrecks into the air
	float tumble_min_share = 0.3f;           // PLACEHOLDER: only wrecks this deep inside the radius (1 at the centre)
	float tumble_up_speed = 90.0f;           // PLACEHOLDER: elmos per s upward at the centre (3 a frame)
	float tumble_spin = 7.5f;                // PLACEHOLDER: radians per s about each axis at the centre (0.25 a frame)
	float bounce = 0.35f;                    // PLACEHOLDER: share of the downward speed kept on landing
	float bounce_min = 24.0f;                // PLACEHOLDER: elmos per s; a slower landing does not bounce (0.8 a frame)
	float bounce_side = 0.7f;                // PLACEHOLDER: share of the sideways speed a bounce keeps
	float bounce_spin = 0.6f;                // PLACEHOLDER: share of the spin a bounce keeps
	float axis_step = 0.25f;                 // PLACEHOLDER: the placing number makes the three spins differ by steps of this
	float ground_ease = 0.8f;                // PLACEHOLDER: share of its tilt a resting wreck keeps over drag_step_s
	float ground_ease_zero = 0.01f;          // PLACEHOLDER: radians; a smaller tilt is set to zero
	// limits
	int max_moving = 256;                    // PLACEHOLDER: wrecks in motion at once
	int per_explosion = 12;                  // PLACEHOLDER: wrecks one explosion moves, nearest first
	int explosions_a_step = 8;               // PLACEHOLDER: explosions looked at each step
	// sound
	float thud_gap_s = 0.4f;                 // PLACEHOLDER: between two thuds anywhere
	float thud_wreck_gap_s = 1.5f;           // PLACEHOLDER: before one wreck thuds again
	float thud_min_drop = 45.0f;             // PLACEHOLDER: elmos per s downward; slower lands silently (1.5 a frame)
	template<class S> void Describe(S& s) {
		s.Field("enabled", enabled);
		s.Field("static_friction", static_friction);
		s.Field("kinetic_friction", kinetic_friction);
		s.Field("drag_step_s", drag_step_s);
		s.Field("drag", drag);
		s.Field("water_drag", water_drag);
		s.Field("stop_speed", stop_speed);
		s.Field("air_height", air_height);
		s.Field("edge_margin", edge_margin);
		s.Field("explosion_damage_scale", explosion_damage_scale);
		s.Field("push_min_damage", push_min_damage);
		s.Field("push_min_radius", push_min_radius);
		s.Field("push_ref_damage", push_ref_damage);
		s.Field("push_max_damage_mul", push_max_damage_mul);
		s.Field("push_max_speed", push_max_speed);
		s.Field("push_mass_ref", push_mass_ref);
		s.Field("tumble_min_damage", tumble_min_damage);
		s.Field("tumble_min_share", tumble_min_share);
		s.Field("tumble_up_speed", tumble_up_speed);
		s.Field("tumble_spin", tumble_spin);
		s.Field("bounce", bounce);
		s.Field("bounce_min", bounce_min);
		s.Field("bounce_side", bounce_side);
		s.Field("bounce_spin", bounce_spin);
		s.Field("axis_step", axis_step);
		s.Field("ground_ease", ground_ease);
		s.Field("ground_ease_zero", ground_ease_zero);
		s.Field("max_moving", max_moving);
		s.Field("per_explosion", per_explosion);
		s.Field("explosions_a_step", explosions_a_step);
		s.Field("thud_gap_s", thud_gap_s);
		s.Field("thud_wreck_gap_s", thud_wreck_gap_s);
		s.Field("thud_min_drop", thud_min_drop);
	}
};

// the config in per step values (MakeWreckParams, at Start)
struct WreckParams {
	float drag = 0.0f, waterDrag = 0.0f, groundEase = 0.0f, stopSpeed = 0.0f, pushSpeed = 0.0f, upSpeed = 0.0f, spin = 0.0f;
	float bounceMin = 0.0f, thudMinDrop = 0.0f;
	int32_t thudGap = 0, thudWreckGap = 0;
};

struct WreckPushWeapon { float damage = 0.0f, radius = 0.0f; bool moves = false, tumbles = false; };

// Wreck state. `placed` holds every wreck that may be moved (ECO tells it); `moving` those in motion (spec column
// wreck_impulse and the Lua's S[f]).
struct WreckState {
	DenseIndex placed;                        // by feature id
	Column<int32_t> seq;                      // the number it was placed as (feature ids differ between games)
	Column<Vec3> pose;                        // the rotation it lies in as its death left it (GG.WreckPose)
	Column<uint8_t> posed;
	Column<int32_t> thudAt;

	DenseIndex moving;                        // by feature id
	Column<Vec3> impulse;                     // spec wreck_impulse: this step's push written to the wreck
	Column<Vec3> vel;                         // elmos per step
	Column<Vec3> rot;                         // radians, added to the pose
	Column<Vec3> spin;                        // radians per step
	Column<uint8_t> air, tilted;

	int32_t nextSeq = 0;
	int32_t lastThud = -1;                    // step of the last thud anywhere; -1: none
	int32_t budgetStep = -1, budget = 0;      // explosions still looked at this step
};

struct WreckNear { int32_t feature = -1; Vec3 pos; };

WreckParams MakeWreckParams(const WreckConfig& c);
void InitWrecks(WreckState& s, int32_t maxFeatures);
WreckPushWeapon PushWeaponOf(const WreckConfig& c, float damage, float areaOfEffect);

void PlaceWreck(WreckState& s, const WreckConfig& c, int32_t feature, const WorldQuery& world);   // GG.WreckPhysics.Placed
void ForgetWreck(WreckState& s, int32_t feature, FxOutput& out);                                  // the feature is gone
bool PushWreck(WreckState& s, const WreckConfig& c, int32_t feature, float vx, float vz, float up, float spin);   // per step values
bool WreckMoving(const WreckState& s, int32_t feature);
void SetWreckPose(WreckState& s, int32_t feature, const Vec3& rot);
void ClearWreckPose(WreckState& s, int32_t feature);
// near: the wrecks inside the weapon's radius, any order (sorted by distance, then id, here)
void ExplodeNearWrecks(WreckState& s, const WreckConfig& c, const WreckParams& p, const WreckPushWeapon& w, int32_t weaponDef,
	const Vec3& at, std::vector<WreckNear>& near, int32_t step, const WorldQuery& world, FxOutput& out);
// every wreck in motion, one step; writes, releases and thuds go to `out`
void StepWrecks(WreckState& s, const WreckConfig& c, const WreckParams& p, int32_t step, const WorldQuery& world, FxOutput& out);

} // namespace KT1::Core::Fx

#endif
