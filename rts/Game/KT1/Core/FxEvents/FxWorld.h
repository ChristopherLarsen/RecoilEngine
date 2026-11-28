/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS core: what the synced effect logic asks of the world, and what it asks to be done. Rules/FxEvents fills the
// queries from the engine (synced reads, the synced random generator) and carries the outputs out. No engine include.

#ifndef KT1_FX_WORLD_H
#define KT1_FX_WORLD_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::Core::Fx {

struct UnitNow {                      // a live unit, read at the moment of the question
	Vec3 pos, mid;
	float radius = 0.0f, height = 0.0f;
	float health = 0.0f, maxHealth = 0.0f, build = 1.0f;
	float box[3] = {16.0f, 16.0f, 16.0f};   // DESIGN: the collision volume's sizes, elmos (fx_death_sets.lua Sizes' default)
	int32_t defId = -1;
	int32_t team = -1, allyTeam = -1;
	int32_t heading = 0;
	bool beingBuilt = false;
};

struct WorldQuery {                   // every function pointer is set by the caller; ctx is passed back
	void* ctx = nullptr;
	float (*groundHeight)(void* ctx, float x, float z) = nullptr;
	void (*groundNormal)(void* ctx, float x, float z, Vec3& normal) = nullptr;
	bool (*unit)(void* ctx, int32_t unit, UnitNow& out) = nullptr;                      // false: dead
	bool (*featurePos)(void* ctx, int32_t feature, Vec3& out) = nullptr;                // false: gone
	float (*featureMass)(void* ctx, int32_t feature) = nullptr;                         // the metal it still holds
	bool (*unitsNear)(void* ctx, const Vec3& pos, float radius) = nullptr;              // any unit's volume in the sphere
	SeenMask (*seen)(void* ctx, const Vec3& pos) = nullptr;                             // who has the point in sight
	float (*random)(void* ctx) = nullptr;                                               // synced, [0, 1)
	int32_t (*randomRange)(void* ctx, int32_t lo, int32_t hi) = nullptr;                // synced, both ends
	float mapSizeX = 0.0f, mapSizeZ = 0.0f;
	float gravity = 0.0f;             // elmos per step squared
};

// ---------------------------------------------------------------- outputs

// One explosion generator the effect budget admitted, and where.
struct EffectSpawn {
	int32_t effect = NO_EFFECT;
	Vec3 pos;
	Vec3 dir = {0.0f, 1.0f, 0.0f};
	float length = 0.0f;
	Priority priority = Priority::Hit;
};

// An effect name resolved at Start (Rules/FxEvents): what a request for it does.
struct EffectRef {
	int32_t effect = NO_EFFECT;
	int32_t lowEffect = NO_EFFECT;    // drawn instead below scale 1 for a hit of tier L or XL; else the same as effect
	bool pooled = false;              // a pooled look's name: the shot follower draws it, it is never spawned
};

// A wreck's new place and rotation, to be written to the feature.
struct WreckWrite {
	int32_t feature = -1;
	Vec3 pos;
	Vec3 rot;                         // radians
	bool setRotation = false;
};

} // namespace KT1::Core::Fx

#endif
