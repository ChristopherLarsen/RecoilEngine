/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 draw: receiving the sim's typed display events (LUA-FREE H01). A draw or audio system registers once, when it is
// set up; the display sink calls it for each event of that type, in step and emit order, in the draw Update phase before
// any system's Update. A listener asks its own rule whether it runs now (SetEnabled switches draw rules in a run).

#ifndef KT1_DRAW_DISPLAY_LISTEN_H
#define KT1_DRAW_DISPLAY_LISTEN_H

#include <cstdint>
#include <functional>

#include "Game/KT1/Core/SimHost/DisplayQueue.h"

namespace KT1::DisplaySink {

using Listener = std::function<void(const Core::SimHost::DisplayEvent&)>;

void ListenRaw(int32_t type, Listener listener);
void ResetListeners();   // game shutdown

// fn(const T&) for every T the sim sends
template<class T, class F> void Listen(F fn) {
	ListenRaw(T::TYPE, [fn](const Core::SimHost::DisplayEvent& e) {
		T payload;
		if (e.Payload(payload)) fn(payload);
	});
}

} // namespace KT1::DisplaySink

#endif
