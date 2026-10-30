/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

#include "MacMetalPresenter.h"

#ifdef MAC_EGL_BRIDGE_AVAILABLE

#include <objc/runtime.h>
#include <objc/message.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <SDL.h>
#include <SDL_metal.h>

#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"
#include "System/Threading/ThreadPool.h"

#if defined(__x86_64__)
	// Intel Macs: SDL's renderer cost about 10 ms a Retina frame (supcom-mac-intel measurements)
	#define MAC_METAL_PRESENT_DEFAULT true
#else
	// Apple Silicon keeps SDL's renderer until this path is measured there
	#define MAC_METAL_PRESENT_DEFAULT false
#endif

CONFIG(bool, MacMetalPresent).defaultValue(MAC_METAL_PRESENT_DEFAULT).description("macOS: present frames through a CAMetalLayer with persistent upload buffers instead of SDL's Metal renderer.");

extern "C" {
	void* MTLCreateSystemDefaultDevice(void);
	void* objc_autoreleasePoolPush(void);
	void objc_autoreleasePoolPop(void*);
}

namespace {
	using Obj = void*;

	struct MTLSize { unsigned long w, h, d; };
	struct MTLOrigin { unsigned long x, y, z; };
	struct CGSize { double w, h; };

	constexpr unsigned long MTLPixelFormatBGRA8Unorm = 80;
	constexpr unsigned long MTLResourceStorageModeShared = 0;
	constexpr unsigned int NUM_BUFFERS = 3;

	SEL Sel(const char* name) { return sel_registerName(name); }

	Obj Send(Obj obj, const char* sel) { return reinterpret_cast<Obj (*)(Obj, SEL)>(objc_msgSend)(obj, Sel(sel)); }
	void SendBool(Obj obj, const char* sel, bool v) { reinterpret_cast<void (*)(Obj, SEL, signed char)>(objc_msgSend)(obj, Sel(sel), v ? 1 : 0); }
	void SendObj(Obj obj, const char* sel, Obj v) { reinterpret_cast<void (*)(Obj, SEL, Obj)>(objc_msgSend)(obj, Sel(sel), v); }
	void SendUL(Obj obj, const char* sel, unsigned long v) { reinterpret_cast<void (*)(Obj, SEL, unsigned long)>(objc_msgSend)(obj, Sel(sel), v); }

	SDL_MetalView view = nullptr;
	Obj layer = nullptr;
	Obj device = nullptr;
	Obj queue = nullptr;

	Obj buffers[NUM_BUFFERS] = {};
	Obj inFlight[NUM_BUFFERS] = {};     // the retained command buffer that last read each upload buffer
	size_t bufferSize = 0;
	unsigned int next = 0;
	int drawW = 0;
	int drawH = 0;

	void ReleaseBuffers() {
		for (unsigned int i = 0; i < NUM_BUFFERS; i++) {
			if (inFlight[i] != nullptr) {
				Send(inFlight[i], "waitUntilCompleted");
				Send(inFlight[i], "release");
				inFlight[i] = nullptr;
			}
			if (buffers[i] != nullptr)
				Send(buffers[i], "release");
			buffers[i] = nullptr;
		}
		bufferSize = 0;
	}

	bool EnsureBuffers(size_t size) {
		if (size == bufferSize)
			return true;

		ReleaseBuffers();
		for (unsigned int i = 0; i < NUM_BUFFERS; i++) {
			buffers[i] = reinterpret_cast<Obj (*)(Obj, SEL, unsigned long, unsigned long)>(objc_msgSend)(device, Sel("newBufferWithLength:options:"), size, MTLResourceStorageModeShared);
			if (buffers[i] == nullptr)
				return false;
		}
		bufferSize = size;
		return true;
	}
}


bool MacMetalPresenter::Active() { return (layer != nullptr); }

bool MacMetalPresenter::Init(SDL_Window* window)
{
	if (layer != nullptr)
		return true;
	if (!configHandler->GetBool("MacMetalPresent"))
		return false;

	if ((device = MTLCreateSystemDefaultDevice()) == nullptr) {
		LOG_L(L_WARNING, "[MacMetalPresenter] no Metal device");
		return false;
	}
	if ((view = SDL_Metal_CreateView(window)) == nullptr) {
		LOG_L(L_WARNING, "[MacMetalPresenter] SDL_Metal_CreateView failed: %s", SDL_GetError());
		Kill();
		return false;
	}

	layer = SDL_Metal_GetLayer(view);
	queue = Send(device, "newCommandQueue");
	if (layer == nullptr || queue == nullptr) {
		Kill();
		return false;
	}

	SendObj(layer, "setDevice:", device);
	SendUL(layer, "setPixelFormat:", MTLPixelFormatBGRA8Unorm);
	SendBool(layer, "setFramebufferOnly:", false);     // the drawable is a blit target
	LOG("[MacMetalPresenter] presenting through a CAMetalLayer (MacMetalPresent=1)");
	return true;
}

void MacMetalPresenter::Kill()
{
	if (device != nullptr)
		ReleaseBuffers();
	if (queue != nullptr)
		Send(queue, "release");
	if (view != nullptr)
		SDL_Metal_DestroyView(view);
	// the device came from MTLCreateSystemDefaultDevice (+1)
	if (device != nullptr)
		Send(device, "release");

	queue = nullptr;
	view = nullptr;
	layer = nullptr;
	device = nullptr;
	drawW = drawH = 0;
}

void MacMetalPresenter::SetVSync(bool on)
{
	if (layer != nullptr)
		SendBool(layer, "setDisplaySyncEnabled:", on);
}

bool MacMetalPresenter::Present(const void* bgra, int w, int h)
{
	if (layer == nullptr || w <= 0 || h <= 0)
		return false;

	const size_t rowBytes = size_t(w) * 4;
	if (!EnsureBuffers(rowBytes * h))
		return false;

	void* pool = objc_autoreleasePoolPush();

	if (w != drawW || h != drawH) {
		reinterpret_cast<void (*)(Obj, SEL, CGSize)>(objc_msgSend)(layer, Sel("setDrawableSize:"), CGSize{double(w), double(h)});
		drawW = w;
		drawH = h;
	}

	// the upload buffer is free once the GPU finished the blit that read it (three frames ago)
	const unsigned int cur = next;
	next = (next + 1) % NUM_BUFFERS;
	if (inFlight[cur] != nullptr) {
		Send(inFlight[cur], "waitUntilCompleted");
		Send(inFlight[cur], "release");
		inFlight[cur] = nullptr;
	}

	// copy flipped (GL rows run bottom up), in row bands on the worker threads; the buffer's pages stay mapped
	uint8_t* dst = static_cast<uint8_t*>(Send(buffers[cur], "contents"));
	const uint8_t* src = static_cast<const uint8_t*>(bgra);
	constexpr int BANDS = 8;
	const int rowsPerBand = (h + BANDS - 1) / BANDS;
	for_mt(0, BANDS, [&](const int b) {
		const int y1 = std::min(h, (b + 1) * rowsPerBand);
		for (int y = b * rowsPerBand; y < y1; ++y)
			memcpy(dst + size_t(h - 1 - y) * rowBytes, src + size_t(y) * rowBytes, rowBytes);
	});

	bool shown = false;
	Obj drawable = Send(layer, "nextDrawable");
	if (drawable != nullptr) {
		Obj texture = Send(drawable, "texture");
		// a resize reaches the drawable a frame late: blit only what fits
		const unsigned long tw = reinterpret_cast<unsigned long (*)(Obj, SEL)>(objc_msgSend)(texture, Sel("width"));
		const unsigned long th = reinterpret_cast<unsigned long (*)(Obj, SEL)>(objc_msgSend)(texture, Sel("height"));
		const unsigned long cw = std::min<unsigned long>(tw, w);
		const unsigned long ch = std::min<unsigned long>(th, h);
		Obj cmdBuf = Send(queue, "commandBuffer");
		Obj blit = Send(cmdBuf, "blitCommandEncoder");

		reinterpret_cast<void (*)(Obj, SEL, Obj, unsigned long, unsigned long, unsigned long, MTLSize, Obj, unsigned long, unsigned long, MTLOrigin)>(objc_msgSend)(
			blit, Sel("copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:"),
			buffers[cur], 0, rowBytes, rowBytes * h, MTLSize{cw, ch, 1}, texture, 0, 0, MTLOrigin{0, 0, 0});
		Send(blit, "endEncoding");
		SendObj(cmdBuf, "presentDrawable:", drawable);
		Send(cmdBuf, "commit");

		inFlight[cur] = Send(cmdBuf, "retain");
		shown = true;
	}

	objc_autoreleasePoolPop(pool);
	return shown;
}

#endif
