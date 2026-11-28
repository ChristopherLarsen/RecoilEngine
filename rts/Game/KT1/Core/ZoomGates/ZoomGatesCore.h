/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 zoom gates (LUA-FREE-WORKPACKAGES D07): the logic of "what is drawn at which zoom". No engine include: tested
// engine-free (Test/ZoomGates/ZoomGatesTest.cpp). Presentation only: synced code never reads any of it.
//
// One table of rows (game-build game/data/kt1/zoomgates.json): what, unit, from, to, cap, the level-of-detail option's
// factor per option value, hysteresis. A row is a gate of one of four kinds:
//   Level  drawn at zoom levels from..to (whole numbers 0..7)
//   Elmos  drawn while the camera distance is in [from, to) elmos; to < 0 = no end
//   Depth  as Elmos, but against one object's own view depth (effects): asked per object with DrawnAtDepth
//   Scale  no gate of its own: only its option factor is read (prop distances)
// The option factor multiplies from and to; cap (> 0) is an upper limit on the scaled end (the owner's model cap).
// Evaluate gives one bit per row; the adapter applies a row only when its bit changed.

#ifndef KT1_ZOOM_GATES_CORE_H
#define KT1_ZOOM_GATES_CORE_H

#include <cstdint>

namespace KT1::Core::ZoomGates {

inline constexpr int kBounds = 7;          // split points between the 8 zoom levels L0..L7
inline constexpr int kMaxLevel = kBounds;
inline constexpr int kDetails = 3;         // level-of-detail option values: 0 lowest, 1 middle, 2 highest
inline constexpr int kMaxGates = 64;       // one bit each in the mask
inline constexpr int kNoLevel = -1;        // "no previous level"
inline constexpr float kNoEnd = -1;        // a gate's `to` or `cap` that is not set

struct Levels {
	float bounds[kBounds] = {};            // elmos, rising
	int stepIndex = kNoLevel;              // the split point (0-based) with no hysteresis: the icon switch
	float hyst = 0;                        // band around every other split point, as a fraction of it
};

// the level of distance d with no memory
// KT1 hot
inline int RawLevel(const Levels& lv, float d) {
	for (int i = 0; i < kBounds; ++i)
		if (d < lv.bounds[i]) return i;
	return kMaxLevel;
}

// the level at distance d; prev = the last level (kNoLevel at first), wholeMap = the whole map is in view.
// A move of one level stays at prev while d is inside the band of the split point crossed, except at stepIndex.
// KT1 hot
inline int Level(const Levels& lv, float d, int prev, bool wholeMap) {
	if (wholeMap) return kMaxLevel;
	const int raw = RawLevel(lv, d);
	if (prev == kNoLevel || raw == prev) return raw;
	const int crossed = (raw > prev) ? raw - 1 : raw;      // 0-based split point between prev and raw, nearest prev
	if ((raw - prev == 1 || prev - raw == 1) && crossed != lv.stepIndex) {
		const float b = lv.bounds[crossed];
		if (raw > prev && d < b * (1 + lv.hyst)) return prev;
		if (raw < prev && d >= b * (1 - lv.hyst)) return prev;
	}
	return raw;
}

enum class Unit : uint8_t { Level, Elmos, Depth, Scale };

struct Gate {
	Unit unit = Unit::Level;
	float from = 0;
	float to = kNoEnd;
	float cap = kNoEnd;
	float detail[kDetails] = {1, 1, 1};
	float hyst = 0;                        // Elmos and Depth: band around each end, as a fraction of it
};

inline int ClampDetail(int detail) { return detail < 0 ? 0 : (detail >= kDetails ? kDetails - 1 : detail); }

// the option factor of a row
inline float Factor(const Gate& g, int detail) { return g.detail[ClampDetail(detail)]; }

// the scaled start of a distance row
inline float Start(const Gate& g, int detail) { return g.from * Factor(g, detail); }

// the scaled end of a distance row with its cap; kNoEnd when it has none
inline float End(const Gate& g, int detail) {
	if (g.to < 0) return kNoEnd;
	const float e = g.to * Factor(g, detail);
	return (g.cap > 0 && g.cap < e) ? g.cap : e;
}

// a distance row at distance d; `was` = its last answer (for the band)
// KT1 hot
inline bool DrawnAtDistance(const Gate& g, float d, int detail, bool was) {
	const float h = was ? g.hyst : -g.hyst;                // drawn: leave late; not drawn: come back late
	const float s = Start(g, detail);
	if (s > 0 && d < s * (1 - h)) return false;
	const float e = End(g, detail);
	if (e >= 0 && d >= e * (1 + h)) return false;
	return true;
}

// one object's own view depth against a Depth row (no memory: an effect is made or not made once)
// KT1 hot
inline bool DrawnAtDepth(const Gate& g, float depth, int detail) {
	Gate exact = g;
	exact.hyst = 0;
	return DrawnAtDistance(exact, depth, detail, true);
}

// a Level row at a zoom level
inline bool DrawnAtLevel(const Gate& g, int level) {
	return level >= static_cast<int>(g.from) && (g.to < 0 || level <= static_cast<int>(g.to));
}

struct Table {
	Gate gates[kMaxGates];
	int count = 0;
};

// one bit per row: Level and Elmos rows by the camera; Depth and Scale rows always set (they are not camera gates).
// prev = the last mask; first = there is none yet.
// KT1 hot
inline uint64_t Evaluate(const Table& t, int level, float dist, int detail, uint64_t prev, bool first) {
	uint64_t mask = 0;
	for (int i = 0; i < t.count && i < kMaxGates; ++i) {
		const Gate& g = t.gates[i];
		const uint64_t bit = uint64_t(1) << i;
		bool on = true;
		if (g.unit == Unit::Level) on = DrawnAtLevel(g, level);
		else if (g.unit == Unit::Elmos) {
			// with no memory, take the plain answer (band 0): ask both ways and they agree outside the band;
			// inside it the nearer state (drawn for an end, not drawn for a start) is not knowable, so use the exact ends
			if (first) { Gate e = g; e.hyst = 0; on = DrawnAtDistance(e, dist, detail, true); }
			else on = DrawnAtDistance(g, dist, detail, (prev & bit) != 0);
		}
		if (on) mask |= bit;
	}
	return mask;
}

// the option factor of a kind from its three sweep brackets {last seen, first gone} (game-build LuaUI/lib/detail_option.lua):
// the ratio of bracket midpoints to the middle option's. first gone < 0 = not reached; last seen 0 = already gone at the
// first sampled zoom (the upper end is all that is known). A middle bracket that is open gives 1 at the highest option.
inline float BracketMid(const float b[2]) {
	if (b[0] == 0) return b[1];
	return b[1] < 0 ? b[0] : (b[0] + b[1]) / 2;
}
inline float SweepFactor(const float brackets[kDetails][2], int detail) {
	detail = ClampDetail(detail);
	if (detail == 1) return 1;
	if (brackets[1][1] < 0 && detail == 2) return 1;
	return BracketMid(brackets[detail]) / BracketMid(brackets[1]);
}

} // namespace KT1::Core::ZoomGates

#endif
