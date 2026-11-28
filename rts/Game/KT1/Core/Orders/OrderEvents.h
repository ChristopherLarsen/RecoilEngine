/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Orders: the family's typed display events (Core/Common/DisplayTypes.h). The sim sends them with
// DisplayEvents::Send; a draw or audio system registers with DisplaySink::Listen<T>. No engine include.

#ifndef KT1_CORE_ORDERS_ORDER_EVENTS_H
#define KT1_CORE_ORDERS_ORDER_EVENTS_H

#include <cstdint>

#include "Game/KT1/Core/Common/DisplayTypes.h"

namespace KT1::Core::Orders {

// An order carried out by the sim itself (so the engine reports no UnitCommand): the acknowledgement mark at its point
// (UI-153; the Lua's "order_ack": ferry routes here, silo launches in family SPECIAL). The draw side shows it to the
// viewer's own team only.
struct OrderAck {
	static constexpr int32_t TYPE = Core::DisplayRange::ORDERS + 0;
	int32_t unit = -1;
	int32_t command = 0;             // the command id (CUSTOM_FERRY, ...)
	float x = 0, y = 0, z = 0;       // the order's point
	int32_t team = -1;
};

// A tracked move ended as completed-with-failure. No live reader today (test cases read rules param order_failed): the
// deletion pass (H04) decides whether it stays.
struct OrderFailed {
	static constexpr int32_t TYPE = Core::DisplayRange::ORDERS + 1;
	int32_t unit = -1;
	int32_t team = -1;
	float x = 0, y = 0, z = 0;       // where the unit stood
	uint8_t reason = 0;              // Core::Orders::FailReason
};

} // namespace KT1::Core::Orders

#endif
