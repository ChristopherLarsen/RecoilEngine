/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1: the shared half of an event map row (EFFECTS' FxEvents and FRAME's sound cues, LUA-FREE H01): which sim
// happening, and which units and weapons it is about. Each family's row adds what it raises. No engine include.
//
//     {"on": "UnitDestroyed", "unit_class": "", "weapon": "", "relation": "any", ...the family's keys}
// Describe is a template over the config schema (Data/Schema.h): Core code includes no Data header.
// "on" is a sim event name (Core/SimHost/SimEventLog.h) or a trigger name the family documents for a happening that is
// not an engine event. An empty filter matches everything.

#ifndef KT1_CORE_EVENT_MAP_ROW_H
#define KT1_CORE_EVENT_MAP_ROW_H

#include <cstdint>
#include <string>

namespace KT1::Core {

enum class TeamRelation : uint8_t { Any, Own, Allied, Enemy };

struct EventMapRow {
	std::string on;                 // the sim event or trigger name
	std::string unit_class;         // the subject's class; empty: any
	std::string weapon;             // the weapon def name; empty: any
	std::string relation = "any";   // any, own, allied, enemy (of the viewer's team)

	template<class S> void Describe(S& s) {
		s.Field("on", on);
		s.Field("unit_class", unit_class);
		s.Field("weapon", weapon);
		s.Field("relation", relation);
	}
};

// "own" -> Own; anything unknown: Any
inline TeamRelation ParseRelation(const std::string& value) {
	if (value == "own") return TeamRelation::Own;
	if (value == "allied") return TeamRelation::Allied;
	return value == "enemy" ? TeamRelation::Enemy : TeamRelation::Any;
}

} // namespace KT1::Core

#endif
