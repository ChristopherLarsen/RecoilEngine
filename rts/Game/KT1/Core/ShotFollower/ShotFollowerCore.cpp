/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/ShotFollower: the shot follower's decisions (see ShotFollowerCore.h for what each rule is and where it comes from).

#include "ShotFollowerCore.h"

#include "Game/KT1/Core/Common/Rate.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace KT1::Core::ShotFollower {

namespace {
constexpr int64_t LCG_MOD = 2147483647;      // DESIGN: 2^31 - 1, the Park-Miller modulus (gfx_shots.lua Lcg)
constexpr int64_t LCG_MUL = 16807;           // DESIGN: the minimal standard multiplier (gfx_shots.lua Lcg)
}

// ---------------------------------------------------------------- setting, caps, mode

Setting ParseSetting(bool has, float value)
{
	if (!has)
		return DEFAULT_SETTING;
	if (value <= 0)
		return Setting::None;
	if (value < static_cast<float>(Setting::Full))   // DESIGN (owner Q7): 1 short, 2 and up full
		return Setting::Short;
	return Setting::Full;
}

const CapsRow* CapsFor(const ShotFollowerConfig& c, float scale)
{
	if (scale <= 0 || c.capsCount <= 0)
		return nullptr;
	const CapsRow* pick = &c.caps[0];
	for (int32_t i = 0; i < c.capsCount; ++i) {
		if (scale >= c.caps[i].scale)
			pick = &c.caps[i];
	}
	return pick;
}

Mode MakeMode(const ShotFollowerConfig& c, Setting setting, bool limiting, const CapsRow* caps)
{
	Mode m;
	if (caps == nullptr)
		return m;                              // effects off: nothing
	m.pulse = caps->scale >= 1;                // DESIGN: the plasma pulse from the middle row up
	m.flak = caps->scale >= 2;                 // DESIGN: flak's ribbon from the top row up
	if (setting == Setting::None)
		return m;
	m.ribbons = m.keep = m.puffs = true;
	if (setting == Setting::Short) {
		m.missilesOnly = true;
		m.lifeMaxS = c.shortLifeS;
	}
	if (limiting) {                            // the guard stops new ribbons and puffs, not the ribbons already drawing
		m.ribbons = m.puffs = false;
		m.lifeMul = c.guardLifeShare;
	}
	return m;
}

// ---------------------------------------------------------------- per shot

Band BandOf(const ShotFollowerConfig& c, float d2)
{
	if (d2 < c.nearElmos * c.nearElmos)
		return BAND_NEAR;
	if (d2 < c.midElmos * c.midElmos)
		return BAND_MID;
	if (d2 < c.farElmos * c.farElmos)
		return BAND_FAR;
	return BAND_BEYOND;
}

float BodyFade(const ShotFollowerConfig& c, float d2)
{
	const float d = std::sqrt(d2);
	const float start = c.farElmos * (1 - c.fade);
	if (d <= start)
		return 1;
	if (d >= c.farElmos)
		return 0;
	return 1 - (d - start) / (c.farElmos - start);
}

float DepthAlpha(const ShotFollowerConfig& c, float y)
{
	if (y >= 0)
		return 1;
	return std::max(c.depthAlphaMin, 1 + y / c.underFadeElmos);
}

bool Visible(bool enemy, bool inLos, bool fullView) { return fullView || !enemy || inLos; }

bool Collapsed(bool overview, bool ownerIcon) { return overview || ownerIcon; }

bool Eligible(const RibbonTraits& t, Band band, const Mode& mode, bool started)
{
	if (band >= BAND_FAR && !t.layered)
		return false;
	if (t.essential && (mode.ribbons || mode.keep))
		return true;
	const bool highOnly = t.highOnly < 0 ? t.flak : t.highOnly != 0;
	if (highOnly && !mode.flak)
		return false;
	if (mode.missilesOnly && !t.missile)
		return false;
	if (mode.ribbons)
		return true;
	return mode.keep && started;
}

int32_t PointsFor(const CapsRow& caps, Band band)
{
	if (band == BAND_MID)
		return std::max<int32_t>(1, caps.points / 2);   // DESIGN (shots.lua S.Points): mid, half the points
	return caps.points;
}

float LifeUnderMode(float lifeS, const Mode& mode)
{
	float l = lifeS * mode.lifeMul;
	if (mode.lifeMaxS >= 0 && l > mode.lifeMaxS)
		l = mode.lifeMaxS;
	return l;
}

int32_t StrideSteps(float lifeS, int32_t points)
{
	const float lifeSteps = lifeS * Rate::StepsPerSecond();
	const float stride = std::ceil(lifeSteps / std::max<int32_t>(1, points));
	return std::max<int32_t>(1, static_cast<int32_t>(stride));
}

Heavy HeavyFor(const ShotFollowerConfig& c, int32_t shotsInView)
{
	Heavy h;
	h.noExtras = shotsInView > c.heavyExtras;
	h.lifeK = shotsInView > c.heavyLife ? c.heavyLifeK : 1;
	return h;
}

float RibbonLife(const ShotFollowerConfig& c, float lifeS, float flightS, float lengthElmos, float speedElmosPerS)
{
	if (flightS > 0)
		return flightS;
	if (lengthElmos > 0 && speedElmosPerS > c.minSpeedElmosPerS)
		return std::max(c.minLengthLifeS, std::min(lifeS * c.lengthLifeMul, lengthElmos / speedElmosPerS));
	return lifeS;
}

float ExtraLife(const ShotFollowerConfig& c, float lifeS, float flightS, float lengthElmos, float speedElmosPerS)
{
	if (flightS > 0)
		return flightS;
	if (lengthElmos <= 0)
		return lifeS;
	if (speedElmosPerS > c.minSpeedElmosPerS)
		return std::max(c.minLengthLifeS, lengthElmos / speedElmosPerS);
	return lifeS;
}

// ---------------------------------------------------------------- cap plan

PlanResult Plan(PlanShot* shots, int32_t n, const CapsRow* caps, int32_t* order)
{
	PlanResult r;
	int32_t want = 0;
	for (int32_t i = 0; i < n; ++i) {
		shots[i].draw = false;
		if (shots[i].want)
			order[want++] = i;
	}
	if (caps == nullptr) {
		r.capped = want;
		return r;
	}
	if (want > caps->ribbons) {
		std::sort(order, order + want, [shots](int32_t a, int32_t b) {
			const PlanShot& x = shots[a];
			const PlanShot& y = shots[b];
			if (x.priority != y.priority)
				return x.priority < y.priority;
			if (x.d2 != y.d2)
				return x.d2 < y.d2;
			return x.id < y.id;
		});
	}
	r.drawn = std::min(want, caps->ribbons);
	r.capped = want - r.drawn;
	for (int32_t i = 0; i < r.drawn; ++i)
		shots[order[i]].draw = true;
	return r;
}

// ---------------------------------------------------------------- cut-offs

float ViewDepth(float px, float py, float pz, float cx, float cy, float cz, float dx, float dy, float dz, float cutScale)
{
	const float depth = (px - cx) * dx + (py - cy) * dy + (pz - cz) * dz;
	return cutScale > 0 ? depth / cutScale : depth;
}

bool PartIn(float cutoffElmos, bool hasCutoff, float depth, Band band, bool tiers, bool layered)
{
	if (hasCutoff)
		return cutoffElmos < 0 || depth <= cutoffElmos;
	if (layered)
		return true;
	return !(tiers && band >= BAND_FAR);
}

bool CoreOut(float cutoffElmos, bool hasCutoff, float depth) { return hasCutoff && cutoffElmos >= 0 && depth > cutoffElmos; }

bool StreamIn(bool layerCutoff, float cutoffElmos, float depth, Band band, bool bodyStream, bool tiers)
{
	if (layerCutoff)
		return cutoffElmos < 0 || depth <= cutoffElmos;
	return band <= BAND_MID || (bodyStream && tiers && band == BAND_FAR);
}

bool ExtraStreamIn(bool layerCutoff, float cutoffElmos, float depth, Band band)
{
	if (layerCutoff)
		return cutoffElmos < 0 || depth <= cutoffElmos;
	return band <= BAND_MID;
}

// ---------------------------------------------------------------- cadence

int32_t StreamEverySteps(int32_t baseEverySteps, Band band, bool layerCutoff)
{
	const int32_t every = std::max<int32_t>(1, baseEverySteps);
	return (band == BAND_MID && !layerCutoff) ? every * 2 : every;   // DESIGN (gfx_shots.lua): mid, every second puff
}

bool DueAt(int32_t step, int32_t id, int32_t everySteps)
{
	const int32_t every = std::max<int32_t>(1, everySteps);
	const int64_t k = static_cast<int64_t>(step) + id;
	return ((k % every) + every) % every == 0;
}

int32_t EveryFromRate(float perSecond)
{
	if (perSecond <= 0)
		return 1;
	return Rate::Every(static_cast<float>(1) / perSecond);
}

// ---------------------------------------------------------------- puff budget

void PuffBudget::Init(const ShotFollowerConfig& c, int32_t perSecond)
{
	window = std::clamp<int32_t>(Rate::Every(c.puffWindowS), 1, MAX_WINDOW_STEPS);
	assert(Rate::Every(c.puffWindowS) <= MAX_WINDOW_STEPS);
	limit = perSecond;
	total = 0;
	step = -1;
	slots.fill(0);
}

void PuffBudget::NewStep(int32_t s)
{
	if (s == step)
		return;
	const int32_t from = std::max(step + 1, s - window + 1);
	for (int32_t g = from; g <= s; ++g) {
		int32_t& slot = slots[static_cast<size_t>(g % window)];
		total -= slot;
		slot = 0;
	}
	step = s;
}

bool PuffBudget::Admit()
{
	if (total >= limit || step < 0)
		return false;
	++slots[static_cast<size_t>(step % window)];
	++total;
	return true;
}

// ---------------------------------------------------------------- path puffs

PathPuffs PathPuffsFor(const ShotFollowerConfig& c, PathState& s, int32_t step, float perSecond, float speedElmosPerStep,
	float packSpeedElmosPerS, int32_t readEverySteps)
{
	PathPuffs out;
	const int32_t maxGap = std::max(Rate::Every(c.pathPuffMaxGapS), readEverySteps);   // a long absence counts as maxGapS
	const int32_t gap = s.lastStep < 0 ? 1 : std::clamp<int32_t>(step - s.lastStep, 1, maxGap);
	s.lastStep = step;
	const float speed = speedElmosPerStep * Rate::StepsPerSecond();                  // elmos per second
	const float k = (packSpeedElmosPerS > 0 && speed > packSpeedElmosPerS) ? speed / packSpeedElmosPerS : 1;
	const float acc = s.carry + perSecond * k * gap * Rate::StepSeconds();
	const float whole = std::floor(acc);
	s.carry = acc - whole;
	out.slots = static_cast<int32_t>(whole);
	out.count = std::min<int32_t>(out.slots, c.pathPuffMax);
	out.stepsBack = static_cast<float>(gap);
	out.lifeK = 1 / k;
	return out;
}

// ---------------------------------------------------------------- seeded jitter

Lcg::Lcg(int64_t seed)
{
	q = ((seed % LCG_MOD) + LCG_MOD) % LCG_MOD;   // Lua's floor modulo: never negative
	if (q == 0)
		q = LCG_MOD - 1;
}

float Lcg::Next()
{
	q = (q * LCG_MUL) % LCG_MOD;
	return static_cast<float>(static_cast<double>(q) / LCG_MOD);   // KT1 lint: allow double the generator's 31-bit state does not fit a float exactly
}

} // namespace KT1::Core::ShotFollower
