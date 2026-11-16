/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef SUPCOM_FRAME_STEP_H
#define SUPCOM_FRAME_STEP_H

#include <memory>
#include <string>
#include <string>

namespace netcode { class RawPacket; }

// supcom: unsynced action "SupComStep [N|off]" for deterministic captures (weapon VFX). "SupComStep N" (default 1)
// lets exactly N more sim frames run, then holds: the client stops consuming NETMSG_NEWFRAME/KEYFRAME and asks the
// server to pause. Frames the server sent before the pause took effect wait in the queue and are used by the next
// step, so each step advances exactly N frames. "SupComStep off" ends stepping (frames flow again; pause state is
// left as it is). Nothing changes the simulation; while no step was ever requested, Hold() is always false (stock).
namespace SupComFrameStep {
	void Command(const std::string& args);

	/// ClientReadNet: true when the next packet is a sim frame that must wait for the next step
	bool Hold(const std::shared_ptr<const netcode::RawPacket>& packet);
}

#endif
