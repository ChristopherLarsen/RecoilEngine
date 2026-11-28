/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind OrderTypes.h (LUA-FREE H01; step S05 ports them).

#include "OrderTypes.h"

namespace KT1::Core::Orders {

CommandKind ParseCommandKind(std::string_view) { return CommandKind::Other; }
bool ParseCommandList(std::string_view, CommandMask&) { return false; }
const char* CommandKindName(CommandKind) { return ""; }
CommandKind CustomKind(int32_t) { return CommandKind::Other; }
int32_t CustomId(CommandKind) { return 0; }
const char* FireStateName(FireState) { return ""; }
bool ParseFireState(std::string_view, FireState&) { return false; }
const char* WorkKindName(WorkKind) { return ""; }
const char* FailReasonName(FailReason) { return ""; }

} // namespace KT1::Core::Orders
