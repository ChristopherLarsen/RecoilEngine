/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core/IconLexicon: HitFlicker - a damaged unit's icon shows red for a short window (GAME-DESIGN 1, owner 2026-11-21:
//   about 0.15 s; DESIGN). Presentation only: nothing here reaches the sim.
// Replaces icon_lex.lua FlickerTick / UnitDamaged. The window is in seconds of draw time (rate-agnostic: no frame count).
// Each drawn frame Tick returns the colour writes to make: Red (the unit's colour becomes the flicker colour) or Restore
// (its own colour again). A rebuild or a rewrite of the unit's instance puts its own colour back, so the red is applied again
// while the window is open (Rebuilt, Rewritten).
// Steady state: no allocation (Init sizes everything). No engine include.

#ifndef KT1_CORE_ICON_LEXICON_HIT_FLICKER_H
#define KT1_CORE_ICON_LEXICON_HIT_FLICKER_H

#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Core::IconLexicon {

class HitFlicker {
public:
	using Id = int32_t;
	struct Write {
		Id id = 0;
		bool red = false; // true: paint the flicker colour; false: restore the unit's own colour
	};

	// ids 0 .. maxIds-1; window: seconds the icon shows red
	void Init(int32_t maxIds, float windowSeconds);

	// The unit took damage at draw time `now` (seconds). A hit inside the window restarts it.
	void Hit(Id id, double now);   // KT1 lint: allow double draw-time seconds from the engine clock (unsynced; a long game keeps sub-ms resolution)
	// The unit is gone: no write for it.
	void Forget(Id id);
	// The whole buffer was rebuilt / this unit's instance was rewritten with its own colour.
	void Rebuilt();
	void Rewritten(Id id);

	// The writes for this drawn frame, valid until the next call. Ends windows that are over.
	std::span<const Write> Tick(double now);   // KT1 lint: allow double draw-time seconds from the engine clock (unsynced; a long game keeps sub-ms resolution)

	int32_t Active() const { return static_cast<int32_t>(live.size()); }

private:
	struct Live { Id id; double start; bool red; };   // KT1 lint: allow double draw-time seconds from the engine clock (unsynced; a long game keeps sub-ms resolution)
	void Drop(size_t i);

	std::vector<int32_t> posOf; // id -> index in live, or -1
	std::vector<Live> live;
	std::vector<Write> writes;
	float window = 0;
};

} // namespace KT1::Core::IconLexicon

#endif
