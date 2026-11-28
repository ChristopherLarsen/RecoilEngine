/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders core: stubs behind DefFacts.h (LUA-FREE H01; step S05 ports them).

#include "DefFacts.h"

namespace KT1::Core::Orders {

uint32_t ParseAccepts(std::string_view) { return 0; }
bool AcceptWordOf(CommandKind, AcceptWord&) { return false; }
FireState DefaultFireState(const DefFacts&, const FireConfig&) { return FireState::Hold; }

} // namespace KT1::Core::Orders
