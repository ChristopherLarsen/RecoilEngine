/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

#pragma once

// Presents the EGL bridge's frames straight through a CAMetalLayer: persistent upload buffers and a GPU blit into the
// drawable, instead of SDL's Metal renderer, which allocates a fresh upload buffer for every frame (about 10 ms of page
// faults per 3840x2160 frame on an Intel Mac). Driven through the Objective-C runtime, so it builds with GCC.

#include "MacEGLBridge.h"

#ifdef MAC_EGL_BRIDGE_AVAILABLE
struct SDL_Window;

namespace MacMetalPresenter {
	// true when the springsetting asks for it and Metal is set up on the window
	bool Init(SDL_Window* window);
	void Kill();
	bool Active();

	// bgra: w x h pixels, bottom row first (as glReadPixels returns them); copied flipped into an upload buffer
	bool Present(const void* bgra, int w, int h);
	void SetVSync(bool on);
}
#endif
