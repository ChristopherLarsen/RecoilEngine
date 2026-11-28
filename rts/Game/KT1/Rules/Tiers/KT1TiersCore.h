/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 rule Tiers: the pure decision (no engine includes), shared by KT1Tiers.cpp and the engine-free tests (KT1/Test/Tiers/).
// PRF-002 v3 R7; plan: game-build docs/engine/RATE-TIERS-PLAN.md.

#ifndef KT1_TIERS_CORE_H
#define KT1_TIERS_CORE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace KT1 {
namespace TiersCore {
	// tier period in sim frames from seconds: 15 at 30 Hz, 5 at 10 Hz for 0.5 s; never under 2
	inline int Period(float periodSeconds, int gameSpeed) {
		return std::max(2, static_cast<int>(std::lround(periodSeconds * gameSpeed)));
	}

	// R7 tier visit: the frames where frame + id is a multiple of the period
	inline bool Visit(int frame, int id, int period) { return ((frame + id) % period) == 0; }

	// what the end-of-frame check reads (synced state only)
	struct Facts {
		bool idle = false;      // empty queue, not built/stunned/transported/carrying/targeting, no speed, not a builder
		bool changed = false;   // position, heading, health, progress or team differ from the previous frame end
		bool far = false;       // no enemy within the action distance
	};

	// the byte for frame nextFrame
	inline bool Decide(const Facts& f, int nextFrame, int id, int period) {
		return f.idle && !f.changed && f.far && !Visit(nextFrame, id, period);
	}

	// live state read at each hook during the frame (promotion at once)
	struct Live {
		bool queueEmpty = true, beingBuilt = false, transported = false, carrying = false, stunned = false;
		bool hasTarget = false, moving = false;
		int lastAttackFrame = -200, lastNanoAdd = -200;
	};

	inline bool Wake(const Live& l, int frame) {
		return !l.queueEmpty || l.beingBuilt || l.transported || l.carrying || l.stunned || l.hasTarget || l.moving
			|| l.lastAttackFrame >= frame - 1 || l.lastNanoAdd >= frame - 1;
	}

	// ally-team occupancy grid: one 64-bit ally-team mask per cell, rebuilt each frame; OR is order-free
	class Grid {
	public:
		void Init(float mapW, float mapH, float cell) {
			cellSize = cell; nx = std::max(1, (int)std::ceil(mapW / cell)); nz = std::max(1, (int)std::ceil(mapH / cell));
			cells.assign((size_t)nx * nz, 0);
		}
		void Clear() { std::fill(cells.begin(), cells.end(), 0ull); }
		void Add(float x, float z, int ally) {
			if (ally < 0 || ally >= 64) return;
			cells[Idx(CX(x), CZ(z))] |= (1ull << ally);
		}
		// true when any cell the circle's bounding square (x, z, r) touches holds a team in enemyMask (errs toward near)
		bool EnemyWithin(float x, float z, float r, uint64_t enemyMask) const {
			const int x0 = CX(x - r), x1 = CX(x + r), z0 = CZ(z - r), z1 = CZ(z + r);
			for (int j = z0; j <= z1; ++j)
				for (int i = x0; i <= x1; ++i)
					if (cells[Idx(i, j)] & enemyMask) return true;
			return false;
		}
	private:
		int CX(float x) const { return std::clamp((int)std::floor(x / cellSize), 0, nx - 1); }
		int CZ(float z) const { return std::clamp((int)std::floor(z / cellSize), 0, nz - 1); }
		size_t Idx(int i, int j) const { return (size_t)j * nx + i; }
		float cellSize = 512.0f; int nx = 1, nz = 1;
		std::vector<uint64_t> cells;
	};
}
}

#endif
