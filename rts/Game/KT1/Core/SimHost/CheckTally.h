/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: the counts of check mode (LUA-FREE §3.2: both paths run and are compared). No engine include.
//
// Per system: how many values were compared and how many differed, this step and in the whole game. The lines are the
// ones tools/engine/lf_proof.py reads:
//   [KT1CHECK] <System> f=<step> n=<compared> diffs=<k>      a step with differences
//   [KT1CHECK] <System> total n=<sum> diffs=<sum> unmatched=<u> writes_skipped=<w> cumulative
//                                  the game so far: the last one is the result. unmatched: answers with nothing to compare
//                                  with (CheckPairs.h); a system whose count is not 0 explains it. writes_skipped: writes
//                                  its native code tried in check mode (WriteGate.h)
//   [KT1CHECK] family=<family> f=<step> n=<sum> diffs=<sum>   and   family=<family> total ...: the same two lines summed
//                                  over a family's systems (LUA-FREE H01)
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
	void Skipped(const std::string& system, int64_t writes) { Find(system).skipped += writes; }

	// fn(line) for every system that differed this step, then for every family one of whose systems did, with the sums
	// over the family's systems; then the step's counts start over. familyOf(system) -> its family, empty for none.
	template<class F, class G> void CloseStep(int step, F&& fn, G&& familyOf) {
		std::vector<Row> families;
		for (const Row& r : rows) AddTo(families, familyOf(r.system), r);
		for (const Row& r : rows)
			if (r.stepDiffs > 0)
				fn("[KT1CHECK] " + r.system + " f=" + std::to_string(step) + " n=" + std::to_string(r.stepN) + " diffs=" + std::to_string(r.stepDiffs));
		for (const Row& f : families)
			if (f.stepDiffs > 0)
				fn("[KT1CHECK] family=" + f.system + " f=" + std::to_string(step) + " n=" + std::to_string(f.stepN) + " diffs=" + std::to_string(f.stepDiffs));
		for (Row& r : rows) {
			r.stepN = 0;
			r.stepDiffs = 0;
		}
	}
	template<class F> void CloseStep(int step, F&& fn) { CloseStep(step, fn, NoFamily); }

	template<class F, class G> void Totals(F&& fn, G&& familyOf) const {
		std::vector<Row> families;
		for (const Row& r : rows) {
			AddTo(families, familyOf(r.system), r);
			fn(TotalLine(r.system, r));
		}
		for (const Row& f : families) fn(TotalLine("family=" + f.system, f));
	}
	template<class F> void Totals(F&& fn) const { Totals(fn, NoFamily); }

	bool Empty() const { return rows.empty(); }
	void Clear() { rows.clear(); }

private:
	struct Row {
		std::string system;
		int64_t n = 0, diffs = 0, stepN = 0, stepDiffs = 0, unmatched = 0, skipped = 0;
		int described = 0;
	};

	static std::string NoFamily(const std::string&) { return {}; }

	static std::string TotalLine(const std::string& who, const Row& r) {
		return "[KT1CHECK] " + who + " total n=" + std::to_string(r.n) + " diffs=" + std::to_string(r.diffs) +
			" unmatched=" + std::to_string(r.unmatched) + " writes_skipped=" + std::to_string(r.skipped) + " cumulative";
	}

	// By name, so the lines come in one order whatever reported first.
	static Row& FindIn(std::vector<Row>& in, const std::string& name) {
		size_t i = 0;
		while (i < in.size() && in[i].system < name) ++i;
		if (i == in.size() || in[i].system != name) in.insert(in.begin() + static_cast<std::ptrdiff_t>(i), Row{name});
		return in[i];
	}
	Row& Find(const std::string& system) { return FindIn(rows, system); }

	static void AddTo(std::vector<Row>& families, const std::string& family, const Row& r) {
		if (family.empty()) return;
		Row& f = FindIn(families, family);
		f.n += r.n;
		f.diffs += r.diffs;
		f.stepN += r.stepN;
		f.stepDiffs += r.stepDiffs;
		f.unmatched += r.unmatched;
		f.skipped += r.skipped;
	}

	std::vector<Row> rows;
};

} // namespace KT1::Core::SimHost

#endif
