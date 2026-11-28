/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core: Intel facts, the functions (IntelFactsCore.h says what and why). Standard library only.

#pragma GCC optimize("fp-contract=off")

#include "IntelFactsCore.h"

#include <algorithm>

namespace KT1::IntelFactsCore {

namespace {
	// Lua's floor(x * FPS + 0.5) on float numbers: seconds to steps, half up
	int32_t ToSteps(float seconds, float sps) { return static_cast<int32_t>(std::floor(seconds * sps + 0.5f)); }

	// VI.At: the newest record at or before step t (1-based; the oldest when none is)
	int At(const Shots& s, float t) {
		int i = static_cast<int>(s.hf.size());
		while (i > 1 && s.hf[i - 1] > t) --i;
		return i;
	}
}

Steps Steps::From(const Seconds& s, int32_t stepsPerSecond, bool staggered, int32_t sg) {
	const float sps = static_cast<float>(stepsPerSecond);
	Steps out;
	out.FG = 1;
	if (staggered) {
		out.FG = ToSteps(s.factsGridTicks * s.refTick, sps);
		if (out.FG < 1) out.FG = 1;
		else if (sg % out.FG != 0) out.FG = sg;
	}
	out.ri = ToSteps(s.redrawInterval, sps);
	out.lagE = ToSteps(s.moveLagEnter, sps);
	out.lagL = ToSteps(s.moveLagLeave, sps);
	if (staggered && out.FG > 1) {
		// DESIGN: the lag in whole facts grids, m of them, for the same mean delay as the lag without stale facts:
		// m = round((lag - (FG - 1) / 2) / FG), at least 0 (int_coverage Grids)
		const float fg = static_cast<float>(out.FG);
		auto grids = [&](float sec) -> int32_t {
			const float l = sec * sps;
			if (l <= 0.0f) return 0;
			return out.FG * std::max(0, static_cast<int32_t>(std::floor((l - (fg - 1.0f) / 2.0f) / fg + 0.5f)));
		};
		out.lagE = grids(s.moveLagEnter);
		out.lagL = grids(s.moveLagLeave);
	}
	if (out.lagL < out.lagE) out.lagL = out.lagE;   // leaving is never quicker than entering
	out.reF = staggered ? 1 : ToSteps(s.redrawPeriod, sps);
	return out;
}

void Cells(const Geometry& g, float x, float z, Row& o) {
	o.px = x; o.pz = z;
	const float a1 = Floor(x / g.fe), b1 = Floor(z / g.fe), a2 = Floor(x / g.ce), b2 = Floor(z / g.ce);   // CMD-010 v6 R7
	o.fcx = a1; o.fcz = b1; o.ccx = a2; o.ccz = b2;
	o.vkey = Floor(x / g.VS) * 8192.0f + Floor(z / g.VS);
	o.skey = Floor(x / g.SS) * 8192.0f + Floor(z / g.SS);
	o.fk = (a1 >= 0 && b1 >= 0 && a1 < g.fnx && b1 < g.fnz) ? b1 * g.fnx + a1 : -1.0f;
	o.ck = (a2 >= 0 && b2 >= 0 && a2 < g.cnx && b2 < g.cnz) ? b2 * g.cnx + a2 : -1.0f;
	o.fb = Floor(a1 / g.FB) * 65536.0f + Floor(b1 / g.FB);
	o.cb = Floor(a2 / g.CB) * 65536.0f + Floor(b2 / g.CB);
}

void Stamp(const Rules& r, Stamped& s, float x, float z, float rE, float f, float c, float& ox, float& oz) {
	const float fr = r.redrawFraction;
	if (!(fr > 0.0f)) { ox = x; oz = z; return; }
	if (s.has) {
		const float dx = x - s.x, dz = z - s.z, lim = fr * rE;
		const bool crossed = r.restampOnCell && (Floor(x / c) != Floor(s.x / c) || Floor(z / c) != Floor(s.z / c));
		// INT-005 v7 R1: kept while it has moved less than the fraction of its radius, for less than the interval
		if (!crossed && dx * dx + dz * dz < lim * lim && f - s.f < static_cast<float>(r.steps.ri) && s.r == rE) {
			s.fresh = (dx == 0.0f && dz == 0.0f) ? 1 : 0;
			ox = s.x; oz = s.z;
			return;
		}
	}
	s.has = true; s.x = x; s.z = z; s.f = f; s.r = rE; s.fresh = 1;
	s.stamps += 1;
	ox = x; oz = z;
}

void Shot(const Steps& st, Shots& s, float x, float z, float c, float f, float& ox, float& oz) {
	const float reF = static_cast<float>(st.reF);
	const float g = Floor(f / reF);
	if (s.hf.empty() || !s.hasC || s.c != c) {
		s.hf.assign(1, f); s.hx.assign(1, x); s.hz.assign(1, z);
		s.hasC = true; s.c = c; s.g = g;
		s.scx = Snap(x, c); s.scz = Snap(z, c);
		s.lcx = s.scx; s.lcz = s.scz;
		ox = s.scx; oz = s.scz;
		return;
	}
	int n = static_cast<int>(s.hf.size());
	if (x != s.hx[n - 1] || z != s.hz[n - 1]) {
		s.hf.push_back(f); s.hx.push_back(x); s.hz.push_back(z);
		n += 1;
	}
	if (g != s.g) {
		s.g = g;
		const float t = g * reF;
		const int ie = At(s, t - static_cast<float>(st.lagE)), il = At(s, t - static_cast<float>(st.lagL));
		s.scx = Snap(s.hx[ie - 1], c); s.scz = Snap(s.hz[ie - 1], c);
		if (il == ie) { s.lcx = s.scx; s.lcz = s.scz; }
		else { s.lcx = Snap(s.hx[il - 1], c); s.lcz = Snap(s.hz[il - 1], c); }
		const int drop = std::min(il, ie) - 1;     // records older than both are never looked up again
		if (drop > 0) {
			s.hf.erase(s.hf.begin(), s.hf.begin() + drop);
			s.hx.erase(s.hx.begin(), s.hx.begin() + drop);
			s.hz.erase(s.hz.begin(), s.hz.begin() + drop);
		}
	}
	ox = s.scx; oz = s.scz;
}

} // namespace KT1::IntelFactsCore
