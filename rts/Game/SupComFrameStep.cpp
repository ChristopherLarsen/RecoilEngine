/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SupComFrameStep.h"

#include <algorithm>
#include <cstdlib>
#include <algorithm>
#include <cstdlib>
#include <string>

#include "Game/Game.h"
#include "Game/GlobalUnsynced.h"
#include "Net/Protocol/BaseNetProtocol.h"
#include "Net/Protocol/NetProtocol.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Log/ILog.h"
#include "System/Net/RawPacket.h"
#include "System/StringUtil.h"

namespace {
	bool active = false;
	bool held = false;
	int remaining = 0; // frames still allowed before holding

	void SetPaused(bool p) {
		if (game == nullptr || game->paused == p)
			return;
		game->paused = p;
		clientNet->Send(CBaseNetProtocol::Get().SendPause(gu->myPlayerNum, p));
	}
}

void SupComFrameStep::Command(const std::string& args)
{
	if (game == nullptr || !game->playing)
		return;

	if (StringToLower(args) == "off") {
		active = false; held = false; remaining = 0;
		LOG("[SupComStep] off at frame %d", gs->frameNum);
		return;
	}

	int n = 1; // frames per step when no count is given
	if (!args.empty())
		n = std::max(1, std::atoi(args.c_str()));

	active = true;
	held = false;
	remaining += n;
	SetPaused(false);
	LOG("[SupComStep] step %d from frame %d", n, gs->frameNum);
}

bool SupComFrameStep::Hold(const std::shared_ptr<const netcode::RawPacket>& packet)
{
	if (!active || packet == nullptr)
		return false;

	const unsigned char type = packet->data[0];
	if (type != NETMSG_NEWFRAME && type != NETMSG_KEYFRAME)
		return false;

	if (remaining > 0) {
		remaining -= 1;
		return false;
	}

	if (!held) {
		held = true;
		SetPaused(true);
		LOG("[SupComStep] held at frame %d", gs->frameNum);
	}
	return true;
}
