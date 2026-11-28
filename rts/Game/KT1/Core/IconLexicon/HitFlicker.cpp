/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: HitFlicker. See HitFlicker.h.

#include "HitFlicker.h"

#include <algorithm>

namespace KT1::Core::IconLexicon {

void HitFlicker::Init(int32_t maxIds, float windowSeconds) {
	posOf.assign(static_cast<size_t>(std::max<int32_t>(0, maxIds)), -1);
	live.clear();
	live.reserve(posOf.size());
	writes.clear();
	writes.reserve(posOf.size());
	window = windowSeconds;
}

void HitFlicker::Hit(Id id, double now) {   // KT1 lint: allow double draw-time seconds from the engine clock (unsynced; a long game keeps sub-ms resolution)
	if (id < 0 || id >= static_cast<Id>(posOf.size()))
		return;
	if (posOf[id] >= 0) {
		live[posOf[id]].start = now;
		return;
	}
	posOf[id] = static_cast<int32_t>(live.size());
	live.push_back({id, now, false});
}

void HitFlicker::Drop(size_t i) {
	posOf[live[i].id] = -1;
	if (i + 1 != live.size()) {
		live[i] = live.back();
		posOf[live[i].id] = static_cast<int32_t>(i);
	}
	live.pop_back();
}

void HitFlicker::Forget(Id id) {
	if (id >= 0 && id < static_cast<Id>(posOf.size()) && posOf[id] >= 0)
		Drop(static_cast<size_t>(posOf[id]));
}

void HitFlicker::Rebuilt() {
	for (Live& l: live)
		l.red = false;
}

void HitFlicker::Rewritten(Id id) {
	if (id >= 0 && id < static_cast<Id>(posOf.size()) && posOf[id] >= 0)
		live[posOf[id]].red = false;
}

std::span<const HitFlicker::Write> HitFlicker::Tick(double now) {   // KT1 lint: allow double draw-time seconds from the engine clock (unsynced; a long game keeps sub-ms resolution)
	writes.clear();
	for (size_t i = 0; i < live.size();) {
		Live& l = live[i];
		if (now - l.start >= window) {
			if (l.red)
				writes.push_back({l.id, false});
			Drop(i);                                  // the last entry moved into i: look at i again
			continue;
		}
		if (!l.red) {
			l.red = true;
			writes.push_back({l.id, true});
		}
		++i;
	}
	return writes;
}

} // namespace KT1::Core::IconLexicon
