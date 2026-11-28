/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Host: native CommandFallback (LUA-FREE H01, ORDERS): a system's handling of a command the engine's command AI does
// not know, asked before the Lua gadgets'. Registered at game setup, like an AllowChain validator.
//
// UnitCommand, UnitCmdDone with its command, and call-ins that cannot wait for the step's event batch need no hook here:
// an adapter's .cpp declares its own event client and runs it inside a Check::Running scope.
//
// Live: the first system that answers Keep or Done decides and Lua is not asked. Check: the system never decides; its
// answer is compared with the replaced handler's, which reports it from inside the call:
//   Spring.KT1.Check.Answer("Orders", "CommandFallback", unitID, cmdID, 0, used, remove)

#ifndef KT1_HOST_COMMAND_HOOKS_H
#define KT1_HOST_COMMAND_HOOKS_H

#include <cstdint>
#include <functional>

#include "AllowChain.h"

namespace KT1 {

enum class Fallback : uint8_t {
	Pass,   // not this system's command
	Keep,   // handled: the command stays at the front of the queue
	Done    // handled: the engine removes the command
};

class CommandHooks {
public:
	using FallbackFn = std::function<Fallback(const CUnit*, const Command&, Faked)>;

	static void AddFallback(const char* system, int order, FallbackFn fn);
	static void Reset();   // game shutdown

	// KT1 hook: SimHost (CEventHandler::CommandFallback). true: a live system handled it, `remove` is the engine's answer
	static bool AskFallback(const CUnit* u, const Command& c, bool& remove);
};

} // namespace KT1

#endif
