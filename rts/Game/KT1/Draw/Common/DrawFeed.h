/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: the feed of unit, feature and player events for draw systems: what a widget's call-ins gave. Ids and values
// only. The host (Host/DrawFeed.cpp) records each event when the engine raises it and hands them out in that order in
// the draw Update phase, after the display events and before any system's Update.
//
// A system listens once, when it is set up (registered at game setup), so by then the object of an event may be dead and
// its id reused: apply the events in order, and read what is true now from DrawView. A listener asks its own rule whether
// it runs now. Events from before the first Listen, or while a system was switched off, are not replayed: on setup and on
// Resumed() read UnitIds / FeatureIds. A change of the viewer (DrawView::Viewer) raises no unit events either: rebuild.
//
// Fog of war: an event arrives only where LuaUI's call-in of the same name arrives (the unit's or feature's ally team,
// the ally team that sees, or everything with a full view), and a value the viewer may not know is 0 or -1.
// So an enemy's death raises no UnitDestroyed: UnitRemoved (no Lua call-in) tells that a unit the viewer saw then has
// left the world.

#ifndef KT1_DRAW_FEED_H
#define KT1_DRAW_FEED_H

#include <cstdint>
#include <functional>

namespace KT1::DrawFeed {

enum class Kind : uint8_t {
	UnitCreated,        // other: the builder, -1 none or not seen
	UnitFinished,
	UnitDestroyed,      // the start of its death. other: the attacker, -1 none or not seen
	UnitRemoved,        // the unit is taken out of the world (any ally team's, if the viewer saw it just then)
	UnitEnteredLos,     // allyTeam: the one that sees
	UnitLeftLos,
	UnitEnteredRadar,
	UnitLeftRadar,
	UnitTaken,          // before the transfer; oldTeam, team (the new one)
	UnitGiven,          // after it; oldTeam, team (the new one)
	UnitCloaked,
	UnitDecloaked,
	UnitDamaged,        // damage, paralyzer, weaponDefId; other: the attacker, -1 none or not seen
	FeatureCreated,
	FeatureDestroyed,
	PlayerChanged,      // id: the player
	Count
};

using Mask = uint32_t;
constexpr Mask Bit(Kind k) { return Mask(1) << static_cast<unsigned>(k); }
static_assert(static_cast<unsigned>(Kind::Count) <= 32, "Mask holds one bit per kind");

struct Event {
	Kind kind = Kind::UnitCreated;
	bool paralyzer = false;
	int32_t id = -1;            // the unit or feature
	int32_t defId = 0;          // its type as far as the viewer knew it then; 0: unknown
	int32_t team = -1;          // its team (Taken, Given: the new one)
	int32_t allyTeam = -1;      // sight events: who sees; a feature: its own (-1 none); else the unit's at the event
	int32_t oldTeam = -1;       // Taken, Given
	int32_t other = -1;         // see the kinds
	int32_t weaponDefId = -1;   // UnitDamaged, UnitDestroyed
	float damage = 0.0f;        // UnitDamaged
	int32_t step = 0;           // the sim step it happened in
};

using Listener = std::function<void(const Event&)>;

// the listener gets every event whose kind is in `kinds`; listeners are called in the order they were added
void Listen(Mask kinds, Listener listener);

void Deliver();   // host: the draw Update phase
void Reset();     // host: game shutdown: every listener and queued event is forgotten

} // namespace KT1::DrawFeed

#endif
