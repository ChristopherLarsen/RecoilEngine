/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Core: transport room (REQ ORD-009 v2 R1-R3, ORD-011 v3 R1, R2), the three calls the ferry makes into spc_cargo.lua
// (SelectLoad, Full; NoteRefused is the adapter's), pulled forward for ORDERS S12 (plan ruling 2026-11-28: a native system
// never calls Lua). GLUE's S13 takes this folder over. Ported as the Lua has it; spc_cargo.lua keeps its own copy until
// GLUE moves, so both must pick alike. No engine include.
//
// A transport has three room counters (small, medium, large). A unit of size class z fits if, in some class k with
// need[z][k] > 0, the room left is at least that need; loading takes use[z][k] from every class.

#ifndef KT1_CORE_CARGO_ROOM_H
#define KT1_CORE_CARGO_ROOM_H

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Core::Cargo {

constexpr int32_t CLASSES = 3;                 // small, medium, large
constexpr int32_t NO_SIZE = -1;                // a unit with no carry_class
using Room = std::array<float, CLASSES>;

// spc_helpers.lua V.cargo (or the roster's G0-PFD [cargo]): by size class, the room needed and taken in each class
struct RoomModel {
	std::array<Room, CLASSES> need{};
	std::array<Room, CLASSES> use{};
};

// one transport as the room rules see it
struct Transport {
	bool hasSlots = false;                     // customParams spc_slots_*
	Room slots{};
	int32_t capacity = 1;                      // UnitDef transportCapacity (the Lua's `or 1`)
	std::vector<int32_t> aboard;               // the size class of each unit aboard, in loading order (no class: small)
};

// Lua Used(t): the room the cargo aboard takes, summed in loading order
Room Used(const Transport& t, const RoomModel& m);
// Lua Place(t, c, extra): the size class the unit takes if it fits beside `extra`, else NO_SIZE (no slots: the caller's case)
int32_t Place(const Transport& t, int32_t size, const Room& extra, const RoomModel& m);
// Lua GG.SpcCargo.SelectLoad(t, cands): indices into `sizes` taken and left, each in the given order
void SelectLoad(const Transport& t, std::span<const int32_t> sizes, const RoomModel& m, std::vector<int32_t>& taken,
	std::vector<int32_t>& left);
// Lua GG.SpcCargo.Full(t)
bool Full(const Transport& t, const RoomModel& m);

} // namespace KT1::Core::Cargo

#endif
