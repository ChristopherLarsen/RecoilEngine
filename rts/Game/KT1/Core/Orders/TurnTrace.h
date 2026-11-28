/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: the busy-unit turn in a trace (LUA-FREE H02, Core/Trace/TraceFile.h). The input is one row per step,
// Trace::Input("Orders", "turn", bytes), holding a whole TurnInput (TurnStep.h) field by field; the first row carries
// the init. The compared columns, under the check helper's keys:
//   front_class  (unit, 0, 0)   FrontClass of each unit given a turn
//   next_check   (unit, 0, 0)   nextCheck[u] after its turn, -1 unset
//   turn_list    (0, 0, 1) the length, then (0, i, 0) the unit, i from 0
// The Lua's front_class, from its locals after the classification: None when id == nil; else Assist when AssistStep
// turned the order into the guard; else Guard when id == CMD.GUARD; else EngineWork when an entry ahead of the order is
// an engine-work command that is not the unit's own step (ours[u].tag); else OwnFightOrPatrol when id is FIGHT or
// PATROL; else Plain.
// No engine include.

#ifndef KT1_CORE_ORDERS_TURN_TRACE_H
#define KT1_CORE_ORDERS_TURN_TRACE_H

#include "TurnStep.h"
#include "Game/KT1/Core/Trace/TraceReplay.h"

namespace KT1::Core::Orders {

void Put(Trace::Bytes& bytes, const TurnInput& in);
// false: the bytes are short, left over, or hold a count or a kind out of range; `in` is then not to be used
bool Get(Trace::ByteReader& bytes, TurnInput& in);

// a step's computed columns, for a replay's EndStep
void GiveTurnColumns(const TurnOutput& out, Trace::Outputs& columns);

} // namespace KT1::Core::Orders

#endif
