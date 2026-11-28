/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: IconOrder - the strategic icons' instance slots in draw order (LUA-FREE-WORKPACKAGES D01).
// Why: icons overlap without spreading and later-created units draw on top (CAM-003 v5 R10; icon_shape.lua Before), so the
//   instance buffer is kept in creation order (born counter, then unit id). The buffer is rebuilt only when that order must
//   change; a unit whose kind changes (create, sight, radar, give) is rewritten in its own slot, and the newest unit is
//   appended. This is icon_lex.lua's IncApply / IncGone / GpuRebuild bookkeeping without the GL calls: the adapter applies
//   each returned Change to its instance batch (one changed range each).
// A removed unit leaves a hole (its slot holds Kind::None, drawn as nothing) until the next rebuild.
// Steady state: no allocation (Init sizes everything). No engine include.

#ifndef KT1_CORE_ICON_LEXICON_ICON_ORDER_H
#define KT1_CORE_ICON_LEXICON_ICON_ORDER_H

#include "IconLexiconCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Core::IconLexicon {

class IconOrder {
public:
	using Id = int32_t;
	using Slot = int32_t;
	static constexpr Slot NONE = -1;

	struct Entry {
		Id id = 0;
		uint32_t born = 0; // creation counter: later units draw on top
		Kind kind = Kind::None;
	};

	enum class Action : uint8_t {
		None,    // nothing to send
		Write,   // write the unit's instance into `slot` (its kind is KindAt(slot))
		Clear,   // zero `slot` (the unit is no longer drawn)
		Append,  // write the unit's instance into the new last slot `slot`
		Rebuild, // the order must change: call Rebuild with every drawn unit, then send the whole buffer
	};
	struct Change {
		Action action = Action::None;
		Slot slot = NONE;
	};

	// ids 0 .. maxIds-1; at most maxSlots instances. Allocates once.
	void Init(int32_t maxIds, int32_t maxSlots);

	// Every unit to draw (kind Full or Contact; None entries are skipped), in any order. Sorts them into draw order in a
	// buffer of its own. Returns false when there were more than maxSlots: the oldest maxSlots are kept, as the Lua did.
	bool Rebuild(std::span<const Entry> units);

	// A unit's kind may have changed (UnitCreated, sight and radar events, given, taken, cloak, selection, hover).
	// Full is always rewritten: its colour, flags or box may differ although the kind is the same.
	Change Update(Id id, uint32_t born, Kind kind);

	// The unit is gone (the engine removed it). Its slot becomes a hole.
	Change Remove(Id id);

	Slot Count() const { return count; }       // slots in use, holes included
	Slot SlotOf(Id id) const { return InRange(id) ? slotOf[id] : NONE; }
	Id IdAt(Slot s) const { return entries[s].id; }
	Kind KindAt(Slot s) const { return entries[s].kind; }
	int32_t Holes() const { return holes; }

	// Stretches of neighbouring contact slots: each is re-sent with one range upload every drawn frame (contacts move with
	// the radar error). Recomputed by ContactRuns after any change; no allocation.
	struct Run { Slot first = 0; Slot count = 0; };
	std::span<const Run> ContactRuns();

private:
	bool InRange(Id id) const { return id >= 0 && id < static_cast<Id>(slotOf.size()); }
	static bool Before(const Entry& a, const Entry& b) { return a.born != b.born ? a.born < b.born : a.id < b.id; }

	std::vector<Slot> slotOf;   // id -> slot or NONE
	std::vector<Entry> entries; // slot -> entry, size maxSlots
	std::vector<Entry> sortBuf; // Rebuild's working copy
	std::vector<Run> runs;
	Slot count = 0;
	int32_t holes = 0;
	uint32_t maxBorn = 0;       // the newest born counter in the buffer: a newer unit can be appended
	bool runsValid = true;
};

} // namespace KT1::Core::IconLexicon

#endif
