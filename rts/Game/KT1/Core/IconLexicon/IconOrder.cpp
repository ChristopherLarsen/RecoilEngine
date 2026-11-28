/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: IconOrder. See IconOrder.h.

#include "IconOrder.h"

#include <algorithm>
#include <cassert>

namespace KT1::Core::IconLexicon {

void IconOrder::Init(int32_t maxIds, int32_t maxSlots) {
	slotOf.assign(static_cast<size_t>(std::max<int32_t>(0, maxIds)), NONE);
	entries.assign(static_cast<size_t>(std::max<int32_t>(0, maxSlots)), Entry{});
	sortBuf.clear();
	sortBuf.reserve(slotOf.size());              // every unit id may be drawable before the cap is applied
	runs.clear();
	runs.reserve(entries.size());
	count = 0;
	holes = 0;
	maxBorn = 0;
	runsValid = true;
}

bool IconOrder::Rebuild(std::span<const Entry> units) {
	for (Slot s = 0; s < count; ++s)
		slotOf[entries[s].id] = NONE;
	sortBuf.clear();
	for (const Entry& e: units)
		if (e.kind != Kind::None && InRange(e.id) && slotOf[e.id] == NONE) {
			assert(sortBuf.size() < sortBuf.capacity());
			slotOf[e.id] = 0;                         // marks the id taken: a unit listed twice is drawn once
			sortBuf.push_back(e);
		}
	std::sort(sortBuf.begin(), sortBuf.end(), Before);
	// past the cap the oldest units keep their icons, as the Lua's buffer did (icon_lex MAX_INST)
	const bool fits = sortBuf.size() <= entries.size();
	for (const Entry& e: sortBuf)
		slotOf[e.id] = NONE;
	count = static_cast<Slot>(std::min(sortBuf.size(), entries.size()));
	holes = 0;
	maxBorn = 0;
	for (Slot s = 0; s < count; ++s) {
		entries[s] = sortBuf[s];
		slotOf[sortBuf[s].id] = s;
		maxBorn = std::max(maxBorn, sortBuf[s].born);
	}
	runsValid = false;
	return fits;
}

IconOrder::Change IconOrder::Update(Id id, uint32_t born, Kind kind) {
	if (!InRange(id))
		return {};
	const Slot s = slotOf[id];
	if (s == NONE) {
		if (kind == Kind::None)
			return {};
		// only the newest unit has its place at the end; any other needs an insertion inside the order
		if (born <= maxBorn || count >= static_cast<Slot>(entries.size()))
			return {Action::Rebuild, NONE};
		entries[count] = {id, born, kind};
		slotOf[id] = count;
		maxBorn = born;
		runsValid = false;
		return {Action::Append, count++};
	}
	Entry& e = entries[s];
	if (kind == Kind::None) {
		if (e.kind == Kind::None)
			return {};
		e.kind = Kind::None;
		++holes;
		runsValid = false;
		return {Action::Clear, s};
	}
	if (e.kind == kind && kind == Kind::Contact)
		return {};                                // a contact's instance is re-sent each frame anyway
	if (e.kind == Kind::None)
		--holes;
	if (e.kind != kind)
		runsValid = false;
	e.kind = kind;
	return {Action::Write, s};
}

IconOrder::Change IconOrder::Remove(Id id) {
	if (!InRange(id) || slotOf[id] == NONE)
		return {};
	const Slot s = slotOf[id];
	slotOf[id] = NONE;
	Entry& e = entries[s];
	const bool wasDrawn = e.kind != Kind::None;
	if (wasDrawn)
		++holes;
	e.kind = Kind::None;
	e.id = id;                                    // kept for IdAt; slotOf no longer points here
	runsValid = false;
	return wasDrawn ? Change{Action::Clear, s} : Change{};
}

std::span<const IconOrder::Run> IconOrder::ContactRuns() {
	if (!runsValid) {
		runs.clear();
		for (Slot s = 0; s < count; ++s) {
			if (entries[s].kind != Kind::Contact)
				continue;
			if (!runs.empty() && runs.back().first + runs.back().count == s)
				++runs.back().count;
			else
				runs.push_back({s, 1});
		}
		runsValid = true;
	}
	return runs;
}

} // namespace KT1::Core::IconLexicon
