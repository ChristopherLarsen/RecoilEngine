/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: asking a chain of native validators. No engine include.
//
// A live validator decides: in a veto chain the first "no" ends it. A check-mode validator never decides: it is asked,
// its answer is kept in the call's Shadow, and the host compares it with what the Lua chain answers.

#ifndef KT1_VALIDATORS_H
#define KT1_VALIDATORS_H

#include <array>
#include <string>

#include "Ordered.h"

namespace KT1::Core::SimHost {

template<class Fn> struct Validator {
	std::string system;
	bool check;     // true: its system's switch is "check"
	Fn fn;
};

template<class Fn> using Chain = Ordered<Validator<Fn>>;

constexpr int SHADOW_CAPACITY = 8;   // DESIGN: check-mode validators compared per call; more are not asked to compare

// The check-mode answers to one call. Lives on the caller's stack, so a call nested inside another keeps its own.
template<class A> class Shadow {
public:
	// KT1 hot
	void Keep(const std::string& system, const A& answer) {
		if (count < SHADOW_CAPACITY) items[count++] = Item{&system, answer};
	}
	template<class F> void Each(F&& fn) const {
		for (int i = 0; i < count; ++i) fn(*items[i].system, items[i].answer);
	}
	int Size() const { return count; }

private:
	struct Item {
		const std::string* system = nullptr;
		A answer{};
	};
	std::array<Item, SHADOW_CAPACITY> items;
	int count = 0;
};

// A veto chain in order. False: a live validator refused, and the chain and Lua are not asked further.
// KT1 hot
template<class Fn, class... Args> bool AskVeto(const Chain<Fn>& chain, Shadow<bool>& shadow, Args&&... args) {
	for (const auto& item : chain) {
		const bool yes = item.value.fn(args...);
		if (item.value.check) shadow.Keep(item.value.system, yes);
		else if (!yes) return false;
	}
	return true;
}

} // namespace KT1::Core::SimHost

#endif
