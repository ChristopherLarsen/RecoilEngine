/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 Capture: the JSON text of a capture script into the Core's Script. No engine include.

#ifndef KT1_CAPTURE_SCRIPT_H
#define KT1_CAPTURE_SCRIPT_H

#include <string>

#include "Game/KT1/Core/Capture/CaptureCore.h"

namespace KT1::CaptureCore {
	// Reads the JSON text of a script into `script` (which holds the defaults on the way in) and checks it. "" when the
	// script can be run, else the first reason it cannot. Unknown keys are reasons.
	std::string ParseScript(const std::string& text, Script& script);
}

#endif
