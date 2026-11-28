/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture: the one question the frame loop asks the native frame capture (LUA-FREE-WORKPACKAGES F07). The capture
// itself is a draw system (KT1Capture.cpp). Included by the one stock file with the "// KT1 hook: Capture" line.

#ifndef KT1_CAPTURE_H
#define KT1_CAPTURE_H

#include <atomic>
#include <climits>

namespace KT1::Capture {
	// The last sim step the server may issue (INT_MAX: no limit; only a capture run, kt1_capture=1 on a solo host, sets
	// one). Header-only for the server: the dedicated server compiles GameServer.cpp but links no KT1 source.
	inline std::atomic<int> holdStep{INT_MAX};

	// True when no step after `step` (the last one issued) may be issued now. A hold only delays steps, as a pause does:
	// none is dropped or merged, and what each computes is unchanged. Callers depend on this function only.
	inline bool HoldAt(int step) { return step >= holdStep.load(std::memory_order_acquire); }
}

#endif
