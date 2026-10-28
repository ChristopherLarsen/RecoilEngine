/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "InputHandler.h"
#include "System/TimeProfiler.h"

#ifdef __APPLE__
#include "System/Platform/Mac/Keyboard.h"
#endif
#include "Rendering/GL/MacEGLBridge.h"
#ifdef MAC_EGL_BRIDGE_AVAILABLE
#include "Rendering/GlobalRendering.h"
#endif

InputHandler input;

InputHandler::InputHandler() = default;

void InputHandler::PushEvent(const SDL_Event& ev)
{
	for (const auto& eventHandler : eventHandlers) {
		if (eventHandler) {
			if (eventHandler(ev))
				break;
		}
	}
}

#ifdef MAC_EGL_BRIDGE_AVAILABLE
// SDL reports mouse positions and window sizes in points, the engine works in window pixels
static void ScaleToPixels(SDL_Event& ev)
{
	if (globalRendering == nullptr || globalRendering->pixelRatio == 1.0f)
		return;

	const float r = globalRendering->pixelRatio;
	const auto px = [r](Sint32& v) { v = static_cast<Sint32>(v * r); };

	switch (ev.type) {
		case SDL_MOUSEMOTION: {
			px(ev.motion.x); px(ev.motion.y);
			px(ev.motion.xrel); px(ev.motion.yrel);
		} break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			px(ev.button.x); px(ev.button.y);
		} break;
		case SDL_MOUSEWHEEL: {
			px(ev.wheel.mouseX); px(ev.wheel.mouseY);
		} break;
		case SDL_WINDOWEVENT: {
			if (ev.window.event == SDL_WINDOWEVENT_RESIZED || ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				px(ev.window.data1); px(ev.window.data2);
			}
		} break;
		default: break;
	}
}
#endif

void InputHandler::PushEvents()
{
	SCOPED_TIMER("Misc::InputHandler::PushEvents");

	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		// SDL_PollEvent may modify FPU flags
		streflop::streflop_init<streflop::Simple>();
#ifdef __APPLE__
		MacKeyboard::TranslateEvent(event);
#endif
#ifdef MAC_EGL_BRIDGE_AVAILABLE
		ScaleToPixels(event);
#endif
		PushEvent(event);
	}
}

InputHandler::HandlerTokenT InputHandler::AddHandler(InputHandler::HandlerFuncT func)
{
	for (size_t i = 0; i < eventHandlers.size(); ++i) {
		if (eventHandlers[i] == nullptr) {
			eventHandlers[i] = func;
			return InputHandler::HandlerTokenT{ *this, i};
		}
	}
	eventHandlers.emplace_back(func);
	return InputHandler::HandlerTokenT{ *this, eventHandlers.size() - 1 };
}


