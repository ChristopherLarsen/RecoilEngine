/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: check mode's pairing of a native answer with the answer of the Lua it replaces. No engine include.
//
// Both sides give an answer under the same key: what was asked (a call-in or a value's name) and up to three ids. The two
// are compared as soon as both are there, in either order; the newest waiting answer of the other side is taken.
//   native only   the replaced Lua was never asked (another gadget refused first): "unmatched", not a difference
//   Lua only      native has no answer for that key: a difference
//   two inputs    both sides say what they were asked about and it is not the same (an earlier gadget changed it): unmatched
// A native answer waits until its call's scope closes (an Allow* call) or the step ends (a value); a Lua answer always
// waits until the step ends.

#ifndef KT1_CHECK_PAIRS_H
#define KT1_CHECK_PAIRS_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "CheckTally.h"

namespace KT1::Core::SimHost {

struct CheckKey {
	std::string system;
	std::string what;
	std::array<int32_t, 3> id{};

	bool operator<(const CheckKey& o) const { return std::tie(system, what, id) < std::tie(o.system, o.what, o.id); }
};

struct CheckValue {
	static constexpr int BOTH = 2;   // DESIGN: the answers one value holds

	std::array<float, BOTH> v{};   // a boolean is 0 or 1; compared bit for bit
	int count = 1;              // how many of v mean something; the shorter side decides how many are compared
	bool hasInput = false;
	float input = 0.0f;         // what this side was asked about, where an earlier gadget can change it
};

// The functions below take describe(key, native, lua), called for a difference that should be logged; `native` is null
// for a Lua answer with no native one.
class CheckPairs {
public:
	explicit CheckPairs(CheckTally& tally) : tally(tally) {}

	template<class F> void Native(const CheckKey& key, const CheckValue& value, F&& describe) { Give(key, value, true, describe); }
	template<class F> void Lua(const CheckKey& key, const CheckValue& value, F&& describe) { Give(key, value, false, describe); }

	size_t Mark() const { return scoped.size(); }

	// The native answers given since Mark() that Lua did not match are counted as unmatched.
	void CloseScope(size_t mark) {
		while (scoped.size() > mark) {
			const auto it = open.find(scoped.back().first);
			if (it != open.end()) {
				auto& waiting = it->second;
				const uint64_t wanted = scoped.back().second;
				const auto at = std::find_if(waiting.begin(), waiting.end(), [wanted](const Waiting& w) { return w.serial == wanted; });
				if (at != waiting.end()) {
					tally.Unmatched(it->first.system, 1);
					waiting.erase(at);
					if (waiting.empty()) open.erase(it);
				}
			}
			scoped.pop_back();
		}
	}

	// In key order, so the lines do not depend on who answered first.
	template<class F> void CloseStep(F&& describe) {
		for (const auto& [key, waiting] : open) {
			for (const Waiting& w : waiting) {
				if (w.native) tally.Unmatched(key.system, 1);
				else if (tally.Add(key.system, 1, 1)) describe(key, static_cast<const CheckValue*>(nullptr), w.value);
			}
		}
		open.clear();
		scoped.clear();
	}

	bool Empty() const { return open.empty(); }
	void Clear() {
		open.clear();
		scoped.clear();
		serial = 0;
	}

private:
	struct Waiting {
		CheckValue value;
		bool native;
		uint64_t serial;
	};

	template<class F> void Give(const CheckKey& key, const CheckValue& value, bool native, F&& describe) {
		const auto it = open.find(key);
		if (it != open.end()) {
			auto& waiting = it->second;
			for (size_t i = waiting.size(); i-- > 0;) {
				if (waiting[i].native == native) continue;
				const CheckValue other = waiting[i].value;
				waiting.erase(waiting.begin() + static_cast<std::ptrdiff_t>(i));
				if (waiting.empty()) open.erase(it);
				if (native) Compare(key, value, other, describe);
				else Compare(key, other, value, describe);
				return;
			}
		}
		open[key].push_back(Waiting{value, native, ++serial});
		if (native) scoped.emplace_back(key, serial);
	}

	template<class F> void Compare(const CheckKey& key, const CheckValue& native, const CheckValue& lua, F&& describe) {
		if (native.hasInput && lua.hasInput && !CheckTally::SameBits(native.input, lua.input)) {
			tally.Unmatched(key.system, 1);
			return;
		}
		bool same = true;
		for (int i = 0; i < std::min(native.count, lua.count); ++i)
			same = same && CheckTally::SameBits(native.v[static_cast<size_t>(i)], lua.v[static_cast<size_t>(i)]);
		if (tally.Add(key.system, 1, same ? 0 : 1)) describe(key, &native, lua);
	}

	CheckTally& tally;
	std::map<CheckKey, std::vector<Waiting>> open;
	std::vector<std::pair<CheckKey, uint64_t>> scoped;
	uint64_t serial = 0;
};

} // namespace KT1::Core::SimHost

#endif
