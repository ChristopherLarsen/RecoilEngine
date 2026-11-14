/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// supcom: the start-up look before the game draws (Kardashev launch screen)
#ifndef SUPCOM_SPLASH_H
#define SUPCOM_SPLASH_H

namespace SupComSplash {
	// config SplashScreenText (default 1): 0 hides the splash progress lines ("[Initializing Virtual File System]", archives,
	// scan time); the version and licence notice stay
	bool ShowProgressText();
	// config LoadScreenClearColor ("r g b" 0-255, default "5 9 13"): the load screen is cleared to it until LuaIntro draws
	void ClearBeforeIntro();
}

#endif
