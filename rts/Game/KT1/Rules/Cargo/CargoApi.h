/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Cargo: the transport room calls other native systems make (LUA-FREE S12; plan ruling 2026-11-28: pulled forward
// from GLUE, whose S13 takes this folder over). They mirror GG.SpcCargo.SelectLoad, Full and NoteRefused of spc_cargo.lua,
// which keeps its own copy until GLUE moves. Plain functions with no switch: used while the orders switch is on.
// Room state comes from the engine (the cargo aboard in attach order and their defs), equal to spc_cargo's loadSeq/alloc.

#ifndef KT1_RULES_CARGO_API_H
#define KT1_RULES_CARGO_API_H

#include <cstdint>
#include <span>
#include <vector>

namespace KT1::Cargo {

// once at game setup (reads gamedata/kt1/cargo.lua and the unit defs)
void Init();
// the candidates taken and left, each in the given order
void SelectLoad(int32_t transport, std::span<const int32_t> candidates, std::vector<int32_t>& taken, std::vector<int32_t>& left);
bool Full(int32_t transport);
// rules params cargo_refused_n and cargo_refused_f (allied); add: joins a note given within NOTE_JOIN steps. Not written
// in check mode. GG.SpcCargo.notes (read only by the dev cases ord_ferry and dev_ferry_test) is the Lua side's alone.
void NoteRefused(int32_t transport, int32_t n, bool add, int32_t frame);

} // namespace KT1::Cargo

#endif
