/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */
// KT1 SimHost core: display effects on their way from the sim to the draw side (SIM-DRAW-PARALLEL 13.4, 13.8).
// No engine include.
//
// The sim Emits during a step; Publish (the step's end) hands the step's events over; Replay gives them to the draw
// side in the order they were emitted and forgets them. Steps published before a replay are kept, in order. Nothing
// here is synced: no sim code may read the queue back.

#ifndef KT1_DISPLAY_QUEUE_H
#define KT1_DISPLAY_QUEUE_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace KT1::Core::SimHost {

enum class DisplayKind : uint8_t {
	Sound,      // id = sound id; a = position; value = volume
	Effect,     // id = explosion generator id; a = position; b = direction; value = damage; size = radius; unit = owner
	Log,        // text
	DebugLine,  // a, b = ends; size = width; value = lifetime in seconds
	Ping        // a = position; text = label; unit = player
};

constexpr size_t DISPLAY_TEXT = 120;   // longer text is cut: the event stays a plain value

struct DisplayEvent {
	DisplayKind kind = DisplayKind::Log;
	int32_t id = -1;
	int32_t unit = -1;
	float a[3] = {0.0f, 0.0f, 0.0f};
	float b[3] = {0.0f, 0.0f, 0.0f};
	float value = 0.0f;
	float size = 0.0f;
	char text[DISPLAY_TEXT] = {0};

	void SetText(const char* s) {
		size_t n = 0;
		if (s != nullptr)
			while (n < DISPLAY_TEXT - 1 && s[n] != 0) ++n;
		if (n > 0) std::memcpy(text, s, n);
		text[n] = 0;
	}
};

class DisplayQueue {
public:
	// KT1 hot
	void Emit(const DisplayEvent& e) { writing.push_back(e); }

	// Copies rather than swaps, so both lists keep their size and a warm queue does not allocate.
	void Publish() {
		ready.insert(ready.end(), writing.begin(), writing.end());
		writing.clear();
	}

	// `sink` may not Emit: replay is draw-side work.
	template<class F> void Replay(F&& sink) {
		for (const DisplayEvent& e : ready) sink(e);
		ready.clear();
	}

	size_t Pending() const { return ready.size(); }

	void Clear() { writing.clear(); ready.clear(); }

private:
	std::vector<DisplayEvent> writing;
	std::vector<DisplayEvent> ready;
};

} // namespace KT1::Core::SimHost

#endif
