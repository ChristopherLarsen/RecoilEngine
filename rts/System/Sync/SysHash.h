/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 per-system sync hashes (NEW-ENGINE-PLAN S3). Diagnostic only: reads synced state, writes nothing synced.
#pragma once

namespace KT1 {
namespace SysHash {
	// Called at the end of every sim frame. With SysHashPeriodS > 0 it hashes each system every frame into a short ring and
	// logs "[SYSHASH] f=<frame> units=<h> features=<h> projectiles=<h> teams=<h> n=<units>" every SysHashPeriodS seconds.
	void Frame(int frameNum);
	// Called when the server reports a desync (NETMSG_GAMESTATE_DUMP): logs the ring's per-system lines and, for the frames
	// in the ring, one "[SYSHASHU] f=<frame> id=<unitID> h=<h>" line per unit, so tools/net/syshash_diff.py can name the
	// first differing system and unit between two clients' logs.
	void DumpRing(int desyncFrame);
}
}
