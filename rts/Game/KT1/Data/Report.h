/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Data: what reading one config file found. Engine-free.

#ifndef KT1_DATA_REPORT_H
#define KT1_DATA_REPORT_H

#include <string>
#include <utility>
#include <vector>

namespace KT1::Data {

// Dev: any problem rejects the whole file, so a typo cannot pass unseen.
// Release: unknown keys are warnings, a bad value keeps its default, the rest of the file applies.
enum class Strictness { Dev, Release };

struct Issue {
	bool error;          // false: a warning
	std::string where;   // "heavy.extras", "caps[2]"; "" for the file as a whole
	std::string text;
};

struct Report {
	Strictness strictness = Strictness::Dev;
	std::vector<Issue> issues;
	bool accepted = false;   // the values were applied to the caller's struct

	void Error(std::string where, std::string text) { issues.push_back({true, std::move(where), std::move(text)}); }
	// A key the struct does not have: an error in dev, a warning in release.
	void Unknown(std::string where, std::string text) { issues.push_back({strictness == Strictness::Dev, std::move(where), std::move(text)}); }

	bool HasErrors() const {
		for (const Issue& i : issues)
			if (i.error) return true;
		return false;
	}
};

} // namespace KT1::Data

#endif
