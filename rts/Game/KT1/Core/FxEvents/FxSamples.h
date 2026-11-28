/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 EFFECTS family: what the family's synced system publishes for its draw systems (Host/Snapshot<T>, one per type),
// so no draw file includes a Sim/ header. Publisher: Rules/FxEvents; readers: Draw/Fx*, through DrawView::Snapshots<T>().
// Plain values only. No engine include.
//
// Def tables (WeaponFx, UnitDefFx, FeatureDefFx) are keyed by def id, filled and committed once at Start. Per-step samples
// are committed at each step's end. A draw system of the family therefore needs the fxevents system on.

#ifndef KT1_FX_SAMPLES_H
#define KT1_FX_SAMPLES_H

#include <cstdint>

#include "Game/KT1/Core/FxEvents/FxTypes.h"

namespace KT1::Core::Fx {

constexpr int32_t NAME_LEN = 24;     // DESIGN: longer class and variant names are cut
constexpr int32_t BUILD_POINTS = 8;  // DESIGN: target piece points a weld zap picks from (gfx_build_fx.lua NewZap)
constexpr int32_t TRACK_FEET = 8;    // DESIGN: legs whose foot position is published
constexpr int32_t CHARGE_WEAPONS = 4;   // DESIGN: charged weapons of one unit whose barrel tip is published

// ---------------------------------------------------------------- def tables

// engine weapon types the effects tell apart (LuaUI/fx/muzzle.lua M.TYPES and M.NONE)
enum class WeaponType : uint8_t {
	Other, Cannon, EmgCannon, LaserCannon, BeamLaser, LightningCannon, MissileLauncher, StarburstLauncher, Flame, DGun,
	AircraftBomb, TorpedoLauncher, Shield, Melee
};

namespace WeaponFlag {
constexpr uint32_t Pooled = 1u << 0;          // customParams fx_pooled
constexpr uint32_t Charged = 1u << 1;         // shot_cost_energy > 0
constexpr uint32_t XlBeam = 1u << 2;          // beam of tier xl, or variant orbital
constexpr uint32_t Depth = 1u << 3;           // fx_class depth
constexpr uint32_t DeathWeapon = 1u << 4;     // fx_blast.lua DEATH_OF_WEAPON
constexpr uint32_t OwnedElsewhere = 1u << 5;  // fx_hit_owner strategic
constexpr uint32_t Gauss = 1u << 6;           // a naval rail gun of tier l or xl (gfx_criteria_fx.lua)
constexpr uint32_t MuzzleOff = 1u << 7;       // fx_muzzle 0
constexpr uint32_t MuzzleColour = 1u << 8;    // muzzleRgb is fx_muzzle_color
constexpr uint32_t BeamColour = 1u << 9;      // beamRgb is the weapon's visual colour
constexpr uint32_t Bomb = 1u << 10;           // fx_class bomb
}

struct WeaponFx {
	char fxClass[NAME_LEN] = {0};
	char fxVariant[NAME_LEN] = {0};
	RosterKey look = NO_KEY;                  // the shot recipe's key (class, faction, tier, variant)
	WeaponType type = WeaponType::Other;
	Faction faction = Faction::Human;
	Tier tier = Tier::S;
	BlastClass blastClass = BlastClass::None;
	BlastFamily blastFamily = BlastFamily::None;
	uint32_t flags = 0;
	float damage = 0.0f;                      // default damage
	float damageAreaOfEffect = 0.0f, areaOfEffect = 0.0f;
	float muzzleRgb[3] = {0.0f, 0.0f, 0.0f};
	float beamRgb[3] = {0.0f, 0.0f, 0.0f};
};

namespace UnitDefFlag {
constexpr uint32_t Building = 1u << 0;
constexpr uint32_t Immobile = 1u << 1;
constexpr uint32_t CanFly = 1u << 2;
constexpr uint32_t CanMove = 1u << 3;
constexpr uint32_t Command = 1u << 4;
constexpr uint32_t Builder = 1u << 5;          // isBuilder or buildSpeed > 0
constexpr uint32_t CanCloak = 1u << 6;         // canCloak, int_cloak or int_cloak_param
constexpr uint32_t CloakByDef = 1u << 7;       // int_cloak 1
constexpr uint32_t Giant = 1u << 8;            // eco_tier 4
constexpr uint32_t TreadMarks = 1u << 9;       // pm_desc a_trail has tread_marks
constexpr uint32_t AnimatedDeath = 1u << 10;   // fx_criteria.lua ANIMATED
}

enum class Locomotion : uint8_t { Fixed, Treads, Wheels, Legs, Hover, Rotor };
enum class AmbientKind : uint8_t { None, Power, Extract, Jammer };

struct UnitDefFx {
	char name[NAME_LEN] = {0};
	uint32_t flags = 0;
	float radius = 0.0f, height = 0.0f, speed = 0.0f;
	float buildDistance = 0.0f;
	int16_t xsize = 0, zsize = 0;              // engine squares
	Faction faction = Faction::Human;
	DeathKind deathKind = DeathKind::Mobile;
	AmbientKind ambient = AmbientKind::None;
	uint8_t buildSlot = 0;                     // WVFX-BLD slot of the faction letter; 0: none
	// the movement descriptor (customParams pm_desc), for tracks
	Locomotion loco = Locomotion::Fixed;
	uint8_t legs = 0;
	uint8_t parts = 0;                         // nparts; 0: not given
	float descW = 0.0f, descL = 0.0f, descVmax = 0.0f, descGait = 1.0f, descStride = 0.0f;
};

namespace FeatureDefFlag {
constexpr uint32_t UnitWreck = 1u << 0;        // pm_unit_wreck 1
}

struct FeatureDefFx {
	uint32_t flags = 0;
	float radius = 0.0f, height = 0.0f;
	uint8_t treePoints = 0;                    // knockable_single 1, knockable_cluster 3, else 0
};

// ---------------------------------------------------------------- per step

struct ShotSample {                            // keyed by projectile id: every weapon and piece projectile
	Vec3 pos, vel;                             // vel: elmos per step
	int32_t weaponDef = -1;                    // -1: none (a piece)
	int32_t owner = -1, ownerAllyTeam = -1;
	SeenMask seen = 0;
	uint8_t piece = 0;
};

namespace UnitFlag {
constexpr uint16_t Active = 1u << 0;
constexpr uint16_t Cloaked = 1u << 1;          // the engine's cloak or rules param int_cloak
constexpr uint16_t BeingBuilt = 1u << 2;
constexpr uint16_t Carried = 1u << 3;
}

struct UnitFxSample {                          // keyed by unit id: every unit
	Vec3 pos, vel, front;
	float health = 0.0f, maxHealth = 0.0f, build = 1.0f;
	float radius = 0.0f, height = 0.0f;
	uint16_t flags = 0;
	uint8_t facing = 0;
};

struct FeatureFxSample {                       // keyed by feature id: plants and unit wrecks only
	Vec3 pos;
	float rotX = 0.0f, rotZ = 0.0f;            // radians
	int32_t defId = -1;
	uint8_t canIgnite = 1;                     // feature rules param plant_can_ignite (absent: yes)
};

enum class BuildWork : uint8_t { Build, Repair, Reclaim, Capture };

struct BuildSample {                           // keyed by builder unit id: builders at work
	BuildWork kind = BuildWork::Build;
	uint8_t targetIsFeature = 0;
	uint8_t builderMoving = 0;
	uint8_t points = 0;                        // entries used of targetPieces
	int32_t target = -1;
	Vec3 source;                               // the nano piece, else near the builder's top
	Vec3 targetMid;
	float targetBase = 0.0f, targetRadius = 0.0f, targetHeight = 0.0f, targetBuild = 1.0f;
	Vec3 targetPieces[BUILD_POINTS];           // a spread of the target's piece positions
};

struct TrackSample {                           // keyed by unit id: legged units
	uint8_t feet = 0;
	float footX[TRACK_FEET] = {}, footZ[TRACK_FEET] = {};
};

struct MuzzleSample {                          // keyed by unit id: units with a charged weapon (the charge glows)
	uint8_t count = 0;                         // entries used
	uint8_t weapon[CHARGE_WEAPONS] = {};       // the weapon number, from 1
	Vec3 muzzle[CHARGE_WEAPONS];               // its barrel tip
};

} // namespace KT1::Core::Fx

#endif
