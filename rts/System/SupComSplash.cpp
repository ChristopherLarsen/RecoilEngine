/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
#include "SupComSplash.h"

#include <cstdio>
#include <string>

#include "Rendering/GL/myGL.h"
#include "System/Config/ConfigHandler.h"

CONFIG(bool, SplashScreenText).defaultValue(true).description("supcom: 0 hides all splash screen text (the game shows the engine licence in its credits).");
CONFIG(std::string, LoadScreenClearColor).defaultValue("5 9 13").description("supcom: \"r g b\" (0-255) the load screen is cleared to until LuaIntro draws.");

bool SupComSplash::ShowProgressText() { return configHandler->GetBool("SplashScreenText"); }

void SupComSplash::ClearBeforeIntro()
{
	static int r = -1, g = 0, b = 0;
	if (r < 0 && std::sscanf(configHandler->GetString("LoadScreenClearColor").c_str(), "%d %d %d", &r, &g, &b) != 3)
		r = 5, g = 9, b = 13;
	glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}
