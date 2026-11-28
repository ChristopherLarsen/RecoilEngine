/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: a list kept in a fixed order. No engine include.
//
// Systems register themselves from static initialisers and Start calls, whose order across files is not defined. The
// order that reaches the sim is therefore only (order, name): the same on every machine and in every build.

#ifndef KT1_ORDERED_H
#define KT1_ORDERED_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace KT1::SimHost {

template<class T> class Ordered {
public:
	struct Item {
		int order;
		std::string name;
		T value;
	};

	// Adding while the list is being walked is not allowed: register at Start.
	void Add(int order, std::string name, T value) {
		const auto at = std::find_if(items.begin(), items.end(), [&](const Item& i) {
			return (order != i.order) ? (order < i.order) : (name < i.name);
		});
		items.insert(at, Item{order, std::move(name), std::move(value)});
	}

	bool Empty() const { return items.empty(); }
	size_t Size() const { return items.size(); }
	void Clear() { items.clear(); }

	auto begin() const { return items.begin(); }
	auto end() const { return items.end(); }
	auto begin() { return items.begin(); }
	auto end() { return items.end(); }

private:
	std::vector<Item> items;
};

// A veto chain: every validator in order, the first "no" ends it (the Lua gadget handler's rule for Allow* call-ins).
// KT1 hot
template<class Fn, class... A> bool AllAllow(const Ordered<Fn>& chain, A&&... args) {
	for (const auto& item : chain)
		if (!item.value(args...)) return false;
	return true;
}

} // namespace KT1::SimHost

#endif
