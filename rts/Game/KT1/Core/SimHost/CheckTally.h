/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the counts of check mode (LUA-FREE §3.2: both paths run and are compared). No engine include.
//
// Per system: how many values were compared and how many differed, this step and in the whole game. The lines are the
// ones tools/engine/lf_proof.py reads:
//   [KT1CHECK] <System> f=<step> n=<compared> diffs=<k>      a step with differences
//   [KT1CHECK] <System> total n=<sum> diffs=<sum> unmatched=<u> cumulative
//                                  the game so far: the last one is the result. unmatched: answers with nothing to compare
//                                  with (CheckPairs.h); a system whose count is not 0 explains it
// Values are compared bit for bit as float, the engine's Lua number.

#ifndef KT1_CHECK_TALLY_H
#define KT1_CHECK_TALLY_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace KT1::Core::SimHost {

constexpr int CHECK_DETAILS = 20;   // differences described per system; the rest are only counted

class CheckTally {
public:
	static bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

	// Returns true when the caller should describe these differences.
	bool Add(const std::string& system, int64_t compared, int64_t differed) {
		Row& r = Find(system);
		r.stepN += compared;
		r.stepDiffs += differed;
		r.n += compared;
		r.diffs += differed;
		if (differed <= 0 || r.described >= CHECK_DETAILS) return false;
		++r.described;
		return true;
	}

	void Unmatched(const std::string& system, int64_t count) { Find(system).unmatched += count; }

	// fn(line) for every system that differed this step; then the step's counts start over.
	template<class F> void CloseStep(int step, F&& fn) {
		for (Row& r : rows) {
			if (r.stepDiffs > 0)
				fn("[KT1CHECK] " + r.system + " f=" + std::to_string(step) + " n=" + std::to_string(r.stepN) + " diffs=" + std::to_string(r.stepDiffs));
			r.stepN = 0;
			r.stepDiffs = 0;
		}
	}

	template<class F> void Totals(F&& fn) const {
		for (const Row& r : rows)
			fn("[KT1CHECK] " + r.system + " total n=" + std::to_string(r.n) + " diffs=" + std::to_string(r.diffs) +
				" unmatched=" + std::to_string(r.unmatched) + " cumulative");
	}

	bool Empty() const { return rows.empty(); }
	void Clear() { rows.clear(); }

private:
	struct Row {
		std::string system;
		int64_t n = 0, diffs = 0, stepN = 0, stepDiffs = 0, unmatched = 0;
		int described = 0;
	};

	// By name, so the lines come in one order whatever reported first.
	Row& Find(const std::string& system) {
		size_t i = 0;
		while (i < rows.size() && rows[i].system < system) ++i;
		if (i == rows.size() || rows[i].system != system) rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(i), Row{system});
		return rows[i];
	}

	std::vector<Row> rows;
};

} // namespace KT1::Core::SimHost

#endif
