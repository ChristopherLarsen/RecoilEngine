/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: asking a chain of native validators. No engine include.
//
// A live validator decides: in a veto chain the first "no" ends it. A check-mode validator never decides: it is asked and
// its answer goes to the check helper, which compares it with the answer of the one Lua handler it replaces.

#ifndef KT1_VALIDATORS_H
#define KT1_VALIDATORS_H

#include <string>

#include "Ordered.h"
#include "WriteGate.h"

namespace KT1::Core::SimHost {

template<class Fn> struct Validator {
	std::string system;
	bool check;     // true: its system's switch is "check" (the host asks the registry)
	Fn fn;
};

template<class Fn> using Chain = Ordered<Validator<Fn>>;

// A veto chain in order, each validator running behind the write gate; checked(system, answer) takes each check-mode
// answer. False: a live validator refused, and the chain and Lua are not asked further.
// KT1 hot
template<class Fn, class Checked, class... Args>
bool AskVeto(const Chain<Fn>& chain, WriteGate& gate, Checked&& checked, Args&&... args) {
	for (const auto& item : chain) {
		const WriteGate::Scope running(gate, item.value.system.c_str(), item.value.check);
		const bool yes = item.value.fn(args...);
		if (item.value.check) checked(item.value.system, yes);
		else if (!yes) return false;
	}
	return true;
}

} // namespace KT1::Core::SimHost

#endif
