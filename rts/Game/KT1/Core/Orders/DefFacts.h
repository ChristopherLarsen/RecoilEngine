/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: what the family knows of a unit def. The adapter reads it once per def at Start (State::defs, indexed
// by unit def id); Orders, AMove and OrderRules all read it. No engine include.

#ifndef KT1_CORE_ORDERS_DEF_FACTS_H
#define KT1_CORE_ORDERS_DEF_FACTS_H

#include <cstdint>
#include <string_view>

#include "OrderTypes.h"

namespace KT1::Core::Orders {

struct FireConfig;   // OrdersConfig.h

enum class DefFlag : uint32_t {
	CanMove,
	CanFly,
	Building,          // UnitDef isBuilding
	Factory,           // isFactory
	FactoryOrPlant,    // isFactory or customParams plant "1" (the guard chain's end)
	Repairs,           // canRepair with a repair or build speed over 0
	IsBuilder,         // isBuilder
	Assists,           // build speed over 0 and (isBuilder or canAssist): the Lua's isBuilderDef
	Reclaims,          // canReclaim is not false and build speed over 0
	CanReclaim,        // canReclaim (ord_edge area reclaim)
	CommandUnit,       // customParams command_unit "1"
	Reclaimable,       // not a command or support unit and customParams eco_reclaimable is not "0" (ECO-019 R1)
	Submarine,         // customParams supcom_submarine "1"
	StartsSubmerged,   // customParams supcom_start_submerged "1"
	Transport,         // isTransport or transportCapacity over 0
	OwnMove,           // customParams supcom_move "1": the fork's ground move type
	AirLoadHeight,     // customParams supcom_air_load_height is set
	UnloadAll,         // customParams supcom_unload_all "1"
	Naval,             // the ship speed-mod class or move family "ship"
	IndirectFire,      // a weapon with customParams arc "high"
	IdleLands,         // customParams supcom_air "1" with supcom_air_idle_land_delay
	HasAccepts,        // an accepted-order set from the pack (orders.accepts or customParams ord_accepts)
	HasSpecAccepts,    // customParams spc_accepts is set
};
constexpr uint32_t BitOf(DefFlag flag) { return uint32_t(1) << static_cast<unsigned>(flag); }

// the words of an accepted-order set
enum class AcceptWord : uint8_t {
	Move, Attack, Patrol, Guard, FireState, Reclaim, Repair, Capture, Pause, SelfDestruct, AttackMove,
	Count
};
constexpr uint32_t BitOf(AcceptWord word) { return uint32_t(1) << static_cast<unsigned>(word); }

// "attack,attack_move,guard,...": the words' bits; a word the family does not check is left out
uint32_t ParseAccepts(std::string_view commaList);
bool AcceptWordOf(CommandKind kind, AcceptWord& word);   // false: no accepted-order set names this kind

struct DefFacts {
	float reach = 0;              // the longest non-interceptor weapon range, elmos; 0: unarmed
	float primaryRange = 0;       // weapon 1's range, elmos; 0: no weapon (the attack-move strip is a factor of it)
	float vision = 0;             // elmos: customParams int_vision (map units) or sightDistance
	float buildReach = 0;         // buildDistance, elmos
	float speed = 0;              // UnitDef speed, elmos per second
	float halfX = 0;              // half footprint, elmos
	float halfZ = 0;
	float waypointAdvance = 0;    // waypoint advance radius, elmos; 0: none (the arrival radius)
	float scanPeriodS = 0;        // the first scanned weapon's interval, seconds; 0: none
	float landShortGap = 0;       // customParams air_land_short_gap, elmos (a transport with cargo room); 0: none
	float cargoShortGap = 0;      // customParams air_cargo_short_gap, elmos
	int32_t tier = 1;             // customParams tier; DESIGN: 1 when absent
	int32_t transportRow = -1;    // row of TransportConfig::codes for the def's eco code; -1: none
	uint32_t accepts = 0;         // AcceptWord bits, meaningful with HasAccepts
	uint32_t specAccepts = 0;     // AcceptWord bits, meaningful with HasSpecAccepts
	uint32_t flags = 0;           // DefFlag bits

	bool Has(DefFlag flag) const { return (flags & BitOf(flag)) != 0; }
	void Set(DefFlag flag, bool on) { flags = on ? (flags | BitOf(flag)) : (flags & ~BitOf(flag)); }
	bool Armed() const { return reach > 0; }
	bool Air() const { return Has(DefFlag::CanFly); }
	bool GroundMover() const { return Has(DefFlag::CanMove) && !Has(DefFlag::CanFly); }
};

// ORD-010 v6 R1: a new unit's fire state (cannot move; then experimental tier; then command unit; then any other)
FireState DefaultFireState(const DefFacts& def, const FireConfig& config);

} // namespace KT1::Core::Orders

#endif
