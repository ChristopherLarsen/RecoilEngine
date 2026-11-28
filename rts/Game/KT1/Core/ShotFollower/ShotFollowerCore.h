/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/ShotFollower: the shot follower's decisions, engine-free (Lua-free plan D05).
// Replaces:  the plan of LuaUI/fx/shots.lua (caps, setting, guard, bands, eligibility, cap order, lives, puff budget) and the per-shot
//            rules of LuaRules/Gadgets/gfx_shots.lua (cut-off gates, puff cadence, path puffs, ribbon and extra lives, the end point,
//            the energy families' seeded jitter). The adapters (Draw/ShotFollower, Draw/Ribbons) read the engine, call these functions
//            and draw; every decision is here so it is tested without an engine build.
// Phase:     draw side, once per sim step (the follower's GameFrame), read by the next drawn frames. Writes no sim state.
// Config:    ShotFollowerConfig below, filled from game/data/kt1/shotfollower.json (F04); every default and its source label here.
// Rate:      every period is seconds, converted with Core::Rate (LUA-FREE-PLAN §2.2 p6). The Lua counted 30 frames a second; where it
//            did, the default is that count over 30 and the rule doc says so. Velocities are elmos per sim step, as the engine gives.
// Labels (LUA-FREE-PLAN §2.2 p10; requirement status in requirements/released/MANIFEST.tsv, 2026-11-27):
//   DESIGN   caps, priority, bands, setting, guard, heavy-fight lightening, puff budget, cadences (owner Q7 2026-10-29, GAME-DESIGN §1;
//            the 60 FPS rule); the cut-off measure (view depth) and its detail-option scale (WVFX-CMD-001 v8 R8a, WVFX-T1D-001 v10 R8
//            and WVFX-TML-001 v8 R9 leave the measure open; WVFX-ZOOM-001, which stated it, is withdrawn).
//   REQ      RND-006 v4 R8: the detail option scales every effect cut-off; WVFX-CMD-001 v8 R8: each layer's cut-off is its data value,
//            -1 none (the value comes in with the layer, not from here).
// Float:     draw only, no determinism need; float throughout, as the Lua number.
// No engine include, no allocation after Init.

#ifndef KT1_CORE_SHOTFOLLOWER_CORE_H
#define KT1_CORE_SHOTFOLLOWER_CORE_H

#include <array>
#include <cstdint>

namespace KT1::Core::ShotFollower {

// ---------------------------------------------------------------- config

struct CapsRow {                       // the caps of one effect option row (KardashevFxScale)
	float scale = 0;                   // the row applies from this option value up
	int32_t ribbons = 0;               // shots that draw a ribbon at once
	int32_t points = 0;                // the most points one ribbon is made of (a longer life takes a point less often)
	int32_t puffsPerSecond = 0;        // puffs started per second, all shots together
};

constexpr int32_t MAX_CAPS_ROWS = 4;
constexpr int32_t MAX_PRIORITY_CLASSES = 16;

struct ShotFollowerConfig {
	// DESIGN (shots.lua S.CAPS; PLACEHOLDER values, to tune at 500 and 1000 units)
	std::array<CapsRow, MAX_CAPS_ROWS> caps = {{ {0.5f, 60, 6, 150}, {1, 200, 12, 600}, {2, 600, 20, 1500}, {} }};
	int32_t capsCount = 3;
	// DESIGN (shots.lua S.HEAVY, owner-approved GAME-DESIGN §1): over `heavyExtras` shots in view no dash or thread; over `heavyLife`
	// new ribbon segments live `heavyLifeK` as long. PLACEHOLDER from the fxlab stress test.
	int32_t heavyExtras = 40;
	int32_t heavyLife = 120;
	float heavyLifeK = 0.5f;
	// DESIGN (shots.lua S.PRIORITY): cap order, lower first; a class not listed takes defaultPriority
	int32_t defaultPriority = 9;
	// DESIGN (shots.lua S.NEAR, MID, FAR, FADE; PLACEHOLDER, elmos of camera distance): near full detail; mid half the points and every
	// second puff; far bodies only, fading over the last `fade` share; beyond far native only
	float nearElmos = 1500, midElmos = 3500, farElmos = 6000, fade = 0.2f;
	// DESIGN (owner Q7 2026-10-29; shots.lua S.SHORT_LIFE, S.GUARD_LIFE): the longest ribbon at the short setting (s); a ribbon's share
	// of its life while the FPS guard limits
	float shortLifeS = 0.5f, guardLifeShare = 0.5f;
	// DESIGN (shots.lua S.UNDER_FADE): a torpedo body's alpha falls by half over this depth (elmos), never under depthAlphaMin
	float underFadeElmos = 80, depthAlphaMin = 0.3f;
	// DESIGN (shots.lua S.PuffBudget): the rolling window of the puff budget (s)
	float puffWindowS = 1;
	// DESIGN (gfx_shots.lua PathPuffs): at most this many path puffs per shot and step; a gap longer than maxGapS counts as maxGapS
	int32_t pathPuffMax = 24;
	float pathPuffMaxGapS = 0.1333333f;     // the Lua's 4 frames at its 30 a second
	// DESIGN (gfx_shots.lua GameFrame): a trail of a set length lives at least minLengthLifeS and at most lengthLifeMul x its life
	float minLengthLifeS = 0.0666667f;      // the Lua's 2 frames at its 30 a second
	float lengthLifeMul = 3;
	float minSpeedElmosPerS = 1;            // slower than this a length trail keeps its own life
	float minSegmentElmos = 0.05f;          // a segment shorter than this is not laid
	float minEndElmos = 0.05f;              // PFX.End: a closing stretch shorter than this is not laid
	// DESIGN (gfx_shots.lua EndPoint): the end point is at least this far above the ground: the impact's and the ribbon's
	float endLiftImpact = 2, endLiftRibbon = 0.5f;
	// DESIGN (gfx_shots.lua PFX.Emit): a muzzle is made where a new shot is first seen within this distance of its owner (elmos)
	float muzzleNearOwnerElmos = 80;
};

// ---------------------------------------------------------------- setting, caps, mode

enum class Setting : int32_t { None = 0, Short = 1, Full = 2 };   // KardashevFxShots (owner Q7 2026-10-29)
constexpr Setting DEFAULT_SETTING = Setting::Short;                // unset reads as Medium's (shots.lua S.DEFAULT)

// the player's setting from the config value; `has` false (unset or not a number) gives the default
Setting ParseSetting(bool has, float value);

// the caps row of an option value: the largest row at or below it (the first row below every row); nullptr when effects are off (<= 0)
const CapsRow* CapsFor(const ShotFollowerConfig& c, float scale);

struct Mode {                          // what the follower may do this step (shots.lua S.Mode)
	bool ribbons = false;              // new ribbons may start
	bool keep = false;                 // ribbons already drawing go on
	bool missilesOnly = false;
	float lifeMul = 1;
	float lifeMaxS = -1;               // < 0: no limit
	bool puffs = false;
	bool pulse = false;                // plasma body pulse
	bool flak = false;                 // flak's faint ribbon (High and up)
};
Mode MakeMode(const ShotFollowerConfig& c, Setting setting, bool limiting, const CapsRow* caps);

// ---------------------------------------------------------------- per shot

enum Band : int32_t { BAND_NEAR = 1, BAND_MID = 2, BAND_FAR = 3, BAND_BEYOND = 4 };
Band BandOf(const ShotFollowerConfig& c, float d2);       // by squared camera distance
float BodyFade(const ShotFollowerConfig& c, float d2);    // 1, to 0 over the far band's last `fade` share
float DepthAlpha(const ShotFollowerConfig& c, float y);   // a body under water (y < 0, elmos below the surface)

bool Visible(bool enemy, bool inLos, bool fullView);      // an enemy shot only in the viewer's sight; all for a full-view spectator
bool Collapsed(bool overview, bool ownerIcon);            // drawn by the engine alone: the full-map view, or its unit is an icon

struct RibbonTraits {                  // the recipe facts eligibility needs
	bool missile = false, flak = false;
	int8_t highOnly = -1;              // -1: as the class (flak only), 0 no, 1 yes
	bool essential = false;            // a bespoke look whose body is its ribbon: drawn at Medium too
	bool layered = false;              // ribbon parts follow only their own cut-offs (no band removes them)
};
bool Eligible(const RibbonTraits& t, Band band, const Mode& mode, bool started);

int32_t PointsFor(const CapsRow& caps, Band band);        // mid: half (at least 1)
float LifeUnderMode(float lifeS, const Mode& mode);       // x lifeMul, at most lifeMaxS
int32_t StrideSteps(float lifeS, int32_t points);         // steps between two points: ceil(life x rate / points), at least 1

struct Heavy { bool noExtras = false; float lifeK = 1; };
Heavy HeavyFor(const ShotFollowerConfig& c, int32_t shotsInView);

// the main ribbon's life (s): a set flight time wins; a set length lives as long as the shot takes to fly it, between minLengthLifeS
// and lengthLifeMul x life; else the life. flightS, lengthElmos <= 0: not set
float RibbonLife(const ShotFollowerConfig& c, float lifeS, float flightS, float lengthElmos, float speedElmosPerS);
// an extra ribbon's (dash, thread) life: its own flight, else its length over the speed (at least minLengthLifeS), else its life
float ExtraLife(const ShotFollowerConfig& c, float lifeS, float flightS, float lengthElmos, float speedElmosPerS);

// ---------------------------------------------------------------- cap plan

struct PlanShot {                      // one shot in view, for the cap
	int32_t id = 0;
	int32_t priority = 0;
	float d2 = 0;
	bool want = false;                 // eligible for a ribbon this step
	bool draw = false;                 // out: drawn this step
};
struct PlanResult { int32_t drawn = 0; int32_t capped = 0; };
// Over the cap: lower priority first, then nearer, then the lower id (the Lua's sort left equal keys in no set order; DESIGN: a stable
// look across frames). `order` is caller scratch with room for n ids (no allocation here). caps nullptr: nothing draws, all capped.
PlanResult Plan(PlanShot* shots, int32_t n, const CapsRow* caps, int32_t* order);

// ---------------------------------------------------------------- cut-offs (DESIGN measure: view depth, scaled by the detail option)

// the view depth of a point (elmos along the view direction from the camera), divided by the detail option's factor (RND-006 v4 R8)
float ViewDepth(float px, float py, float pz, float cx, float cy, float cz, float dx, float dy, float dz, float cutScale);
// a ribbon part (dash, thread): its own cut-off (elmos, < 0 none) when it has one; else a layered look draws it; else the band rule
// (tiers: the far band keeps the core only)
bool PartIn(float cutoffElmos, bool hasCutoff, float depth, Band band, bool tiers, bool layered);
// past the core's own cut-off: the core stops (a layered look keeps its other parts)
bool CoreOut(float cutoffElmos, bool hasCutoff, float depth);
// a puff stream: its own cut-off when it carries per-layer cut-offs (cutoff < 0 never), else the band rule (near and mid; a body
// stream of a tiered look to the far band too)
bool StreamIn(bool layerCutoff, float cutoffElmos, float depth, Band band, bool bodyStream, bool tiers);
// a further stream (extraPuffs): its own cut-off, else near and mid
bool ExtraStreamIn(bool layerCutoff, float cutoffElmos, float depth, Band band);

// ---------------------------------------------------------------- cadence

// a stream's period in steps: its base period, doubled in the mid band unless it has per-layer cut-offs (no thinning: those are
// made in full or not at all)
int32_t StreamEverySteps(int32_t baseEverySteps, Band band, bool layerCutoff);
// due this step: staggered by id so shots of one period do not bunch on one step
bool DueAt(int32_t step, int32_t id, int32_t everySteps);
// a stream's base period from its emission rate (per second): one puff every 1 / rate s, at least every step
int32_t EveryFromRate(float perSecond);

// ---------------------------------------------------------------- puff budget (a rolling window, rate-agnostic)

constexpr int32_t MAX_WINDOW_STEPS = 128;
class PuffBudget {
public:
	void Init(const ShotFollowerConfig& c, int32_t perSecond);   // after Rate is set
	void SetPerSecond(int32_t perSecond) { limit = perSecond; }
	void NewStep(int32_t step);        // forgets the steps that left the window
	bool Admit();                      // true while fewer than the limit started in the window; counts it
	int32_t Total() const { return total; }
private:
	std::array<int32_t, MAX_WINDOW_STEPS> slots{};
	int32_t window = 1, limit = 0, total = 0, step = -1;
};

// ---------------------------------------------------------------- path puffs (a trail laid at an emission rate along the path)

struct PathState { float carry = 0; int32_t lastStep = -1; };
struct PathPuffs {
	int32_t count = 0;                 // puffs to lay this step (at most pathPuffMax)
	int32_t slots = 0;                 // the puffs the rate gave; the spacing is over these, so a capped step keeps the spacing
	float stepsBack = 0;               // the span (steps of travel) they spread over, ending at the shot
	float lifeK = 1;                   // their life is over this (a fast shot lays them in space as at the pack's speed)
};
// rate per second; packSpeed (elmos per second, <= 0 none): a shot faster than it emits as many more per second, so the spacing in
// space stays the pack's. readEverySteps: how often the adapter reads this shot (a gap up to that is never clamped). Puff i (1..count) lies at stepsBack x (i - 1/2) / slots steps of travel behind the shot.
PathPuffs PathPuffsFor(const ShotFollowerConfig& c, PathState& s, int32_t step, float perSecond, float speedElmosPerStep,
	float packSpeedElmosPerS, int32_t readEverySteps);

// ---------------------------------------------------------------- the end point of a shot gone this step

// One step on along its velocity, cut where that stretch enters the ground (linear over the step), at least `lift` above it.
// groundAt(x, z) gives the ground height.
struct Vec3 { float x = 0, y = 0, z = 0; };
template <typename Ground>
Vec3 EndPoint(Vec3 p, Vec3 v, float lift, Ground groundAt) {
	Vec3 e{p.x + v.x, p.y + v.y, p.z + v.z};
	float gy = groundAt(e.x, e.z);
	if (e.y < gy && v.y < 0) {
		const float g0 = groundAt(p.x, p.z);
		const float above0 = p.y - g0, span = above0 - (e.y - gy);
		const float minSpan = 1.0f / 1000;     // as the Lua's 1e-3: no division by a flat stretch
		float t = above0 / (span > minSpan ? span : minSpan);
		t = t < 0 ? 0 : (t > 1 ? 1 : t);
		e = Vec3{p.x + v.x * t, p.y + v.y * t, p.z + v.z * t};
		gy = groundAt(e.x, e.z);
	}
	if (e.y < gy + lift)
		e.y = gy + lift;
	return e;
}

// ---------------------------------------------------------------- seeded jitter (energy families: arcs, release)

// Park-Miller minimal standard generator, as gfx_shots.lua Lcg, in exact integers (the Lua ran it in float, its number type, so its
// later values drifted; a look only, DESIGN): the same seed gives the same arc on every frame and client.
class Lcg {
public:
	explicit Lcg(int64_t seed);
	float Next();                      // in (0, 1)
private:
	int64_t q;
};

} // namespace KT1::Core::ShotFollower

#endif
