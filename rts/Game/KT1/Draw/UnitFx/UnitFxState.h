/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 UnitFx: state and logic of the unit and world effects, as free functions (replaces gfx_unit_fx.lua): damage smoke
// and fire, lingering wreck smoke, footfalls, teleport, charge glows, sacrifice, remnants, falling aircraft, damage over
// time ticks, trails, structure ambience, craters, burning plants. Spec column emitters = the pools' instances.
// Engine-free: the system (UnitFxApi.h) reads the view and the snapshots and passes plain values in.

#ifndef KT1_UNIT_FX_STATE_H
#define KT1_UNIT_FX_STATE_H

#include <cstdint>
#include <span>
#include <vector>

#include "Game/KT1/Core/Common/Column.h"
#include "Game/KT1/Core/Common/DenseIndex.h"
#include "Game/KT1/Core/Damage/CombatEvents.h"
#include "Game/KT1/Core/FxEvents/FxEvent.h"
#include "Game/KT1/Core/FxEvents/FxSamples.h"
#include "Game/KT1/Draw/FxCommon/FxParticles.h"
#include "Game/KT1/Draw/FxCommon/FxRandom.h"
#include "Game/KT1/Draw/FxCommon/FxRoster.h"
#include "Game/KT1/Draw/UnitFx/EmitterKey.h"
#include "Game/KT1/Draw/UnitFx/UnitFxConfig.h"

namespace KT1::UnitFx {

using Core::Fx::Vec3;

enum class Pool : uint8_t { Smoke, Spark, Glow, Wreck, SpriteSmoke, SpriteFire, SpriteSpark, SpriteDust, SpriteFlash, Count };

// a unit as the caller sees it this frame
struct SeenUnit {
	int32_t unit = -1, defId = -1;
	Vec3 pos;                         // drawn position
	float radius = 0.0f, height = 0.0f;
	float dist = 0.0f;                // camera distance
	float groundY = 0.0f;
	bool icon = false, visible = false, allied = false;
	const Core::Fx::UnitFxSample* sample = nullptr;
	const Core::Fx::UnitDefFx* def = nullptr;
};

struct Frame {                        // what every call needs
	const UnitFxConfig& config;
	const KT1::FxDraw::RosterConfig& roster;
	KT1::FxDraw::FxTime time;
	float scale = 1.0f;               // the effect option
	bool fullView = false;
};

struct Lingering { Vec3 pos; float size = 0.0f, until = 0.0f, next = 0.0f; Core::Fx::Faction faction{}; };
struct Charging { int32_t unit = -1, weapon = 0; float progress = 0.0f, last = 0.0f, stopped = -1.0f; Vec3 muzzle; };
struct Teleporting { int32_t unit = -1; Vec3 from, to; float start = 0.0f, seconds = 0.0f, next = 0.0f; };
struct Flow { Vec3 from, to; float start = 0.0f; };          // sacrifice and remnant runs
struct Falling { int32_t unit = -1; float next = 0.0f; };
struct Crater { Vec3 pos; int32_t decal = -1; float born = 0.0f, life = 0.0f; };
struct BurningPlant { int32_t feature = -1, points = 1; Vec3 pos; float height = 0.0f, start = 0.0f, burn = 0.0f; uint8_t charred = 0; };

struct UnitFxState {
	KT1::FxDraw::ParticlePool pools[static_cast<int>(Pool::Count)];   // spec column emitters
	KT1::FxDraw::Random random;

	Core::DenseIndex damaged;                          // units drawing a damage state, by unit id
	Core::Column<EmitterKey> key;
	Core::Column<float> health;                        // at the last look: a fall while heavy bursts
	Core::Column<float> since, nextEmit, size;
	Core::Column<Vec3> pos;
	float nextPoll = 0.0f, nextAmbientPoll = 0.0f;

	std::vector<int32_t> ambient;                // structures showing an ambient effect, nearest first
	std::vector<Lingering> lingering;
	std::vector<Charging> charging;
	std::vector<Teleporting> teleports;
	std::vector<Flow> sacrifices, remnants;
	std::vector<Falling> falling;
	std::vector<Crater> craters;
	std::vector<BurningPlant> burning;
	std::vector<int32_t> burnt;                  // plants that burned once, sorted
	int32_t dotTicksThisFrame = 0;
	uint32_t dotFrame = 0;
};

void InitState(UnitFxState& s, const UnitFxConfig& c, int32_t maxUnits);
// every config.poll_s: which units draw damage states, nearest first up to the cap; lingering smoke of the ones gone
void PollDamage(UnitFxState& s, const Frame& f, std::span<const SeenUnit> units);
void PollAmbient(UnitFxState& s, const Frame& f, std::span<const SeenUnit> structures);
void EmitDamage(UnitFxState& s, const Frame& f);
void EmitAmbient(UnitFxState& s, const Frame& f, std::span<const SeenUnit> structures);
void EmitTimed(UnitFxState& s, const Frame& f);              // lingering, charging, teleports, flows, falling, burning
// trails of flying pieces (shots), thrown wrecks (features) and high fast aircraft
void EmitPieceTrails(UnitFxState& s, const Frame& f, std::span<const Core::Fx::ShotSample> pieces);
void EmitWreckTrails(UnitFxState& s, const Frame& f, std::span<const Core::Fx::FeatureFxSample> wrecksInAir);
void EmitContrails(UnitFxState& s, const Frame& f, std::span<const SeenUnit> aircraft);

void Footfall(UnitFxState& s, const Frame& f, const Vec3& foot, float groundY, Core::Fx::SizeBucket size, int32_t legs,
	int32_t step, const float biomeRgb[3]);
void OnTeleport(UnitFxState& s, const Frame& f, const Core::Fx::Teleport& e, Core::Fx::Faction faction, bool allied);
void OnSacrifice(UnitFxState& s, const Frame& f, const Core::Fx::Sacrifice& e, bool seen);
void OnRemnant(UnitFxState& s, const Frame& f, const Core::Fx::Remnant& e);
void OnCrash(UnitFxState& s, const Frame& f, const Core::Fx::Crash& e);
void OnCharge(UnitFxState& s, const Frame& f, const Core::TimedRules::ChargeProgress& e, const Vec3& muzzle);
void OnChargeFired(UnitFxState& s, const Frame& f, const Core::TimedRules::ChargeFired& e);
// at: the unit's place now for a single target, else the event's point
void OnDotTick(UnitFxState& s, const Frame& f, const Core::Damage::DotTick& e, const Vec3& at, float unitRadius);

// A blast's craters, burning plants and charged or XL beam marks. A scorch the beam or charged hit asks for goes to the
// caller's decals (Draw/Decals).
struct BlastIn {
	Core::Fx::BlastClass cls = Core::Fx::BlastClass::None;
	Vec3 pos;
	float groundY = 0.0f;
	const Core::Fx::WeaponFx* weapon = nullptr;
};
struct BurnCandidate { int32_t feature = -1, points = 0; Vec3 pos; float height = 0.0f; };
struct MarkAsk { float x = 0.0f, z = 0.0f, size = 0.0f, life = 0.0f; };
void OnBlast(UnitFxState& s, const Frame& f, const BlastIn& in, std::span<const BurnCandidate> plants,
	std::vector<MarkAsk>& marks, std::vector<int32_t>& newCraters);

int32_t CapAt(const std::vector<CapPoint>& points, float scale);     // linear between the points

} // namespace KT1::UnitFx

#endif
